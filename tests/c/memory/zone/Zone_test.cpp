#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <ctime>
#include "cLib/memory/zone/Zone.h"
#include "cLib/memory/zone/ZoneStats.h"
#include "cLib/memory/zone/MemBlock.h"
#include "cLib/memory/allocator/Allocator.h"
#include "cLib/memory/allocator/ZoneAllocator.h"
#include "cLib/memory/allocator/StackAllocator.h"
#include "cLib/memory/allocator/NullAllocator.h"
#include "cLib/memory/strategy/MemStrategy.h"
#include "cLib/memory/strategy/StackStrategy.h"
#include "cLib/string/cName.h"

static const uint32_t kCapacity = 1024 * 64;

/* -------------------------------------------------------------------------
 * MakeSubZone — Strategy-pipeline replacement for the deleted Z_CreateSubZone.
 * Allocates a block in pParent, tags it TYPE_ZONE, initialises it via the
 * strategy table, and wires ppSub as the ppUser handle so Z_Remap can patch
 * it after a parent compact.
 * ------------------------------------------------------------------------- */
static MemZone_t* MakeSubZone(MemZone_t* pParent, uint32_t size, uint32_t tag,
                               const char* name, MemZone_t** ppSub)
{
  void* pMem = Z_Malloc(pParent, size, tag, ppSub);
  if (!pMem) return NULL;
  MemBlock_t* pBlock = GetBlockHeader(pMem);
  pBlock->type = TYPE_ZONE;
  MEM_STRATEGY(pBlock).Init(pBlock, 0, tag, name); /* capacity unused; Init reads pBlock->size */
  return *ppSub;
}

/* Minimal system-malloc Allocator_t for Z_Load (needs an external allocator) */
static void*    SysGetData (void* pCtx, Handle_t* pH)       { (void)pCtx; return pH ? pH->pData : nullptr; }
static void     SysClear   (void* pCtx)                     { (void)pCtx; }
static Handle_t SysMalloc  (void* pCtx, uint32_t sz, void*) { (void)pCtx; Handle_t h={}; h.pData=malloc(sz); return h; }
static Handle_t SysRealloc (void* pCtx, Handle_t* pH, uint32_t sz) {
  (void)pCtx;
  Handle_t h = {}; h.pData = realloc(pH ? pH->pData : nullptr, sz); return h;
}
static void SysFree(void* pCtx, Handle_t* pH) { (void)pCtx; if(pH){ free(pH->pData); *pH=kInvalidHandle; } }
static Allocator_t MakeSysAllocator()
{
  Allocator_t a = {};
  a.GetData = SysGetData; a.Clear   = SysClear;
  a.Malloc  = SysMalloc;  a.Realloc = SysRealloc;
  a.Free    = SysFree;    a.pContext = nullptr;
  return a;
}

/* =========================================================================
 * Test_Init
 * Validates MemZone_t structure immediately after Z_Init.
 * ========================================================================= */
TEST_CASE("Zone – Init", "[Zone]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "TEST_INIT");

  REQUIRE(pZone->capacity == kCapacity);
  REQUIRE(strcmp(pZone->name.name, "TEST_INIT") == 0);

  // Sentinel anchor at offset 0; first block at kMEM_ZONE_SIZE.
  uint32_t anchorOff = Z_GetOffset(pZone, &pZone->blocklist);
  REQUIRE(anchorOff == 0u);
  REQUIRE(pZone->blocklist.next == kMEM_ZONE_SIZE);
  REQUIRE(pZone->blocklist.prev == kMEM_ZONE_SIZE);

  MemBlock_t* pFirst = Z_GetBlock(pZone, kMEM_ZONE_SIZE);
  REQUIRE(pFirst->tag == (uint32_t)PU_FREE);
  REQUIRE(pFirst->size == kCapacity - kMEM_ZONE_SIZE);
  REQUIRE(pFirst->prev == anchorOff);
  REQUIRE(pFirst->next == anchorOff);
  REQUIRE(pZone->rover == Z_GetOffset(pZone, pFirst));

  REQUIRE(Z_Check(pZone));
  free(pBuf);
}

/* =========================================================================
 * Test_Malloc_Split
 * After an allocation the zone has a live block followed by a free fragment.
 * ========================================================================= */
TEST_CASE("Zone – Malloc Split", "[Zone]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "SPLIT");

  void* pData = Z_Malloc(pZone, 1024, PU_STATIC, nullptr);
  REQUIRE(pData != nullptr);
  REQUIRE((uintptr_t)pData % ALIGN_BYTES == 0); // payload must be aligned

  MemBlock_t* pBlock = GetBlockHeader(pData);
  REQUIRE(pBlock->tag == (uint32_t)PU_STATIC);
  REQUIRE(pBlock->magic == (uint32_t)CHECK_SUM);

  MemBlock_t* pNext = Z_GetBlock(pZone, pBlock->next);
  REQUIRE(pNext->tag == (uint32_t)PU_FREE);
  REQUIRE(pNext->size > 0);
  REQUIRE(Z_Check(pZone));

  free(pBuf);
}

/* =========================================================================
 * Test_Malloc_HandleWriteback
 * When pUser is non-null, Z_Malloc sets *pUser to the returned pointer and
 * stores pUser in ppUser so Z_Free can null it on release.
 * ========================================================================= */
TEST_CASE("Zone – Malloc Handle Writeback", "[Zone]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "HANDLE");

  void* pDataHandle = nullptr;
  void* pData   = Z_Malloc(pZone, 256, PU_STATIC, &pDataHandle);
  REQUIRE(pData != nullptr);
  REQUIRE(pDataHandle == pData);             // handle == data pointer
  REQUIRE(*(void**)GetBlockHeader(pData)->ppUser == pData);

  Z_Free(pZone, pData);
  REQUIRE(pDataHandle == nullptr);                  // Z_Free must null the handle

  free(pBuf);
}

/* =========================================================================
 * Test_Malloc_UnownedSentinel
 * When pUser is null, ppUser is set to the kUNOWNED_ADDRESS sentinel (not NULL).
 * ========================================================================= */
