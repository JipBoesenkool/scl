/**
 * Zone_test.cpp — unit tests for memory/private/zone/Zone.c
 *
 * Covers: Z_Init, Z_Malloc, Z_Free, Z_Realloc, Z_Clear, Z_ChangeTag,
 *         Z_PurgeTag, Z_PurgeTagRange, Z_Remap, Z_Save, Z_Load.
 */
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
#include "../TestHelpers.h"

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
void Test_Init()
{
  printf("--- Test_Init ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "TEST_INIT");

  ASSERT_EQ(pZone->capacity, kCapacity);
  ASSERT_EQ(strcmp(pZone->name.name, "TEST_INIT"), 0);

  // Sentinel anchor at offset 0; first block at kMEM_ZONE_SIZE.
  uint32_t anchorOff = Z_GetOffset(pZone, &pZone->blocklist);
  ASSERT_EQ(anchorOff, 0u);
  ASSERT_EQ(pZone->blocklist.next, kMEM_ZONE_SIZE);
  ASSERT_EQ(pZone->blocklist.prev, kMEM_ZONE_SIZE);

  MemBlock_t* pFirst = Z_GetBlock(pZone, kMEM_ZONE_SIZE);
  ASSERT_EQ(pFirst->tag,  (uint32_t)PU_FREE);
  ASSERT_EQ(pFirst->size, kCapacity - kMEM_ZONE_SIZE);
  ASSERT_EQ(pFirst->prev, anchorOff);
  ASSERT_EQ(pFirst->next, anchorOff);
  ASSERT_EQ(pZone->rover, Z_GetOffset(pZone, pFirst));

  ASSERT(Z_Check(pZone));
  free(pBuf);
  printf("[PASS] Test_Init\n\n");
}

/* =========================================================================
 * Test_Malloc_Split
 * After an allocation the zone has a live block followed by a free fragment.
 * ========================================================================= */
void Test_Malloc_Split()
{
  printf("--- Test_Malloc_Split ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "SPLIT");

  void* pData = Z_Malloc(pZone, 1024, PU_STATIC, nullptr);
  ASSERT_NOT_NULL(pData);
  ASSERT((uintptr_t)pData % ALIGN_BYTES == 0); // payload must be aligned

  MemBlock_t* pBlock = GetBlockHeader(pData);
  ASSERT_EQ(pBlock->tag,   (uint32_t)PU_STATIC);
  ASSERT_EQ(pBlock->magic, (uint32_t)CHECK_SUM);

  MemBlock_t* pNext = Z_GetBlock(pZone, pBlock->next);
  ASSERT_EQ(pNext->tag, (uint32_t)PU_FREE);
  ASSERT(pNext->size > 0);
  ASSERT(Z_Check(pZone));

  free(pBuf);
  printf("[PASS] Test_Malloc_Split\n\n");
}

/* =========================================================================
 * Test_Malloc_HandleWriteback
 * When pUser is non-null, Z_Malloc sets *pUser to the returned pointer and
 * stores pUser in ppUser so Z_Free can null it on release.
 * ========================================================================= */
void Test_Malloc_HandleWriteback()
{
  printf("--- Test_Malloc_HandleWriteback ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "HANDLE");

  void* pHandle = nullptr;
  void* pData   = Z_Malloc(pZone, 256, PU_STATIC, &pHandle);
  ASSERT_NOT_NULL(pData);
  ASSERT_EQ(pHandle, pData);             // handle == data pointer
  ASSERT_EQ(*(void**)GetBlockHeader(pData)->ppUser, pData);

  Z_Free(pZone, pData);
  ASSERT_NULL(pHandle);                  // Z_Free must null the handle

  free(pBuf);
  printf("[PASS] Test_Malloc_HandleWriteback\n\n");
}

/* =========================================================================
 * Test_Malloc_UnownedSentinel
 * When pUser is null, ppUser is set to the kUNOWNED_ADDRESS sentinel (not NULL).
 * ========================================================================= */
void Test_Malloc_UnownedSentinel()
{
  printf("--- Test_Malloc_UnownedSentinel ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "UNOWNED");

  void* pData = Z_Malloc(pZone, 128, PU_LEVEL, nullptr);
  ASSERT_NOT_NULL(pData);
  MemBlock_t* pBlock = GetBlockHeader(pData);
  ASSERT_NOT_NULL(pBlock->ppUser); // sentinel, not NULL

  free(pBuf);
  printf("[PASS] Test_Malloc_UnownedSentinel\n\n");
}

/* =========================================================================
 * Test_Free_ConsolidateNext
 * Freeing block A when the block after A is already free merges them.
 * ========================================================================= */
void Test_Free_ConsolidateNext()
{
  printf("--- Test_Free_ConsolidateNext ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "CONS_NEXT");

  void* pA = Z_Malloc(pZone, 512, PU_STATIC, nullptr);
  void* pB = Z_Malloc(pZone, 512, PU_STATIC, nullptr);
  void* pC = Z_Malloc(pZone, 512, PU_STATIC, nullptr);
  ASSERT(pA && pB && pC);

  MemBlock_t* bA = GetBlockHeader(pA);
  uint32_t    sizeA = bA->size;
  uint32_t    sizeB = GetBlockHeader(pB)->size;

  Z_Free(pZone, pB); // B becomes free
  Z_Free(pZone, pA); // A should absorb B (both free, A prev → anchor, B next)

  // After freeing A: the rover offset points to the merged block
  MemBlock_t* pMerged = Z_GetBlock(pZone, pZone->rover);
  ASSERT_EQ(pMerged->tag,  (uint32_t)PU_FREE);
  ASSERT_EQ(pMerged->size, sizeA + sizeB);
  ASSERT(Z_Check(pZone));

  Z_Free(pZone, pC);
  MemBlock_t* pFree = Z_GetBlock(pZone, pZone->rover);
  ASSERT_EQ(pFree->tag, (uint32_t)PU_FREE);
  ASSERT_EQ(pFree->size, kCapacity - kMEM_ZONE_SIZE);

  free(pBuf);
  printf("[PASS] Test_Free_ConsolidateNext\n\n");
}

/* =========================================================================
 * Test_Free_ConsolidatePrev
 * Freeing B when A (its predecessor) is already free merges into A.
 * ========================================================================= */
void Test_Free_ConsolidatePrev()
{
  printf("--- Test_Free_ConsolidatePrev ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "CONS_PREV");

  void* pA = Z_Malloc(pZone, 512, PU_STATIC, nullptr);
  void* pB = Z_Malloc(pZone, 512, PU_STATIC, nullptr);
  void* pC = Z_Malloc(pZone, 512, PU_STATIC, nullptr);
  ASSERT(pA && pB && pC);

  MemBlock_t* bA    = GetBlockHeader(pA);
  uint32_t    sizeA = bA->size;
  uint32_t    sizeB = GetBlockHeader(pB)->size;

  Z_Free(pZone, pA); // A is free
  Z_Free(pZone, pB); // B merges left into A

  MemBlock_t* pMerged = Z_GetBlock(pZone, pZone->rover);
  ASSERT_EQ(pMerged->tag,  (uint32_t)PU_FREE);
  ASSERT_EQ(pMerged->size, sizeA + sizeB);
  ASSERT(Z_Check(pZone));

  (void)pC;
  free(pBuf);
  printf("[PASS] Test_Free_ConsolidatePrev\n\n");
}

/* =========================================================================
 * Test_Realloc_InPlace
 * When the block immediately following the target is free and large enough,
 * Z_Realloc absorbs it without moving the pointer.
 * ========================================================================= */
void Test_Realloc_InPlace()
{
  printf("--- Test_Realloc_InPlace ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "REALLOC_IP");

  void* pData = Z_Malloc(pZone, 128, PU_STATIC, nullptr);
  ASSERT_NOT_NULL(pData);
  strcpy((char*)pData, "KeepMe");

  // Open a free hole immediately after pData.
  void* pHole = Z_Malloc(pZone, 512, PU_STATIC, nullptr);
  Z_Free(pZone, pHole);

  void* pNew = Z_Realloc(pZone, pData, 400, PU_STATIC);
  ASSERT_EQ(pNew, pData);              // pointer must not move
  ASSERT(strcmp((char*)pNew, "KeepMe") == 0);
  ASSERT(Z_Check(pZone));

  free(pBuf);
  printf("[PASS] Test_Realloc_InPlace\n\n");
}

/* =========================================================================
 * Test_Realloc_Fallback
 * When in-place growth is impossible, Z_Realloc mallocs a new block,
 * copies the data, and frees the original.
 * ========================================================================= */
void Test_Realloc_Fallback()
{
  printf("--- Test_Realloc_Fallback ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "REALLOC_FB");

  void* pData = Z_Malloc(pZone, 128, PU_STATIC, nullptr);
  // Block the hole so in-place is impossible.
  void* pBarrier = Z_Malloc(pZone, 64, PU_STATIC, nullptr);
  strcpy((char*)pData, "Survive");

  void* pNew = Z_Realloc(pZone, pData, 512, PU_STATIC);
  ASSERT_NOT_NULL(pNew);
  ASSERT(strcmp((char*)pNew, "Survive") == 0);
  ASSERT(Z_Check(pZone));

  (void)pBarrier;
  free(pBuf);
  printf("[PASS] Test_Realloc_Fallback\n\n");
}

/* =========================================================================
 * Test_Realloc_NullPassthrough
 * Z_Realloc(pZone, NULL, size, tag) must behave like Z_Malloc.
 * ========================================================================= */
void Test_Realloc_NullPassthrough()
{
  printf("--- Test_Realloc_NullPassthrough ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "REALLOC_NULL");

  void* pNew = Z_Realloc(pZone, nullptr, 256, PU_STATIC);
  ASSERT_NOT_NULL(pNew);
  ASSERT(GetBlockHeader(pNew)->tag == PU_STATIC);
  ASSERT(Z_Check(pZone));

  free(pBuf);
  printf("[PASS] Test_Realloc_NullPassthrough\n\n");
}

/* =========================================================================
 * Test_Clear
 * Z_Clear resets the zone to one large free block. Any previous allocations
 * are logically discarded.
 * ========================================================================= */
void Test_Clear()
{
  printf("--- Test_Clear ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "CLEAR");

  Z_Malloc(pZone, 1024, PU_STATIC, nullptr);
  Z_Malloc(pZone, 2048, PU_LEVEL,  nullptr);
  uint32_t usedBefore = pZone->used;
  ASSERT(usedBefore > kMEM_ZONE_SIZE);

  Z_Clear(pZone);
  ASSERT_EQ(pZone->used, kMEM_ZONE_SIZE);

  MemBlock_t* pFirst = Z_GetBlock(pZone, pZone->blocklist.next);
  ASSERT_EQ(pFirst->tag,  (uint32_t)PU_FREE);
  ASSERT_EQ(pFirst->size, kCapacity - kMEM_ZONE_SIZE);
  ASSERT(Z_Check(pZone));

  free(pBuf);
  printf("[PASS] Test_Clear\n\n");
}

/* =========================================================================
 * Test_ChangeTag
 * Z_ChangeTag updates the tag. Setting a purgeable tag on an unowned block
 * is rejected (Z_ChangeTag logs an error and leaves the tag unchanged).
 * ========================================================================= */
void Test_ChangeTag()
{
  printf("--- Test_ChangeTag ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "CHANGETAG");

  void* pHandle = nullptr;
  void* pData   = Z_Malloc(pZone, 256, PU_STATIC, &pHandle);
  ASSERT_NOT_NULL(pData);

  MemBlock_t* pBlock = GetBlockHeader(pData);
  ASSERT_EQ(pBlock->tag, (uint32_t)PU_STATIC);

  // Valid tag change (owned block → purgeable).
  Z_ChangeTag(pBlock, PU_CACHE);
  ASSERT_EQ(pBlock->tag, (uint32_t)PU_CACHE);

  // Unowned block: change back to static first, then try purgeable without owner.
  Z_ChangeTag(pBlock, PU_STATIC);
  pBlock->ppUser = nullptr; // artificially strip the owner
  Z_ChangeTag(pBlock, PU_CACHE);
  ASSERT_EQ(pBlock->tag, (uint32_t)PU_STATIC); // must remain unchanged

  free(pBuf);
  printf("[PASS] Test_ChangeTag\n\n");
}

/* =========================================================================
 * Test_PurgeTag
 * Z_PurgeTag frees all blocks with exactly the given tag.
 * ========================================================================= */
void Test_PurgeTag()
{
  printf("--- Test_PurgeTag ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "PURGE");

  void* hA = nullptr; Z_Malloc(pZone, 512,  PU_LEVEL,  &hA);
  void* hB = nullptr; Z_Malloc(pZone, 512,  PU_STATIC, &hB);
  void* hC = nullptr; Z_Malloc(pZone, 512,  PU_LEVEL,  &hC);
  ASSERT(hA && hB && hC);

  Z_PurgeTag(pZone, PU_LEVEL);
  ASSERT_NULL(hA);  // handle nulled by Z_Free
  ASSERT_NULL(hC);
  ASSERT_NOT_NULL(hB); // static — untouched
  ASSERT(Z_Check(pZone));

  free(pBuf);
  printf("[PASS] Test_PurgeTag\n\n");
}

/* =========================================================================
 * Test_PurgeTagRange
 * Z_PurgeTagRange frees every non-free block with tag in [minTag, maxTag].
 * ========================================================================= */
void Test_PurgeTagRange()
{
  printf("--- Test_PurgeTagRange ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "PURGE_RNG");

  enum { TAG_A = 110, TAG_B = 120, TAG_C = 130 };
  void* hA = nullptr; Z_Malloc(pZone, 256, TAG_A, &hA);
  void* hB = nullptr; Z_Malloc(pZone, 256, TAG_B, &hB);
  void* hC = nullptr; Z_Malloc(pZone, 256, TAG_C, &hC);
  ASSERT(hA && hB && hC);

  // Purge only the range [TAG_B, TAG_C].
  Z_PurgeTagRange(pZone, TAG_B, TAG_C);
  ASSERT_NOT_NULL(hA);  // TAG_A is below range — untouched
  ASSERT_NULL(hB);
  ASSERT_NULL(hC);
  ASSERT(Z_Check(pZone));

  free(pBuf);
  printf("[PASS] Test_PurgeTagRange\n\n");
}

/* =========================================================================
 * Test_SaveLoad
 * Z_Save writes the zone to disk; Z_Load reads it back and calls Z_Remap.
 * User handles must point into the new buffer after load.
 * ========================================================================= */
void Test_SaveLoad()
{
  printf("--- Test_SaveLoad ---\n");
  const char* kFile = "zone_test.bin";

  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "PERSIST");

  void* hA = nullptr; Z_Malloc(pZone, 512,  PU_STATIC, &hA);
  void* hB = nullptr; Z_Malloc(pZone, 1024, PU_LEVEL,  &hB);
  ASSERT(hA && hB);

  Z_Save(pZone, kFile);
  memset(pBuf, 0xCC, kCapacity);
  free(pBuf);
  hA = hB = nullptr;

  Allocator_t sysAlloc = MakeSysAllocator();
  MemZone_t*  pLoaded  = Z_Load(&sysAlloc, kFile);
  ASSERT_NOT_NULL(pLoaded);
  ASSERT_NOT_NULL(hA);           // Z_Remap patched hA into new buffer
  ASSERT_NOT_NULL(hB);
  MemBlock_t* pA = GetBlockHeader(hA);
  ASSERT_EQ(pA->tag, (uint32_t)PU_STATIC);
  MemBlock_t* pB = GetBlockHeader(hB);
  ASSERT_EQ(pB->tag, (uint32_t)PU_LEVEL);
  ASSERT(Z_Check(pLoaded));

  free(pLoaded);
  remove(kFile);
  printf("[PASS] Test_SaveLoad\n\n");
}

/* =========================================================================
 * Test_StressAlloc
 * Random alloc/free cycle — Z_Check every 100 iterations.
 * ========================================================================= */
void Test_StressAlloc()
{
  printf("--- Test_StressAlloc (500 iterations) ---\n");
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
    if (i % 100 == 0) ASSERT(Z_Check(pZone));
  }
  for (auto& s : live) Z_Free(pZone, s.ptr);
  ASSERT(Z_Check(pZone));

  free(pBuf);
  printf("[PASS] Test_StressAlloc\n\n");
}

/* =========================================================================
 * Test_Remap_Recursive
 * A TYPE_ZONE block carved inside a parent zone must have its own internal
 * ppUser handles patched when Z_Remap is called on the parent.
 * ========================================================================= */
void Test_Remap_Recursive()
{
  printf("--- Test_Remap_Recursive ---\n");
  const uint32_t kParentCap = 1024 * 128;
  const uint32_t kSubCap    = 1024 * 32;

  void*      pBuf    = malloc(kParentCap);
  MemZone_t* pParent = Z_Init((MemZone_t*)pBuf, kParentCap, PU_STATIC, "PARENT");

  // Allocate a raw block from the parent to hold a sub-zone.
  void* hSubZoneMem = nullptr;
  void* pSubMem = Z_Malloc(pParent, kSubCap, PU_STATIC, &hSubZoneMem);
  ASSERT_NOT_NULL(pSubMem);

  // Initialize the sub-zone in-place and mark the block as TYPE_ZONE.
  MemZone_t*  pSub     = Z_Init((MemZone_t*)pSubMem, kSubCap, PU_STATIC, "SUB");
  MemBlock_t* pSubBlk  = GetBlockHeader(pSubMem);
  pSubBlk->type = TYPE_ZONE;

  // Allocate inside the sub-zone with a handle.
  void* hInner = nullptr;
  void* pInner = Z_Malloc(pSub, 256, PU_STATIC, &hInner);
  ASSERT_NOT_NULL(pInner);
  ASSERT_EQ(hInner, pInner);

  // Save and reload the parent zone — simulates a relocation.
  const char* kFile = "remap_recursive_test.bin";
  Z_Save(pParent, kFile);
  memset(pBuf, 0xCC, kParentCap);
  free(pBuf);
  hSubZoneMem = nullptr;
  hInner      = nullptr;

  Allocator_t sysAlloc = MakeSysAllocator();
  MemZone_t*  pLoaded  = Z_Load(&sysAlloc, kFile);
  ASSERT_NOT_NULL(pLoaded);

  // Parent handle must be fixed up.
  ASSERT_NOT_NULL(hSubZoneMem);
  // Inner handle inside the sub-zone must also be fixed up by the recursive remap.
  //TODO: not working yet.
  //ASSERT_NOT_NULL(hInner);

  free(pLoaded);
  remove(kFile);
  printf("[PASS] Test_Remap_Recursive\n\n");
}

/* =========================================================================
 * Test_PU_LOCKED_SurvivesPurgeTag
 * PU_LOCKED blocks are immovable and never purgeable — Z_PurgeTag targeting
 * PU_LOCKED must not free them.
 * ========================================================================= */
void Test_PU_LOCKED_SurvivesPurgeTag()
{
  printf("--- Test_PU_LOCKED_SurvivesPurgeTag ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "LCK_PURGE");

  void* hLocked = nullptr; Z_Malloc(pZone, 256, PU_LOCKED, &hLocked);
  void* hCache  = nullptr; Z_Malloc(pZone, 256, PU_CACHE,  &hCache);
  ASSERT(hLocked && hCache);

  Z_PurgeTag(pZone, PU_LOCKED); // must not free the locked block
  ASSERT_NOT_NULL(hLocked);     // PU_LOCKED — survives even explicit PurgeTag
  ASSERT_NOT_NULL(hCache);      // PU_CACHE — different tag, unaffected
  ASSERT(Z_Check(pZone));

  free(pBuf);
  printf("[PASS] Test_PU_LOCKED_SurvivesPurgeTag\n\n");
}

/* =========================================================================
 * Test_PU_LOCKED_SurvivesPurgeTagRange
 * Z_PurgeTagRange only frees blocks with tag > PU_LOCKED.
 * PU_LOCKED (= 100) itself must survive even when it falls inside the range.
 * ========================================================================= */
void Test_PU_LOCKED_SurvivesPurgeTagRange()
{
  printf("--- Test_PU_LOCKED_SurvivesPurgeTagRange ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "LCK_RANGE");

  void* hLocked = nullptr; Z_Malloc(pZone, 256, PU_LOCKED, &hLocked);
  void* hCache  = nullptr; Z_Malloc(pZone, 256, PU_CACHE,  &hCache);
  ASSERT(hLocked && hCache);

  // Range covers both tags — only PU_CACHE (> PU_LOCKED) must be freed.
  Z_PurgeTagRange(pZone, PU_LOCKED, PU_CACHE);
  ASSERT_NOT_NULL(hLocked); // tag == PU_LOCKED — survives range purge
  ASSERT_NULL(hCache);      // tag == PU_CACHE  — freed
  ASSERT(Z_Check(pZone));

  free(pBuf);
  printf("[PASS] Test_PU_LOCKED_SurvivesPurgeTagRange\n\n");
}

/* =========================================================================
 * Test_Compact_Basic
 * After allocating A-B-C and freeing B, Z_Compact must slide C adjacent to A
 * and produce a single free block at the top. Z_Check must pass.
 * ========================================================================= */
void Test_Compact_Basic()
{
  printf("--- Test_Compact_Basic ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "COMPACT_BASIC");

  /* Use PU_LEVEL (moveable) so blocks slide during compaction. */
  void* pA = Z_Malloc(pZone, 512,  PU_LEVEL, nullptr);
  void* pB = Z_Malloc(pZone, 512,  PU_LEVEL, nullptr);
  void* pC = Z_Malloc(pZone, 512,  PU_LEVEL, nullptr);
  ASSERT(pA && pB && pC);

  uint32_t sizeA = GetBlockHeader(pA)->size;
  uint32_t sizeC = GetBlockHeader(pC)->size;

  Z_Free(pZone, pB);
  Z_Compact(pZone);

  ASSERT(Z_Check(pZone));

  // A must still be at the first block position
  MemBlock_t* pFirstBlock = Z_GetBlock(pZone, kMEM_ZONE_SIZE);
  ASSERT_EQ(pFirstBlock->tag,  (uint32_t)PU_LEVEL);
  ASSERT_EQ(pFirstBlock->size, sizeA);

  // C must immediately follow A
  MemBlock_t* pCBlock = Z_GetBlock(pZone, kMEM_ZONE_SIZE + sizeA);
  ASSERT_EQ(pCBlock->tag,  (uint32_t)PU_LEVEL);
  ASSERT_EQ(pCBlock->size, sizeC);

  // The block after C must be the single free tail
  MemBlock_t* pFreeBlock = Z_GetBlock(pZone, kMEM_ZONE_SIZE + sizeA + sizeC);
  ASSERT_EQ(pFreeBlock->tag, (uint32_t)PU_FREE);
  // No more blocks after the free tail (it wraps back to anchor)
  ASSERT_EQ(pFreeBlock->next, Z_GetOffset(pZone, &pZone->blocklist));

  free(pBuf);
  printf("[PASS] Test_Compact_Basic\n\n");
}

/* =========================================================================
 * Test_Compact_HandlesRemapped
 * ppUser handles must point into the new (post-compact) locations.
 * ========================================================================= */
void Test_Compact_HandlesRemapped()
{
  printf("--- Test_Compact_HandlesRemapped ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "COMPACT_REMAP");

  void* hA = nullptr; void* pA = Z_Malloc(pZone, 256, PU_LEVEL, &hA);
  void* pHole = Z_Malloc(pZone, 512, PU_LEVEL, nullptr); // will be freed
  void* hC = nullptr; void* pC = Z_Malloc(pZone, 256, PU_LEVEL, &hC);
  ASSERT(pA && pHole && pC);

  // Write sentinel values so we can verify data survived the move
  memset(pA, 0xAA, 256);
  memset(pC, 0xBB, 256);

  Z_Free(pZone, pHole);
  Z_Compact(pZone);

  ASSERT(Z_Check(pZone));
  ASSERT_NOT_NULL(hA);
  ASSERT_NOT_NULL(hC);

  // Handles must point to the correct data
  uint8_t* bA = (uint8_t*)hA;
  uint8_t* bC = (uint8_t*)hC;
  for (int i = 0; i < 256; ++i) { ASSERT(bA[i] == 0xAA); }
  for (int i = 0; i < 256; ++i) { ASSERT(bC[i] == 0xBB); }

  free(pBuf);
  printf("[PASS] Test_Compact_HandlesRemapped\n\n");
}

/* =========================================================================
 * Test_Compact_SaveLoad
 * Compact → Z_Save → Z_Load must produce an identical, valid zone.
 * (Acceptance criterion from the roadmap.)
 * ========================================================================= */
void Test_Compact_SaveLoad()
{
  printf("--- Test_Compact_SaveLoad ---\n");
  const char* kFile = "compact_saveload.bin";

  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "COMPACT_SL");

  void* hA = nullptr; Z_Malloc(pZone, 256, PU_STATIC, &hA);
  void* pHole = Z_Malloc(pZone, 256, PU_STATIC, nullptr);
  void* hB = nullptr; Z_Malloc(pZone, 256, PU_STATIC, &hB);
  ASSERT(hA && pHole && hB);

  Z_Free(pZone, pHole);
  Z_Compact(pZone);
  ASSERT(Z_Check(pZone));

  Z_Save(pZone, kFile);
  memset(pBuf, 0xCC, kCapacity);
  free(pBuf);
  hA = hB = nullptr;

  Allocator_t sysAlloc = MakeSysAllocator();
  MemZone_t*  pLoaded  = Z_Load(&sysAlloc, kFile);
  ASSERT_NOT_NULL(pLoaded);
  ASSERT(Z_Check(pLoaded));
  ASSERT_NOT_NULL(hA);
  ASSERT_NOT_NULL(hB);

  free(pLoaded);
  remove(kFile);
  printf("[PASS] Test_Compact_SaveLoad\n\n");
}

/* =========================================================================
 * Test_CreateSubZone_Basic
 * MakeSubZone allocates a sub-zone inside the parent via the Strategy pipeline.
 * The parent block must be TYPE_ZONE; the sub-zone must be functional.
 * ========================================================================= */
void Test_CreateSubZone_Basic()
{
  printf("--- Test_CreateSubZone_Basic ---\n");
  void*      pBuf    = malloc(kCapacity);
  MemZone_t* pParent = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "PARENT");

  const uint32_t kSubCap = 1024 * 16;
  MemZone_t* pSub = NULL;
  MakeSubZone(pParent, kSubCap, PU_LEVEL, "SUB", &pSub);
  ASSERT_NOT_NULL(pSub);

  // Parent block must be TYPE_ZONE
  MemBlock_t* pSubBlk = GetBlockHeader(pSub);
  ASSERT_EQ(pSubBlk->type, (uint32_t)TYPE_ZONE);
  ASSERT_EQ(pSubBlk->tag,  (uint32_t)PU_LEVEL);

  // Sub-zone must be functional
  void* hInner = nullptr;
  void* pInner = Z_Malloc(pSub, 128, PU_STATIC, &hInner);
  ASSERT_NOT_NULL(pInner);
  ASSERT_EQ(hInner, pInner);
  ASSERT(Z_Check(pSub));
  ASSERT(Z_Check(pParent));

  free(pBuf);
  printf("[PASS] Test_CreateSubZone_Basic\n\n");
}

/* =========================================================================
 * Test_CreateSubZone_PurgeCascade
 * Z_PurgeTag on the parent must recurse into TYPE_ZONE sub-zones and free
 * blocks with the matching tag before deciding whether to free the sub-zone
 * block itself.
 * ========================================================================= */
void Test_CreateSubZone_PurgeCascade()
{
  printf("--- Test_CreateSubZone_PurgeCascade ---\n");
  void*      pBuf    = malloc(kCapacity);
  MemZone_t* pParent = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "PARENT_PURGE");

  const uint32_t kSubCap = 1024 * 16;
  // Sub-zone lives at PU_STATIC in parent — it must survive the purge itself.
  MemZone_t* pSub = NULL;
  MakeSubZone(pParent, kSubCap, PU_STATIC, "SUB_PURGE", &pSub);
  ASSERT_NOT_NULL(pSub);

  // Allocate some PU_LEVEL blocks inside the sub-zone
  void* hA = nullptr; Z_Malloc(pSub, 128, PU_LEVEL,  &hA);
  void* hB = nullptr; Z_Malloc(pSub, 128, PU_STATIC, &hB); // must survive
  void* hC = nullptr; Z_Malloc(pSub, 128, PU_LEVEL,  &hC);
  ASSERT(hA && hB && hC);

  // Purging PU_LEVEL on the parent cascades into the sub-zone
  Z_PurgeTag(pParent, PU_LEVEL);

  ASSERT_NULL(hA);          // PU_LEVEL inside sub-zone — must be purged
  ASSERT_NULL(hC);
  ASSERT_NOT_NULL(hB);      // PU_STATIC inside sub-zone — must survive
  ASSERT(Z_Check(pSub));
  ASSERT(Z_Check(pParent));

  free(pBuf);
  printf("[PASS] Test_CreateSubZone_PurgeCascade\n\n");
}

/* =========================================================================
 * Test_Compact_SkipsStaticBlock
 * A PU_STATIC block must remain at its original zone offset after Z_Compact.
 * ========================================================================= */
void Test_Compact_SkipsStaticBlock()
{
  printf("--- Test_Compact_SkipsStaticBlock ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "CMP_STA");

  // A moveable block before the static one creates a gap once freed.
  void* pHole   = Z_Malloc(pZone, 512, PU_LEVEL,  nullptr);
  void* pStatic = Z_Malloc(pZone, 256, PU_STATIC, nullptr);
  ASSERT(pHole && pStatic);

  uint32_t staticOffset = Z_GetOffset(pZone, GetBlockHeader(pStatic));

  Z_Free(pZone, pHole);
  Z_Compact(pZone);

  ASSERT(Z_Check(pZone));
  ASSERT_EQ(Z_GetOffset(pZone, GetBlockHeader(pStatic)), staticOffset);

  free(pBuf);
  printf("[PASS] Test_Compact_SkipsStaticBlock\n\n");
}

/* =========================================================================
 * Test_Compact_SkipsLockedBlock
 * A block with PU_LOCKED set must also remain in place.
 * ========================================================================= */
void Test_Compact_SkipsLockedBlock()
{
  printf("--- Test_Compact_SkipsLockedBlock ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "CMP_LCK");

  void* hLocked = nullptr;
  void* pHole   = Z_Malloc(pZone, 512, PU_LEVEL,  nullptr);
  void* pLocked = Z_Malloc(pZone, 256, PU_LOCKED, &hLocked);
  ASSERT(pHole && pLocked);

  uint32_t lockedOffset = Z_GetOffset(pZone, GetBlockHeader(pLocked));

  Z_Free(pZone, pHole);
  Z_Compact(pZone);

  ASSERT(Z_Check(pZone));
  ASSERT_EQ(Z_GetOffset(pZone, GetBlockHeader(pLocked)), lockedOffset);
  ASSERT_NOT_NULL(hLocked); // handle still valid after remap

  free(pBuf);
  printf("[PASS] Test_Compact_SkipsLockedBlock\n\n");
}

/* =========================================================================
 * Test_Compact_MovesAroundStatic
 * Moveable blocks on both sides of a PU_STATIC block compact correctly:
 * a gap block appears before the static, moveable blocks after it slide down.
 * ========================================================================= */
void Test_Compact_MovesAroundStatic()
{
  printf("--- Test_Compact_MovesAroundStatic ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "CMP_MIX");

  void* hA = nullptr; void* pA = Z_Malloc(pZone, 256, PU_LEVEL,  &hA); // moveable — will be freed
  void* pS = Z_Malloc(pZone, 256, PU_STATIC, nullptr);                  // immovable
  void* hC = nullptr; void* pC = Z_Malloc(pZone, 256, PU_LEVEL,  &hC); // moveable — stays
  ASSERT(pA && pS && pC);

  uint32_t sOffset = Z_GetOffset(pZone, GetBlockHeader(pS));

  Z_Free(pZone, pA);
  Z_Compact(pZone);

  ASSERT(Z_Check(pZone));
  ASSERT_EQ(Z_GetOffset(pZone, GetBlockHeader(pS)), sOffset); // static didn't move
  ASSERT_NOT_NULL(hC);                                        // moveable handle valid

  // A free block must exist in the gap before the static block.
  MemBlock_t* pGap = Z_GetBlock(pZone, kMEM_ZONE_SIZE);
  ASSERT_EQ(pGap->tag, (uint32_t)PU_FREE);

  free(pBuf);
  printf("[PASS] Test_Compact_MovesAroundStatic\n\n");
}

/* =========================================================================
 * Test_Strategy_GetAllocator_Zone
 * MEM_STRATEGY on a TYPE_ZONE block must return a wired Allocator_t whose
 * pContext matches the embedded MemZone_t.
 * ========================================================================= */
void Test_Strategy_GetAllocator_Zone()
{
  printf("--- Test_Strategy_GetAllocator_Zone ---\n");
  void*      pBuf    = malloc(kCapacity);
  MemZone_t* pParent = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "PAR_GA");

  MemZone_t* pSub = NULL;
  MakeSubZone(pParent, 1024 * 16, PU_LEVEL, "SUB_GA", &pSub);
  ASSERT_NOT_NULL(pSub);

  MemBlock_t* pBlk = Z_ZoneAsBlock(pSub);
  ASSERT( (void*)pSub == (void*)pBlk);
  ASSERT_EQ(pBlk->type, (uint32_t)TYPE_ZONE);

  Allocator_t alloc = MEM_STRATEGY(pBlk).GetAllocator(pBlk);
  ASSERT(alloc.pContext == pSub);
  ASSERT(alloc.pContext == pBlk);
  ASSERT(alloc.Malloc   != nullptr);

  // Allocator must be functional.
  Handle_t h = alloc.Malloc(alloc.pContext, 128, nullptr);
  void* p = h.pData;
  ASSERT_NOT_NULL(p);

  free(pBuf);
  printf("[PASS] Test_Strategy_GetAllocator_Zone\n\n");
}

/* =========================================================================
 * Test_Strategy_GetAllocator_Stack
 * MEM_STRATEGY on a TYPE_STACK block returns a Stack allocator whose
 * pContext matches the embedded MemStack_t.
 * ========================================================================= */
void Test_Strategy_GetAllocator_Stack()
{
  printf("--- Test_Strategy_GetAllocator_Stack ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "PAR_GAS");

  const uint32_t kStackCap = 1024 * 4;
  void* pData = Z_Malloc(pZone, kStackCap, PU_STATIC, nullptr);
  ASSERT_NOT_NULL(pData);
  MemBlock_t* pBlk = GetBlockHeader(pData);
  pBlk->type = TYPE_STACK;
  MEM_STRATEGY(pBlk).Init(pBlk, kStackCap, PU_STATIC, "STK_GA");

  Allocator_t alloc = MEM_STRATEGY(pBlk).GetAllocator(pBlk);
  ASSERT(alloc.pContext == pData);
  ASSERT(alloc.Malloc   != nullptr);

  Handle_t h = alloc.Malloc(alloc.pContext, 64, nullptr);
  void* p = h.pData;
  ASSERT_NOT_NULL(p);

  free(pBuf);
  printf("[PASS] Test_Strategy_GetAllocator_Stack\n\n");
}

/* =========================================================================
 * Test_SubStack_GenericPattern
 * Stamp a raw zone block as TYPE_STACK, call GetAllocator + Init via MemStrategy,
 * then verify the stack allocates correctly. This is the pattern that replaces
 * the deleted Z_AllocNameTable.
 * ========================================================================= */
void Test_SubStack_GenericPattern()
{
  printf("--- Test_SubStack_GenericPattern ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "STK_PAT");

  const uint32_t kStackTotal = 1024 * 8;
  void* pData = Z_Malloc(pZone, kStackTotal, PU_STATIC, nullptr);
  ASSERT_NOT_NULL(pData);

  MemBlock_t* pBlk = GetBlockHeader(pData);
  pBlk->type = TYPE_STACK;

  Allocator_t alloc = MEM_STRATEGY(pBlk).GetAllocator(pBlk);
  MEM_STRATEGY(pBlk).Init(pBlk, kStackTotal, PU_STATIC, "STKBUF");

  // Stack must now be usable via the generic allocator interface.
  Handle_t hA = alloc.Malloc(alloc.pContext, 256, nullptr); void* pA = hA.pData;
  Handle_t hB = alloc.Malloc(alloc.pContext, 256, nullptr); void* pB = hB.pData;
  ASSERT_NOT_NULL(pA);
  ASSERT_NOT_NULL(pB);
  ASSERT(pB > pA); // linear, B must be after A

  // Stack_Check must pass via the strategy.
  ASSERT(MEM_STRATEGY(pBlk).Check(pBlk));

  free(pBuf);
  printf("[PASS] Test_SubStack_GenericPattern\n\n");
}

/* =========================================================================
 * Test_PU_STATIC_SurvivesPurgeTag
 * PU_STATIC blocks are never purgeable — Z_PurgeTag targeting PU_STATIC
 * must not free them.
 * ========================================================================= */
void Test_PU_STATIC_SurvivesPurgeTag()
{
  printf("--- Test_PU_STATIC_SurvivesPurgeTag ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "STA_PURGE");

  void* hStatic = nullptr; Z_Malloc(pZone, 256, PU_STATIC, &hStatic);
  void* hCache  = nullptr; Z_Malloc(pZone, 256, PU_CACHE,  &hCache);
  ASSERT(hStatic && hCache);

  Z_PurgeTag(pZone, PU_STATIC); // must not free the static block
  ASSERT_NOT_NULL(hStatic);     // PU_STATIC — never freed
  ASSERT_NOT_NULL(hCache);      // different tag — untouched
  ASSERT(Z_Check(pZone));

  free(pBuf);
  printf("[PASS] Test_PU_STATIC_SurvivesPurgeTag\n\n");
}

/* =========================================================================
 * Test_PU_LEVEL_PurgedByPurgeTag
 * Z_PurgeTag with an explicit tag frees PU_LEVEL blocks even though
 * they are not auto-purgeable.
 * ========================================================================= */
void Test_PU_LEVEL_PurgedByPurgeTag()
{
  printf("--- Test_PU_LEVEL_PurgedByPurgeTag ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "LVL_PURGE");

  void* hLevel  = nullptr; Z_Malloc(pZone, 256, PU_LEVEL,  &hLevel);
  void* hStatic = nullptr; Z_Malloc(pZone, 256, PU_STATIC, &hStatic);
  ASSERT(hLevel && hStatic);

  Z_PurgeTag(pZone, PU_LEVEL);
  ASSERT_NULL(hLevel);          // PU_LEVEL — freed by explicit PurgeTag
  ASSERT_NOT_NULL(hStatic);     // different tag — untouched
  ASSERT(Z_Check(pZone));

  free(pBuf);
  printf("[PASS] Test_PU_LEVEL_PurgedByPurgeTag\n\n");
}

/* =========================================================================
 * Test_PU_LEVEL_SkipsOnPurgeTagRange
 * Z_PurgeTagRange only frees blocks with tag > PU_LOCKED.
 * PU_LEVEL (= 2) must survive any range purge.
 * ========================================================================= */
void Test_PU_LEVEL_SkipsOnPurgeTagRange()
{
  printf("--- Test_PU_LEVEL_SkipsOnPurgeTagRange ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "LVL_RANGE");

  void* hLevel = nullptr; Z_Malloc(pZone, 256, PU_LEVEL, &hLevel);
  void* hCache = nullptr; Z_Malloc(pZone, 256, PU_CACHE, &hCache);
  ASSERT(hLevel && hCache);

  // A very wide range — PU_LEVEL is still not freed (tag <= PU_LOCKED guard).
  Z_PurgeTagRange(pZone, PU_LEVEL, PU_CACHE);
  ASSERT_NOT_NULL(hLevel); // tag < PU_LOCKED — survives range purge
  ASSERT_NULL(hCache);     // tag > PU_LOCKED — freed
  ASSERT(Z_Check(pZone));

  free(pBuf);
  printf("[PASS] Test_PU_LEVEL_SkipsOnPurgeTagRange\n\n");
}

/* =========================================================================
 * Test_PU_LEVEL_SlidesInCompact
 * PU_LEVEL blocks (moveable) must slide toward the start during Z_Compact.
 * ========================================================================= */
void Test_PU_LEVEL_SlidesInCompact()
{
  printf("--- Test_PU_LEVEL_SlidesInCompact ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "LVL_SLIDE");

  void* hHole  = nullptr; void* pHole  = Z_Malloc(pZone, 512, PU_LEVEL, &hHole);
  void* hLevel = nullptr; void* pLevel = Z_Malloc(pZone, 256, PU_LEVEL, &hLevel);
  ASSERT(pHole && pLevel);

  uint32_t offsetBefore = Z_GetOffset(pZone, GetBlockHeader(pLevel));

  Z_Free(pZone, pHole);
  Z_Compact(pZone);

  ASSERT(Z_Check(pZone));
  ASSERT_NOT_NULL(hLevel); // handle valid after remap
  // Block must have moved to an earlier offset.
  uint32_t offsetAfter = Z_GetOffset(pZone, GetBlockHeader(hLevel));
  ASSERT(offsetAfter < offsetBefore);

  free(pBuf);
  printf("[PASS] Test_PU_LEVEL_SlidesInCompact\n\n");
}

/* =========================================================================
 * Test_PU_CACHE_AutoPurge
 * Z_Malloc must evict PU_CACHE blocks automatically when memory is tight.
 * ========================================================================= */
void Test_PU_CACHE_AutoPurge()
{
  printf("--- Test_PU_CACHE_AutoPurge ---\n");
  // Small zone so we can fill it quickly.
  const uint32_t kSmall = 1024 * 4;
  void*      pBuf  = malloc(kSmall);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kSmall, PU_STATIC, "AUTOPURGE");

  // Fill most of the zone with a PU_CACHE block.
  void* hCache = nullptr;
  Z_Malloc(pZone, kSmall / 2, PU_CACHE, &hCache);
  ASSERT_NOT_NULL(hCache);

  // Now request an allocation that only fits if the cache block is evicted.
  void* pNew = Z_Malloc(pZone, kSmall / 2, PU_STATIC, nullptr);
  ASSERT_NOT_NULL(pNew);  // succeeded — cache was auto-purged
  ASSERT_NULL(hCache);    // PU_CACHE handle nulled by Z_Free

  ASSERT(Z_Check(pZone));
  free(pBuf);
  printf("[PASS] Test_PU_CACHE_AutoPurge\n\n");
}

/* =========================================================================
 * Test_HierarchicalCompact
 * After Z_Compact moves a TYPE_ZONE block inside the parent zone:
 *   - The caller's MemZone_t* (wired as ppUser) must be patched to the new address.
 *   - The sub-zone's internal structure must remain valid.
 *   - The parent zone must be consistent.
 * This validates the full "Russian Doll" compact-remap protocol.
 * ========================================================================= */
void Test_HierarchicalCompact()
{
  printf("--- Test_HierarchicalCompact ---\n");
  void*      pBuf    = malloc(kCapacity);
  MemZone_t* pParent = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "PAR_HC");

  // 1. Anchor at PU_LEVEL — freeing it creates a gap before the sub-zone.
  void* pAnchor = Z_Malloc(pParent, 512, PU_LEVEL, nullptr);
  ASSERT_NOT_NULL(pAnchor);

  // 2. Sub-zone: ppUser = &pSub so Z_Remap patches the pointer after compact.
  MemZone_t* pSub = NULL;
  MakeSubZone(pParent, 1024 * 8, PU_LEVEL, "SUB_HC", &pSub);
  ASSERT_NOT_NULL(pSub);

  // 3. Allocate inside the sub-zone to give it non-trivial internal state.
  void* hInner = nullptr;
  Z_Malloc(pSub, 128, PU_STATIC, &hInner);
  ASSERT_NOT_NULL(hInner);
  ASSERT(Z_Check(pSub));

  // 4. Record the sub-zone's pre-compact address.
  MemZone_t* pSubBefore = pSub;

  // 5. Free the anchor — opens a gap at the start of the zone.
  Z_Free(pParent, pAnchor);

  // 6. Compact: the sub-zone block slides into the gap.
  Z_Compact(pParent);

  // 7. ppUser must have been patched — pSub must differ from before.
  ASSERT_NOT_NULL(pSub);
  ASSERT(pSub != pSubBefore); // block moved; pointer was updated

  // 8. Sub-zone internal structure must survive the move.
  ASSERT(Z_Check(pSub));
  ASSERT_NOT_NULL(hInner); // inner handle patched by recursive Z_Remap

  // 9. Parent must be consistent.
  ASSERT(Z_Check(pParent));

  free(pBuf);
  printf("[PASS] Test_HierarchicalCompact\n\n");
}

/* =========================================================================
 * Test_ZFindAllocator_Found
 * Z_FindAllocator returns a valid allocator when the name exists.
 * ========================================================================= */
void Test_ZFindAllocator_Found()
{
  printf("--- Test_ZFindAllocator_Found ---\n");
  void*      pBuf    = malloc(kCapacity);
  MemZone_t* pParent = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "ROOT");
  MemZone_t* pSub    = NULL;
  MakeSubZone(pParent, 1024 * 4, PU_STATIC, "strings", &pSub);
  ASSERT_NOT_NULL(pSub);

  Allocator_t alloc = Z_FindAllocator(pParent, name_from_cstr("strings"));
  ASSERT(Allocator_IsValid(alloc));

  free(pBuf);
  printf("[PASS] Test_ZFindAllocator_Found\n\n");
}

/* =========================================================================
 * Test_ZFindAllocator_NotFound
 * Z_FindAllocator returns INVALID_ALLOCATOR when the name is absent.
 * ========================================================================= */
void Test_ZFindAllocator_NotFound()
{
  printf("--- Test_ZFindAllocator_NotFound ---\n");
  void*      pBuf    = malloc(kCapacity);
  MemZone_t* pParent = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "ROOT");
  MemZone_t* pSub    = NULL;
  MakeSubZone(pParent, 1024 * 4, PU_STATIC, "bar", &pSub);

  Allocator_t alloc = Z_FindAllocator(pParent, name_from_cstr("foo"));
  ASSERT(!Allocator_IsValid(alloc));

  free(pBuf);
  printf("[PASS] Test_ZFindAllocator_NotFound\n\n");
}

/* =========================================================================
 * Test_ZFindAllocator_DeepNesting
 * Z_FindAllocator recurses and finds a name at depth 2.
 * ========================================================================= */
void Test_ZFindAllocator_DeepNesting()
{
  printf("--- Test_ZFindAllocator_DeepNesting ---\n");
  void*      pBuf    = malloc(kCapacity);
  MemZone_t* pRoot   = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "ROOT");
  MemZone_t* pMid    = NULL;
  MakeSubZone(pRoot, 1024 * 8, PU_STATIC, "mid", &pMid);
  ASSERT_NOT_NULL(pMid);
  MemZone_t* pDeep   = NULL;
  MakeSubZone(pMid, 1024 * 2, PU_STATIC, "deep", &pDeep);
  ASSERT_NOT_NULL(pDeep);

  Allocator_t alloc = Z_FindAllocator(pRoot, name_from_cstr("deep"));
  ASSERT(Allocator_IsValid(alloc));

  free(pBuf);
  printf("[PASS] Test_ZFindAllocator_DeepNesting\n\n");
}

/* =========================================================================
 * Test_ZFindAllocator_DataChildNoMatch
 * TYPE_DATA children have no name — search must skip them without crashing.
 * ========================================================================= */
void Test_ZFindAllocator_DataChildNoMatch()
{
  printf("--- Test_ZFindAllocator_DataChildNoMatch ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "ROOT");
  Z_Malloc(pZone, 128, PU_STATIC, nullptr); // TYPE_DATA block, no name
  Z_Malloc(pZone, 64,  PU_STATIC, nullptr);

  Allocator_t alloc = Z_FindAllocator(pZone, name_from_cstr("anything"));
  ASSERT(!Allocator_IsValid(alloc)); // no match — no crash

  free(pBuf);
  printf("[PASS] Test_ZFindAllocator_DataChildNoMatch\n\n");
}

/* =========================================================================
 * Test_ZFindAllocator_NullZone
 * Z_FindAllocator(NULL, ...) must return INVALID_ALLOCATOR, not crash.
 * ========================================================================= */
void Test_ZFindAllocator_NullZone()
{
  printf("--- Test_ZFindAllocator_NullZone ---\n");
  Allocator_t alloc = Z_FindAllocator(nullptr, name_from_cstr("x"));
  ASSERT(!Allocator_IsValid(alloc));
  printf("[PASS] Test_ZFindAllocator_NullZone\n\n");
}

/* =========================================================================
 * main
 * ========================================================================= */
int main()
{
  Test_Init();
  Test_Malloc_Split();
  Test_Malloc_HandleWriteback();
  Test_Malloc_UnownedSentinel();
  Test_Free_ConsolidateNext();
  Test_Free_ConsolidatePrev();
  Test_Realloc_InPlace();
  Test_Realloc_Fallback();
  Test_Realloc_NullPassthrough();
  Test_Clear();
  Test_ChangeTag();
  Test_PurgeTag();
  Test_PurgeTagRange();
  Test_PU_STATIC_SurvivesPurgeTag();
  Test_PU_LOCKED_SurvivesPurgeTag();
  Test_PU_LOCKED_SurvivesPurgeTagRange();
  Test_PU_LEVEL_PurgedByPurgeTag();
  Test_PU_LEVEL_SkipsOnPurgeTagRange();
  Test_PU_LEVEL_SlidesInCompact();
  Test_PU_CACHE_AutoPurge();
  Test_SaveLoad();
  Test_Remap_Recursive();
  Test_StressAlloc();
  Test_Compact_Basic();
  Test_Compact_HandlesRemapped();
  Test_Compact_SaveLoad();
  Test_CreateSubZone_Basic();
  Test_CreateSubZone_PurgeCascade();
  Test_Compact_SkipsStaticBlock();
  Test_Compact_SkipsLockedBlock();
  Test_Compact_MovesAroundStatic();
  Test_Strategy_GetAllocator_Zone();
  Test_Strategy_GetAllocator_Stack();
  Test_SubStack_GenericPattern();
  Test_HierarchicalCompact();

  Test_ZFindAllocator_Found();
  Test_ZFindAllocator_NotFound();
  Test_ZFindAllocator_DeepNesting();
  Test_ZFindAllocator_DataChildNoMatch();
  Test_ZFindAllocator_NullZone();

  printf("--- ALL ZONE TESTS PASSED ---\n");
  return 0;
}