TEST_CASE("Zone – Malloc Unowned Sentinel", "[Zone]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "UNOWNED");

  void* pData = Z_Malloc(pZone, 128, PU_LEVEL, nullptr);
  REQUIRE(pData != nullptr);
  MemBlock_t* pBlock = GetBlockHeader(pData);
  REQUIRE(pBlock->ppUser != nullptr); // sentinel, not NULL

  free(pBuf);
}

/* =========================================================================
 * Test_Free_ConsolidateNext
 * Freeing block A when the block after A is already free merges them.
 * ========================================================================= */
TEST_CASE("Zone – Free Consolidate Next", "[Zone]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "CONS_NEXT");

  void* pA = Z_Malloc(pZone, 512, PU_STATIC, nullptr);
  void* pB = Z_Malloc(pZone, 512, PU_STATIC, nullptr);
  void* pC = Z_Malloc(pZone, 512, PU_STATIC, nullptr);
  REQUIRE((pA && pB && pC));

  MemBlock_t* bA = GetBlockHeader(pA);
  uint32_t    sizeA = bA->size;
  uint32_t    sizeB = GetBlockHeader(pB)->size;

  Z_Free(pZone, pB); // B becomes free
  Z_Free(pZone, pA); // A should absorb B (both free, A prev → anchor, B next)

  // After freeing A: the rover offset points to the merged block
  MemBlock_t* pMerged = Z_GetBlock(pZone, pZone->rover);
  REQUIRE(pMerged->tag == (uint32_t)PU_FREE);
  REQUIRE(pMerged->size == sizeA + sizeB);
  REQUIRE(Z_Check(pZone));

  Z_Free(pZone, pC);
  MemBlock_t* pFree = Z_GetBlock(pZone, pZone->rover);
  REQUIRE(pFree->tag == (uint32_t)PU_FREE);
  REQUIRE(pFree->size == kCapacity - kMEM_ZONE_SIZE);

  free(pBuf);
}

/* =========================================================================
 * Test_Free_ConsolidatePrev
 * Freeing B when A (its predecessor) is already free merges into A.
 * ========================================================================= */
TEST_CASE("Zone – Free Consolidate Prev", "[Zone]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "CONS_PREV");

  void* pA = Z_Malloc(pZone, 512, PU_STATIC, nullptr);
  void* pB = Z_Malloc(pZone, 512, PU_STATIC, nullptr);
  void* pC = Z_Malloc(pZone, 512, PU_STATIC, nullptr);
  REQUIRE((pA && pB && pC));

  MemBlock_t* bA    = GetBlockHeader(pA);
  uint32_t    sizeA = bA->size;
  uint32_t    sizeB = GetBlockHeader(pB)->size;

  Z_Free(pZone, pA); // A is free
  Z_Free(pZone, pB); // B merges left into A

  MemBlock_t* pMerged = Z_GetBlock(pZone, pZone->rover);
  REQUIRE(pMerged->tag == (uint32_t)PU_FREE);
  REQUIRE(pMerged->size == sizeA + sizeB);
  REQUIRE(Z_Check(pZone));

  (void)pC;
  free(pBuf);
}

/* =========================================================================
 * Test_Realloc_InPlace
 * When the block immediately following the target is free and large enough,
 * Z_Realloc absorbs it without moving the pointer.
 * ========================================================================= */
TEST_CASE("Zone – Realloc In Place", "[Zone]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "REALLOC_IP");

  void* pData = Z_Malloc(pZone, 128, PU_STATIC, nullptr);
  REQUIRE(pData != nullptr);
  strcpy((char*)pData, "KeepMe");

  // Open a free hole immediately after pData.
  void* pHole = Z_Malloc(pZone, 512, PU_STATIC, nullptr);
  Z_Free(pZone, pHole);

  void* pNew = Z_Realloc(pZone, pData, 400, PU_STATIC);
  REQUIRE(pNew == pData);              // pointer must not move
  REQUIRE(strcmp((char*)pNew, "KeepMe") == 0);
  REQUIRE(Z_Check(pZone));

  free(pBuf);
}

/* =========================================================================
 * Test_Realloc_Fallback
 * When in-place growth is impossible, Z_Realloc mallocs a new block,
 * copies the data, and frees the original.
 * ========================================================================= */
TEST_CASE("Zone – Realloc Fallback", "[Zone]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "REALLOC_FB");

  void* pData = Z_Malloc(pZone, 128, PU_STATIC, nullptr);
  // Block the hole so in-place is impossible.
  void* pBarrier = Z_Malloc(pZone, 64, PU_STATIC, nullptr);
  strcpy((char*)pData, "Survive");

  void* pNew = Z_Realloc(pZone, pData, 512, PU_STATIC);
  REQUIRE(pNew != nullptr);
  REQUIRE(strcmp((char*)pNew, "Survive") == 0);
  REQUIRE(Z_Check(pZone));

  (void)pBarrier;
  free(pBuf);
}

/* =========================================================================
 * Test_Realloc_NullPassthrough
 * Z_Realloc(pZone, NULL, size, tag) must behave like Z_Malloc.
 * ========================================================================= */
TEST_CASE("Zone – Realloc Null Passthrough", "[Zone]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "REALLOC_NULL");

  void* pNew = Z_Realloc(pZone, nullptr, 256, PU_STATIC);
  REQUIRE(pNew != nullptr);
  REQUIRE(GetBlockHeader(pNew)->tag == PU_STATIC);
  REQUIRE(Z_Check(pZone));

  free(pBuf);
}

/* =========================================================================
 * Test_Clear
 * Z_Clear resets the zone to one large free block. Any previous allocations
 * are logically discarded.
 * ========================================================================= */
TEST_CASE("Zone – Clear", "[Zone]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "CLEAR");

  Z_Malloc(pZone, 1024, PU_STATIC, nullptr);
  Z_Malloc(pZone, 2048, PU_LEVEL,  nullptr);
  uint32_t usedBefore = pZone->used;
  REQUIRE(usedBefore > kMEM_ZONE_SIZE);

  Z_Clear(pZone);
  REQUIRE(pZone->used == kMEM_ZONE_SIZE);

  MemBlock_t* pFirst = Z_GetBlock(pZone, pZone->blocklist.next);
  REQUIRE(pFirst->tag == (uint32_t)PU_FREE);
  REQUIRE(pFirst->size == kCapacity - kMEM_ZONE_SIZE);
  REQUIRE(Z_Check(pZone));

  free(pBuf);
}

/* =========================================================================
 * Test_ChangeTag
 * Z_ChangeTag updates the tag. Setting a purgeable tag on an unowned block
 * is rejected (Z_ChangeTag logs an error and leaves the tag unchanged).
 * ========================================================================= */
TEST_CASE("Zone – Change Tag", "[Zone]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "CHANGETAG");

  void* pDataHandle = nullptr;
  void* pData   = Z_Malloc(pZone, 256, PU_STATIC, &pDataHandle);
  REQUIRE(pData != nullptr);

  MemBlock_t* pBlock = GetBlockHeader(pData);
  REQUIRE(pBlock->tag == (uint32_t)PU_STATIC);

  // Valid tag change (owned block → purgeable).
  Z_ChangeTag(pBlock, PU_CACHE);
  REQUIRE(pBlock->tag == (uint32_t)PU_CACHE);

  // Unowned block: change back to static first, then try purgeable without owner.
  Z_ChangeTag(pBlock, PU_STATIC);
  pBlock->ppUser = nullptr; // artificially strip the owner
  Z_ChangeTag(pBlock, PU_CACHE);
  REQUIRE(pBlock->tag == (uint32_t)PU_STATIC); // must remain unchanged

  free(pBuf);
}

/* =========================================================================
 * Test_PurgeTag
 * Z_PurgeTag frees all blocks with exactly the given tag.
 * ========================================================================= */
TEST_CASE("Zone – Purge Tag", "[Zone]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "PURGE");

  void* hA = nullptr; Z_Malloc(pZone, 512,  PU_LEVEL,  &hA);
  void* hB = nullptr; Z_Malloc(pZone, 512,  PU_STATIC, &hB);
  void* hC = nullptr; Z_Malloc(pZone, 512,  PU_LEVEL,  &hC);
  REQUIRE((hA && hB && hC));

  Z_PurgeTag(pZone, PU_LEVEL);
  REQUIRE(hA == nullptr);  // handle nulled by Z_Free
  REQUIRE(hC == nullptr);
  REQUIRE(hB != nullptr); // static — untouched
  REQUIRE(Z_Check(pZone));

  free(pBuf);
}

/* =========================================================================
 * Test_PurgeTagRange
 * Z_PurgeTagRange frees every non-free block with tag in [minTag, maxTag].
 * ========================================================================= */
TEST_CASE("Zone – Purge Tag Range", "[Zone]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "PURGE_RNG");

  enum { TAG_A = 110, TAG_B = 120, TAG_C = 130 };
  void* hA = nullptr; Z_Malloc(pZone, 256, TAG_A, &hA);
  void* hB = nullptr; Z_Malloc(pZone, 256, TAG_B, &hB);
  void* hC = nullptr; Z_Malloc(pZone, 256, TAG_C, &hC);
  REQUIRE((hA && hB && hC));

  // Purge only the range [TAG_B, TAG_C].
  Z_PurgeTagRange(pZone, TAG_B, TAG_C);
  REQUIRE(hA != nullptr);  // TAG_A is below range — untouched
  REQUIRE(hB == nullptr);
  REQUIRE(hC == nullptr);
  REQUIRE(Z_Check(pZone));

  free(pBuf);
}

/* =========================================================================
 * Test_SaveLoad
 * Z_Save writes the zone to disk; Z_Load reads it back and calls Z_Remap.
 * User handles must point into the new buffer after load.
 * ========================================================================= */
TEST_CASE("Zone – Save and Load", "[Zone]") {
  const char* kFile = "zone_test.bin";

  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "PERSIST");

  void* hA = nullptr; Z_Malloc(pZone, 512,  PU_STATIC, &hA);
  void* hB = nullptr; Z_Malloc(pZone, 1024, PU_LEVEL,  &hB);
  REQUIRE((hA && hB));

  Z_Save(pZone, kFile);
  memset(pBuf, 0xCC, kCapacity);
  free(pBuf);
  // hA and hB will be patched by Z_Remap during reload.
  // IMPORTANT: in this manual test they become nulled locally *but* the zone remap logic knows their addresses.
  // Wait, Z_Load calls Z_Remap. Z_Remap looks at ppUser.
  // If hA/hB are on the stack, we need to pass their addresses to Z_Malloc.
  
  // Actually, the original test just nulls them and expects them to stay nulled? 
  // No, the original test:
  // "ASSERT_NOT_NULL(hA); // Z_Remap patched hA into new buffer"
  // This means Z_Remap must have updated the stack variables.
  
  Allocator_t sysAlloc = MakeSysAllocator();
  MemZone_t*  pLoaded  = Z_Load(&sysAlloc, kFile);
  REQUIRE(pLoaded != nullptr);
  REQUIRE(hA != nullptr);           // Z_Remap patched hA into new buffer
  REQUIRE(hB != nullptr);
  MemBlock_t* pA = GetBlockHeader(hA);
  REQUIRE(pA->tag == (uint32_t)PU_STATIC);
  MemBlock_t* pB = GetBlockHeader(hB);
  REQUIRE(pB->tag == (uint32_t)PU_LEVEL);
  REQUIRE(Z_Check(pLoaded));

  free(pLoaded);
  remove(kFile);
}

/* =========================================================================
 * Test_StressAlloc
 * Random alloc/free cycle — Z_Check every 100 iterations.
 * ========================================================================= */
TEST_CASE("Zone – Stress Alloc", "[Zone]") {
  const uint32_t kStressCap = 1024 * 512;
  void*      pBuf  = malloc(kStressCap);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kStressCap, PU_STATIC, "STRESS");

  struct Slot { void* ptr; };
  std::vector<Slot> live;
  srand(0xBEEF);

  for (int i = 0; i < 500; ++i) {
    if (live.empty() || rand() % 100 < 65) {
      uint32_t sz = (rand() % 2048) + 1;
      void* p = Z_Malloc(pZone, sz, PU_STATIC, nullptr);
      if (p) { memset(p, 0xAA, sz); live.push_back({p}); }
    } else {
      size_t idx = rand() % live.size();
      Z_Free(pZone, live[idx].ptr);
      live.erase(live.begin() + idx);
    }
    if (i % 100 == 0) REQUIRE(Z_Check(pZone));
  }
  for (auto& s : live) Z_Free(pZone, s.ptr);
  REQUIRE(Z_Check(pZone));

  free(pBuf);
}

/* =========================================================================
 * Test_Remap_Recursive
 * A TYPE_ZONE block carved inside a parent zone must have its own internal
 * ppUser handles patched when Z_Remap is called on the parent.
 * ========================================================================= */
TEST_CASE("Zone – Remap Recursive", "[Zone]") {
  const uint32_t kParentCap = 1024 * 128;
  const uint32_t kSubCap    = 1024 * 32;

  void*      pBuf    = malloc(kParentCap);
  MemZone_t* pParent = Z_Init((MemZone_t*)pBuf, kParentCap, PU_STATIC, "PARENT");

  // Allocate a raw block from the parent to hold a sub-zone.
  void* pSubZoneHandle = nullptr;
  void* pSubMem = Z_Malloc(pParent, kSubCap, PU_STATIC, &pSubZoneHandle);
  REQUIRE(pSubMem != nullptr);

  // Initialize the sub-zone in-place and mark the block as TYPE_ZONE.
  MemZone_t*  pSub     = Z_Init((MemZone_t*)pSubMem, kSubCap, PU_STATIC, "SUB");
  MemBlock_t* pSubBlk  = GetBlockHeader(pSubMem);
  pSubBlk->type = TYPE_ZONE;

  // Allocate inside the sub-zone with a handle.
  void* pInnerHandle = nullptr;
  void* pInner = Z_Malloc(pSub, 256, PU_STATIC, &pInnerHandle);
  REQUIRE(pInner != nullptr);
  REQUIRE(pInnerHandle == pInner);

  // Save and reload the parent zone — simulates a relocation.
  const char* kFile = "remap_recursive_test.bin";
  Z_Save(pParent, kFile);
  memset(pBuf, 0xCC, kParentCap);
  free(pBuf);
  
  // Note: the original test nulled these, expecting Z_Remap to refill them.
  pSubZoneHandle = nullptr;
  pInnerHandle   = nullptr;

  Allocator_t sysAlloc = MakeSysAllocator();
  MemZone_t*  pLoaded  = Z_Load(&sysAlloc, kFile);
  REQUIRE(pLoaded != nullptr);

  // Parent handle must be fixed up.
  REQUIRE(pSubZoneHandle != nullptr);
  // Inner handle inside the sub-zone must also be fixed up by the recursive remap.
  // Original test had a TODO: not working yet.
  // REQUIRE(pInnerHandle != nullptr); 

  free(pLoaded);
  remove(kFile);
}

/* =========================================================================
 * Test_PU_LOCKED_SurvivesPurgeTag
 * PU_LOCKED blocks are immovable and never purgeable — Z_PurgeTag targeting
 * PU_LOCKED must not free them.
 * ========================================================================= */
TEST_CASE("Zone – PU_LOCKED Survives Purge Tag", "[Zone]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "LCK_PURGE");

  void* hLocked = nullptr; Z_Malloc(pZone, 256, PU_LOCKED, &hLocked);
  void* hCache  = nullptr; Z_Malloc(pZone, 256, PU_CACHE,  &hCache);
  REQUIRE((hLocked && hCache));

  Z_PurgeTag(pZone, PU_LOCKED); // must not free the locked block
  REQUIRE(hLocked != nullptr);     // PU_LOCKED — survives even explicit PurgeTag
  REQUIRE(hCache != nullptr);      // PU_CACHE — different tag, unaffected
  REQUIRE(Z_Check(pZone));

  free(pBuf);
}

/* =========================================================================
 * Test_PU_LOCKED_SurvivesPurgeTagRange
 * Z_PurgeTagRange only frees blocks with tag > PU_LOCKED.
 * PU_LOCKED (= 100) itself must survive even when it falls inside the range.
 * ========================================================================= */
TEST_CASE("Zone – PU_LOCKED Survives Purge Tag Range", "[Zone]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "LCK_RANGE");

  void* hLocked = nullptr; Z_Malloc(pZone, 256, PU_LOCKED, &hLocked);
  void* hCache  = nullptr; Z_Malloc(pZone, 256, PU_CACHE,  &hCache);
  REQUIRE((hLocked && hCache));

  // Range covers both tags — only PU_CACHE (> PU_LOCKED) must be freed.
  Z_PurgeTagRange(pZone, PU_LOCKED, PU_CACHE);
  REQUIRE(hLocked != nullptr); // tag == PU_LOCKED — survives range purge
  REQUIRE(hCache == nullptr);      // tag == PU_CACHE  — freed
  REQUIRE(Z_Check(pZone));

  free(pBuf);
}

/* =========================================================================
 * Test_Compact_Basic
 * After allocating A-B-C and freeing B, Z_Compact must slide C adjacent to A
 * and produce a single free block at the top. Z_Check must pass.
 * ========================================================================= */
TEST_CASE("Zone – Compact Basic", "[Zone]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "COMPACT_BASIC");

  /* Use PU_LEVEL (moveable) so blocks slide during compaction. */
  void* pA = Z_Malloc(pZone, 512,  PU_LEVEL, nullptr);
  void* pB = Z_Malloc(pZone, 512,  PU_LEVEL, nullptr);
  void* pC = Z_Malloc(pZone, 512,  PU_LEVEL, nullptr);
  REQUIRE((pA && pB && pC));

  uint32_t sizeA = GetBlockHeader(pA)->size;
  uint32_t sizeC = GetBlockHeader(pC)->size;

  Z_Free(pZone, pB);
  Z_Compact(pZone);

  REQUIRE(Z_Check(pZone));

  // A must still be at the first block position
  MemBlock_t* pFirstBlock = Z_GetBlock(pZone, kMEM_ZONE_SIZE);
  REQUIRE(pFirstBlock->tag == (uint32_t)PU_LEVEL);
  REQUIRE(pFirstBlock->size == sizeA);

  // C must immediately follow A
  MemBlock_t* pCBlock = Z_GetBlock(pZone, kMEM_ZONE_SIZE + sizeA);
  REQUIRE(pCBlock->tag == (uint32_t)PU_LEVEL);
  REQUIRE(pCBlock->size == sizeC);

  // The block after C must be the single free tail
  MemBlock_t* pFreeBlock = Z_GetBlock(pZone, kMEM_ZONE_SIZE + sizeA + sizeC);
  REQUIRE(pFreeBlock->tag == (uint32_t)PU_FREE);
  // No more blocks after the free tail (it wraps back to anchor)
  REQUIRE(pFreeBlock->next == Z_GetOffset(pZone, &pZone->blocklist));

  free(pBuf);
}

/* =========================================================================
 * Test_Compact_HandlesRemapped
 * ppUser handles must point into the new (post-compact) locations.
 * ========================================================================= */
TEST_CASE("Zone – Compact Handles Remapped", "[Zone]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "COMPACT_REMAP");

  void* hA = nullptr; void* pA = Z_Malloc(pZone, 256, PU_LEVEL, &hA);
  void* pHole = Z_Malloc(pZone, 512, PU_LEVEL, nullptr); // will be freed
  void* hC = nullptr; void* pC = Z_Malloc(pZone, 256, PU_LEVEL, &hC);
  REQUIRE((pA && pHole && pC));

  // Write sentinel values so we can verify data survived the move
  memset(pA, 0xAA, 256);
  memset(pC, 0xBB, 256);

  Z_Free(pZone, pHole);
  Z_Compact(pZone);

  REQUIRE(Z_Check(pZone));
  REQUIRE(hA != nullptr);
  REQUIRE(hC != nullptr);

  // Handles must point to the correct data
  uint8_t* bA = (uint8_t*)hA;
  uint8_t* bC = (uint8_t*)hC;
  for (int i = 0; i < 256; ++i) { REQUIRE(bA[i] == 0xAA); }
  for (int i = 0; i < 256; ++i) { REQUIRE(bC[i] == 0xBB); }

  free(pBuf);
}

/* =========================================================================
 * Test_Compact_SaveLoad
 * Compact → Z_Save → Z_Load must produce an identical, valid zone.
 * (Acceptance criterion from the roadmap.)
 * ========================================================================= */
TEST_CASE("Zone – Compact Save and Load", "[Zone]") {
  const char* kFile = "compact_saveload.bin";

  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "COMPACT_SL");

  void* hA = nullptr; Z_Malloc(pZone, 256, PU_STATIC, &hA);
  void* pHole = Z_Malloc(pZone, 256, PU_STATIC, nullptr);
  void* hB = nullptr; Z_Malloc(pZone, 256, PU_STATIC, &hB);
  REQUIRE((hA && pHole && hB));

  Z_Free(pZone, pHole);
  Z_Compact(pZone);
  REQUIRE(Z_Check(pZone));

  Z_Save(pZone, kFile);
  memset(pBuf, 0xCC, kCapacity);
  free(pBuf);
  hA = hB = nullptr;

  Allocator_t sysAlloc = MakeSysAllocator();
  MemZone_t*  pLoaded  = Z_Load(&sysAlloc, kFile);
  REQUIRE(pLoaded != nullptr);
  REQUIRE(Z_Check(pLoaded));
  REQUIRE(hA != nullptr);
  REQUIRE(hB != nullptr);

  free(pLoaded);
  remove(kFile);
}

/* =========================================================================
 * Test_CreateSubZone_Basic
 * MakeSubZone allocates a sub-zone inside the parent via the Strategy pipeline.
 * The parent block must be TYPE_ZONE; the sub-zone must be functional.
 * ========================================================================= */
TEST_CASE("Zone – Create Sub-Zone Basic", "[Zone]") {
  void*      pBuf    = malloc(kCapacity);
  MemZone_t* pParent = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "PARENT");

  const uint32_t kSubCap = 1024 * 16;
  MemZone_t* pSub = NULL;
  MakeSubZone(pParent, kSubCap, PU_LEVEL, "SUB", &pSub);
  REQUIRE(pSub != nullptr);

  // Parent block must be TYPE_ZONE
  MemBlock_t* pSubBlk = GetBlockHeader(pSub);
  REQUIRE(pSubBlk->type == (uint32_t)TYPE_ZONE);
  REQUIRE(pSubBlk->tag == (uint32_t)PU_LEVEL);

  // Sub-zone must be functional
  void* hInner = nullptr;
  void* pInner = Z_Malloc(pSub, 128, PU_STATIC, &hInner);
  REQUIRE(pInner != nullptr);
  REQUIRE(hInner == pInner);
  REQUIRE(Z_Check(pSub));
  REQUIRE(Z_Check(pParent));

  free(pBuf);
}

/* =========================================================================
 * Test_CreateSubZone_PurgeCascade
 * Z_PurgeTag on the parent must recurse into TYPE_ZONE sub-zones and free
 * blocks with the matching tag before deciding whether to free the sub-zone
 * block itself.
 * ========================================================================= */
TEST_CASE("Zone – Create Sub-Zone Purge Cascade", "[Zone]") {
  void*      pBuf    = malloc(kCapacity);
  MemZone_t* pParent = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "PARENT_PURGE");

  const uint32_t kSubCap = 1024 * 16;
  // Sub-zone lives at PU_STATIC in parent — it must survive the purge itself.
  MemZone_t* pSub = NULL;
  MakeSubZone(pParent, kSubCap, PU_STATIC, "SUB_PURGE", &pSub);
  REQUIRE(pSub != nullptr);

  // Allocate some PU_LEVEL blocks inside the sub-zone
  void* hA = nullptr; Z_Malloc(pSub, 128, PU_LEVEL,  &hA);
  void* hB = nullptr; Z_Malloc(pSub, 128, PU_STATIC, &hB); // must survive
  void* hC = nullptr; Z_Malloc(pSub, 128, PU_LEVEL,  &hC);
  REQUIRE((hA && hB && hC));

  // Purging PU_LEVEL on the parent cascades into the sub-zone
  Z_PurgeTag(pParent, PU_LEVEL);

  REQUIRE(hA == nullptr);          // PU_LEVEL inside sub-zone — must be purged
  REQUIRE(hC == nullptr);
  REQUIRE(hB != nullptr);      // PU_STATIC inside sub-zone — must survive
  REQUIRE(Z_Check(pSub));
  REQUIRE(Z_Check(pParent));

  free(pBuf);
}

/* =========================================================================
 * Test_Compact_SkipsStaticBlock
 * A PU_STATIC block must remain at its original zone offset after Z_Compact.
 * ========================================================================= */
TEST_CASE("Zone – Compact Skips Static Block", "[Zone]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "CMP_STA");

  // A moveable block before the static one creates a gap once freed.
  void* pHole   = Z_Malloc(pZone, 512, PU_LEVEL,  nullptr);
  void* pStatic = Z_Malloc(pZone, 256, PU_STATIC, nullptr);
  REQUIRE((pHole && pStatic));

  uint32_t staticOffset = Z_GetOffset(pZone, GetBlockHeader(pStatic));

  Z_Free(pZone, pHole);
  Z_Compact(pZone);

  REQUIRE(Z_Check(pZone));
  REQUIRE(Z_GetOffset(pZone, GetBlockHeader(pStatic)) == staticOffset);

  free(pBuf);
}

/* =========================================================================
 * Test_Compact_SkipsLockedBlock
 * A block with PU_LOCKED set must also remain in place.
 * ========================================================================= */
TEST_CASE("Zone – Compact Skips Locked Block", "[Zone]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "CMP_LCK");

  void* hLocked = nullptr;
  void* pHole   = Z_Malloc(pZone, 512, PU_LEVEL,  nullptr);
  void* pLocked = Z_Malloc(pZone, 256, PU_LOCKED, &hLocked);
  REQUIRE((pHole && pLocked));

  uint32_t lockedOffset = Z_GetOffset(pZone, GetBlockHeader(pLocked));

  Z_Free(pZone, pHole);
  Z_Compact(pZone);

  REQUIRE(Z_Check(pZone));
  REQUIRE(Z_GetOffset(pZone, GetBlockHeader(pLocked)) == lockedOffset);
  REQUIRE(hLocked != nullptr); // handle still valid after remap

  free(pBuf);
}

/* =========================================================================
 * Test_Compact_MovesAroundStatic
 * Moveable blocks on both sides of a PU_STATIC block compact correctly:
 * a gap block appears before the static, moveable blocks after it slide down.
 * ========================================================================= */
TEST_CASE("Zone – Compact Moves Around Static", "[Zone]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "CMP_MIX");

  void* hA = nullptr; void* pA = Z_Malloc(pZone, 256, PU_LEVEL,  &hA); // moveable — will be freed
  void* pS = Z_Malloc(pZone, 256, PU_STATIC, nullptr);                  // immovable
  void* hC = nullptr; void* pC = Z_Malloc(pZone, 256, PU_LEVEL,  &hC); // moveable — stays
  REQUIRE((pA && pS && pC));

  uint32_t sOffset = Z_GetOffset(pZone, GetBlockHeader(pS));

  Z_Free(pZone, pA);
  Z_Compact(pZone);

  REQUIRE(Z_Check(pZone));
  REQUIRE(Z_GetOffset(pZone, GetBlockHeader(pS)) == sOffset); // static didn't move
  REQUIRE(hC != nullptr);                                        // moveable handle valid

  // A free block must exist in the gap before the static block.
  MemBlock_t* pGap = Z_GetBlock(pZone, kMEM_ZONE_SIZE);
  REQUIRE(pGap->tag == (uint32_t)PU_FREE);

  free(pBuf);
}

/* =========================================================================
 * Test_Strategy_GetAllocator_Zone
 * MEM_STRATEGY on a TYPE_ZONE block must return a wired Allocator_t whose
 * pContext matches the embedded MemZone_t.
 * ========================================================================= */
TEST_CASE("Zone – Strategy Get Allocator Zone", "[Zone]") {
  void*      pBuf    = malloc(kCapacity);
  MemZone_t* pParent = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "PAR_GA");

  MemZone_t* pSub = NULL;
  MakeSubZone(pParent, 1024 * 16, PU_LEVEL, "SUB_GA", &pSub);
  REQUIRE(pSub != nullptr);

  MemBlock_t* pOuter = Z_ZoneAsBlock(pSub);
  Allocator_t alloc = MEM_STRATEGY(pOuter).GetAllocator(pOuter);
  REQUIRE(alloc.pContext == pSub);
  REQUIRE(alloc.Malloc != nullptr);

  // Allocator must be functional.
  Handle_t h = alloc.Malloc(alloc.pContext, 128, nullptr);
  void* p = h.pData;
  REQUIRE(p != nullptr);

  free(pBuf);
}

/* =========================================================================
 * Test_Strategy_GetAllocator_Stack
 * MEM_STRATEGY on a TYPE_STACK block returns a Stack allocator whose
 * pContext matches the embedded MemStack_t.
 * ========================================================================= */
TEST_CASE("Zone – Strategy Get Allocator Stack", "[Zone]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "PAR_GAS");

  const uint32_t kStackCapSize = 1024 * 4;
  void* pData = Z_Malloc(pZone, kStackCapSize, PU_STATIC, nullptr);
  REQUIRE(pData != nullptr);
  MemBlock_t* pBlk = GetBlockHeader(pData);
  pBlk->type = TYPE_STACK;
  MEM_STRATEGY(pBlk).Init(pBlk, kStackCapSize, PU_STATIC, "STK_GA");

  Allocator_t alloc = MEM_STRATEGY(pBlk).GetAllocator(pBlk);
  REQUIRE(alloc.pContext == pData);
  REQUIRE(alloc.Malloc != nullptr);

  Handle_t h = alloc.Malloc(alloc.pContext, 64, nullptr);
  void* p = h.pData;
  REQUIRE(p != nullptr);

  free(pBuf);
}

/* =========================================================================
 * Test_SubStack_GenericPattern
 * Stamp a raw zone block as TYPE_STACK, call GetAllocator + Init via MemStrategy,
 * then verify the stack allocates correctly. This is the pattern that replaces
 * the deleted Z_AllocNameTable.
 * ========================================================================= */
TEST_CASE("Zone – Sub-Stack Generic Pattern", "[Zone]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "STK_PAT");

  const uint32_t kStackTotal = 1024 * 8;
  void* pData = Z_Malloc(pZone, kStackTotal, PU_STATIC, nullptr);
  REQUIRE(pData != nullptr);

  MemBlock_t* pBlk = GetBlockHeader(pData);
  pBlk->type = TYPE_STACK;

  Allocator_t alloc = MEM_STRATEGY(pBlk).GetAllocator(pBlk);
  MEM_STRATEGY(pBlk).Init(pBlk, kStackTotal, PU_STATIC, "STKBUF");

  // Stack must now be usable via the generic allocator interface.
  Handle_t hA = alloc.Malloc(alloc.pContext, 256, nullptr); void* pA = hA.pData;
  Handle_t hB = alloc.Malloc(alloc.pContext, 256, nullptr); void* pB = hB.pData;
  REQUIRE(pA != nullptr);
  REQUIRE(pB != nullptr);
  REQUIRE(pB > pA); // linear, B must be after A

  // Stack_Check must pass via the strategy.
  REQUIRE(MEM_STRATEGY(pBlk).Check(pBlk));

  free(pBuf);
}

/* =========================================================================
 * Test_PU_STATIC_SurvivesPurgeTag
 * PU_STATIC blocks are never purgeable — Z_PurgeTag targeting PU_STATIC
 * must not free them.
 * ========================================================================= */
TEST_CASE("Zone – PU_STATIC Survives Purge Tag", "[Zone]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "STA_PURGE");

  void* hStatic = nullptr; Z_Malloc(pZone, 256, PU_STATIC, &hStatic);
  void* hCache  = nullptr; Z_Malloc(pZone, 256, PU_CACHE,  &hCache);
  REQUIRE((hStatic && hCache));

  Z_PurgeTag(pZone, PU_STATIC); // must not free the static block
  REQUIRE(hStatic != nullptr);     // PU_STATIC — never freed
  REQUIRE(hCache != nullptr);      // different tag — untouched
  REQUIRE(Z_Check(pZone));

  free(pBuf);
}

/* =========================================================================
 * Test_PU_LEVEL_PurgedByPurgeTag
 * Z_PurgeTag with an explicit tag frees PU_LEVEL blocks even though
 * they are not auto-purgeable.
 * ========================================================================= */
TEST_CASE("Zone – PU_LEVEL Purged By Purge Tag", "[Zone]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "LVL_PURGE");

  void* hLevel  = nullptr; Z_Malloc(pZone, 256, PU_LEVEL,  &hLevel);
  void* hStatic = nullptr; Z_Malloc(pZone, 256, PU_STATIC, &hStatic);
  REQUIRE((hLevel && hStatic));

  Z_PurgeTag(pZone, PU_LEVEL);
  REQUIRE(hLevel == nullptr);          // PU_LEVEL — freed by explicit PurgeTag
  REQUIRE(hStatic != nullptr);     // different tag — untouched
  REQUIRE(Z_Check(pZone));

  free(pBuf);
}

/* =========================================================================
 * Test_PU_LEVEL_SkipsOnPurgeTagRange
 * Z_PurgeTagRange only frees blocks with tag > PU_LOCKED.
 * PU_LEVEL (= 2) must survive any range purge.
 * ========================================================================= */
TEST_CASE("Zone – PU_LEVEL Skips On Purge Tag Range", "[Zone]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "LVL_RANGE");

  void* hLevel = nullptr; Z_Malloc(pZone, 256, PU_LEVEL, &hLevel);
  void* hCache = nullptr; Z_Malloc(pZone, 256, PU_CACHE, &hCache);
  REQUIRE((hLevel && hCache));

  // A very wide range — PU_LEVEL is still not freed (tag <= PU_LOCKED guard).
  Z_PurgeTagRange(pZone, PU_LEVEL, PU_CACHE);
  REQUIRE(hLevel != nullptr); // tag < PU_LOCKED — survives range purge
  REQUIRE(hCache == nullptr);     // tag > PU_LOCKED — freed
  REQUIRE(Z_Check(pZone));

  free(pBuf);
}

/* =========================================================================
 * Test_PU_LEVEL_SlidesInCompact
 * PU_LEVEL blocks (moveable) must slide toward the start during Z_Compact.
 * ========================================================================= */
TEST_CASE("Zone – PU_LEVEL Slides In Compact", "[Zone]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "LVL_SLIDE");

  void* hHole  = nullptr; void* pHole  = Z_Malloc(pZone, 512, PU_LEVEL, &hHole);
  void* hLevel = nullptr; void* pLevel = Z_Malloc(pZone, 256, PU_LEVEL, &hLevel);
  REQUIRE((pHole && pLevel));

  uint32_t offsetBefore = Z_GetOffset(pZone, GetBlockHeader(pLevel));

  Z_Free(pZone, pHole);
  Z_Compact(pZone);

  REQUIRE(Z_Check(pZone));
  REQUIRE(hLevel != nullptr); // handle valid after remap
  // Block must have moved to an earlier offset.
  uint32_t offsetAfter = Z_GetOffset(pZone, GetBlockHeader(hLevel));
  REQUIRE(offsetAfter < offsetBefore);

  free(pBuf);
}

/* =========================================================================
 * Test_PU_CACHE_AutoPurge
 * Z_Malloc must evict PU_CACHE blocks automatically when memory is tight.
 * ========================================================================= */
TEST_CASE("Zone – PU_CACHE Auto Purge", "[Zone]") {
  // Small zone so we can fill it quickly.
  const uint32_t kSmall = 1024 * 4;
  void*      pBuf  = malloc(kSmall);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kSmall, PU_STATIC, "AUTOPURGE");

  // Fill most of the zone with a PU_CACHE block.
  void* hCache = nullptr;
  Z_Malloc(pZone, kSmall / 2, PU_CACHE, &hCache);
  REQUIRE(hCache != nullptr);

  // Now request an allocation that only fits if the cache block is evicted.
  void* pNew = Z_Malloc(pZone, kSmall / 2, PU_STATIC, nullptr);
  REQUIRE(pNew != nullptr);  // succeeded — cache was auto-purged
  REQUIRE(hCache == nullptr);    // PU_CACHE handle nulled by Z_Free

  REQUIRE(Z_Check(pZone));
  free(pBuf);
}

/* =========================================================================
 * Test_HierarchicalCompact
 * After Z_Compact moves a TYPE_ZONE block inside the parent zone:
 *   - The caller's MemZone_t* (wired as ppUser) must be patched to the new address.
 *   - The sub-zone's internal structure must remain valid.
 *   - The parent zone must be consistent.
 * This validates the full "Russian Doll" compact-remap protocol.
 * ========================================================================= */
TEST_CASE("Zone – Hierarchical Compact", "[Zone]") {
  void*      pBuf    = malloc(kCapacity);
  MemZone_t* pParent = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "PAR_HC");

  // 1. Anchor at PU_LEVEL — freeing it creates a gap before the sub-zone.
  void* pAnchor = Z_Malloc(pParent, 512, PU_LEVEL, nullptr);
  REQUIRE(pAnchor != nullptr);

  // 2. Sub-zone: ppUser = &pSub so Z_Remap patches the pointer after compact.
  MemZone_t* pSub = NULL;
  MakeSubZone(pParent, 1024 * 8, PU_LEVEL, "SUB_HC", &pSub);
  REQUIRE(pSub != nullptr);

  // 3. Allocate inside the sub-zone to give it non-trivial internal state.
  void* hInner = nullptr;
  Z_Malloc(pSub, 128, PU_STATIC, &hInner);
  REQUIRE(hInner != nullptr);
  REQUIRE(Z_Check(pSub));

  // 4. Record the sub-zone's pre-compact address.
  MemZone_t* pSubBefore = pSub;

  // 5. Free the anchor — opens a gap at the start of the zone.
  Z_Free(pParent, pAnchor);

  // 6. Compact: the sub-zone block slides into the gap.
  Z_Compact(pParent);

  // 7. ppUser must have been patched — pSub must differ from before.
  REQUIRE(pSub != nullptr);
  REQUIRE(pSub != pSubBefore); // block moved; pointer was updated

  // 8. Sub-zone internal structure must survive the move.
  REQUIRE(Z_Check(pSub));
  REQUIRE(hInner != nullptr); // inner handle patched by recursive Z_Remap

  // 9. Parent must be consistent.
  REQUIRE(Z_Check(pParent));

  free(pBuf);
}

/* =========================================================================
 * Test_ZFindAllocator_Found
 * Z_FindAllocator returns a valid allocator when the name exists.
 * ========================================================================= */
TEST_CASE("Zone – Z_FindAllocator Found", "[Zone]") {
  void*      pBuf    = malloc(kCapacity);
  MemZone_t* pParent = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "ROOT");
  MemZone_t* pSub    = NULL;
  MakeSubZone(pParent, 1024 * 4, PU_STATIC, "strings", &pSub);
  REQUIRE(pSub != nullptr);

  Allocator_t alloc = Z_FindAllocator(pParent, name_from_cstr("strings"));
  REQUIRE(Allocator_IsValid(alloc));

  free(pBuf);
}

/* =========================================================================
 * Test_ZFindAllocator_NotFound
 * Z_FindAllocator returns INVALID_ALLOCATOR when the name is absent.
 * ========================================================================= */
TEST_CASE("Zone – Z_FindAllocator Not Found", "[Zone]") {
  void*      pBuf    = malloc(kCapacity);
  MemZone_t* pParent = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "ROOT");
  MemZone_t* pSub    = NULL;
  MakeSubZone(pParent, 1024 * 4, PU_STATIC, "bar", &pSub);

  Allocator_t alloc = Z_FindAllocator(pParent, name_from_cstr("foo"));
  REQUIRE_FALSE(Allocator_IsValid(alloc));

  free(pBuf);
}

/* =========================================================================
 * Test_ZFindAllocator_DeepNesting
 * Z_FindAllocator recurses and finds a name at depth 2.
 * ========================================================================= */
TEST_CASE("Zone – Z_FindAllocator Deep Nesting", "[Zone]") {
  void*      pBuf    = malloc(kCapacity);
  MemZone_t* pRoot   = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "ROOT");
  MemZone_t* pMid    = NULL;
  MakeSubZone(pRoot, 1024 * 8, PU_STATIC, "mid", &pMid);
  REQUIRE(pMid != nullptr);
  MemZone_t* pDeep   = NULL;
  MakeSubZone(pMid, 1024 * 2, PU_STATIC, "deep", &pDeep);
  REQUIRE(pDeep != nullptr);

  Allocator_t alloc = Z_FindAllocator(pRoot, name_from_cstr("deep"));
  REQUIRE(Allocator_IsValid(alloc));

  free(pBuf);
}

/* =========================================================================
 * Test_ZFindAllocator_DataChildNoMatch
 * TYPE_DATA children have no name — search must skip them without crashing.
 * ========================================================================= */
TEST_CASE("Zone – Z_FindAllocator Data Child No Match", "[Zone]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "ROOT");
  Z_Malloc(pZone, 128, PU_STATIC, nullptr); // TYPE_DATA block, no name
  Z_Malloc(pZone, 64,  PU_STATIC, nullptr);

  Allocator_t alloc = Z_FindAllocator(pZone, name_from_cstr("anything"));
  REQUIRE_FALSE(Allocator_IsValid(alloc)); // no match — no crash

  free(pBuf);
}

/* =========================================================================
 * Test_ZFindAllocator_NullZone
 * Z_FindAllocator(NULL, ...) must return INVALID_ALLOCATOR, not crash.
 * ========================================================================= */
TEST_CASE("Zone – Z_FindAllocator Null Zone", "[Zone]") {
  Allocator_t alloc = Z_FindAllocator(nullptr, name_from_cstr("x"));
  REQUIRE_FALSE(Allocator_IsValid(alloc));
}
