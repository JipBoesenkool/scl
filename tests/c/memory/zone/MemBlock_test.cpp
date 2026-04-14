/**
 * MemBlock_test.cpp — unit tests for memory/private/zone/MemBlock.c
 *
 * Covers: Align, GetBlockHeader, GetBlockData, Block_Save, Block_Load.
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "cLib/memory/zone/MemBlock.h"
#include "cLib/memory/allocator/Allocator.h"
#include "cLib/memory/allocator/ZoneAllocator.h"
#include "cLib/memory/zone/Zone.h"
#include "../TestHelpers.h"

/* =========================================================================
 * Test_Align
 * Align rounds up to the nearest ALIGN_BYTES (8) boundary.
 * ========================================================================= */
void Test_Align()
{
  printf("--- Test_Align ---\n");
  ASSERT_EQ(Align(0),  0u);
  ASSERT_EQ(Align(1),  8u);
  ASSERT_EQ(Align(7),  8u);
  ASSERT_EQ(Align(8),  8u);
  ASSERT_EQ(Align(9),  16u);
  ASSERT_EQ(Align(15), 16u);
  ASSERT_EQ(Align(16), 16u);
  ASSERT_EQ(Align(17), 24u);
  // Already-aligned large value stays the same.
  ASSERT_EQ(Align(1024), 1024u);
  // One byte over a boundary rounds up by a full ALIGN_BYTES.
  ASSERT_EQ(Align(1025), 1032u);
  // Result must always be a multiple of ALIGN_BYTES.
  for (uint32_t s = 0; s <= 256; ++s)
    ASSERT(Align(s) % ALIGN_BYTES == 0);
  printf("[PASS] Test_Align\n\n");
}

/* =========================================================================
 * Test_BlockPointerRoundtrip
 * GetBlockData(GetBlockHeader(pData)) == pData
 * GetBlockHeader(GetBlockData(pBlock)) == pBlock
 * ========================================================================= */
void Test_BlockPointerRoundtrip()
{
  printf("--- Test_BlockPointerRoundtrip ---\n");

  // Create a properly aligned byte buffer large enough for a header + payload.
  alignas(8) uint8_t buf[128] = {};
  MemBlock_t* pBlock = reinterpret_cast<MemBlock_t*>(buf);
  void*       pData  = GetBlockData(pBlock);

  ASSERT_NOT_NULL(pData);
  ASSERT_EQ((uint8_t*)pData, (uint8_t*)pBlock + kMEM_BLOCK_SIZE);
  ASSERT_EQ(GetBlockHeader(pData), pBlock);

  printf("[PASS] Test_BlockPointerRoundtrip\n\n");
}

/* =========================================================================
 * Test_BlockPointerNullGuards
 * GetBlockHeader(NULL) and GetBlockData(NULL) must return NULL.
 * ========================================================================= */
void Test_BlockPointerNullGuards()
{
  printf("--- Test_BlockPointerNullGuards ---\n");
  ASSERT_NULL(GetBlockHeader(nullptr));
  ASSERT_NULL(GetBlockData(nullptr));
  printf("[PASS] Test_BlockPointerNullGuards\n\n");
}

/* =========================================================================
 * Test_BlockSize
 * kMEM_BLOCK_SIZE must equal sizeof(MemBlock_t) rounded up to ALIGN_BYTES.
 * The static_assert in the header already enforces sizeof == 32, but this
 * test makes the relationship explicit and detectable at runtime.
 * ========================================================================= */
void Test_BlockSize()
{
  printf("--- Test_BlockSize ---\n");
  uint32_t expected = (sizeof(MemBlock_t) + ALIGN_MASK) & ~ALIGN_MASK;
  ASSERT_EQ(kMEM_BLOCK_SIZE, expected);
  ASSERT_EQ(kMEM_BLOCK_SIZE, 32u); // MemBlock_t is 32 bytes, already aligned.
  printf("[PASS] Test_BlockSize\n\n");
}

/* =========================================================================
 * Test_BlockSaveLoad
 * Block_Save serialises header metadata + payload to disk.
 * Block_Load allocates a fresh block, reads back the data, and restores it.
 * ========================================================================= */
void Test_BlockSaveLoad()
{
  printf("--- Test_BlockSaveLoad ---\n");
  const char* kFile = "memblock_test.bin";
  const char* kMsg  = "BlockSaveLoadPayload";

  // Set up a zone to allocate from.
  uint32_t   capacity = 1024 * 8;
  void*      pRaw     = malloc(capacity);
  MemZone_t* pZone    = Z_Init((MemZone_t*)pRaw, capacity, PU_STATIC, "BLK_ZONE");

  static void* pHandle = nullptr;
  void* pData = Z_Malloc(pZone, 64, PU_STATIC, &pHandle);
  ASSERT_NOT_NULL(pData);
  strcpy((char*)pData, kMsg);

  MemBlock_t* pBlock = GetBlockHeader(pData);
  ASSERT(Block_Save(pBlock, kFile));

  // Free and null the handle so Load into a fresh slot.
  Z_Free(pZone, pData);
  pHandle = nullptr;

  // Load back via zone allocator.
  Allocator_t  alloc       = Zone_GetAllocator(pZone);
  MemBlock_t*  pLoadedBlock = nullptr;
  void*        pLoaded     = Block_Load(&alloc, &pLoadedBlock, kFile);

  ASSERT_NOT_NULL(pLoaded);
  ASSERT_NOT_NULL(pLoadedBlock);
  ASSERT_EQ(pLoadedBlock->tag,  (uint32_t)PU_STATIC);
  ASSERT(strcmp((char*)pLoaded, kMsg) == 0);

  remove(kFile);
  free(pRaw);
  printf("[PASS] Test_BlockSaveLoad\n\n");
}

/* =========================================================================
 * Test_BlockSaveNullGuards
 * Block_Save returns false on null arguments and must not crash.
 * ========================================================================= */
void Test_BlockSaveNullGuards()
{
  printf("--- Test_BlockSaveNullGuards ---\n");
  ASSERT(!Block_Save(nullptr, "x.bin"));
  // Block_Load with null args must return null.
  MemBlock_t* out = nullptr;
  Allocator_t dummy = {};
  ASSERT_NULL(Block_Load(nullptr, &out, "x.bin"));
  ASSERT_NULL(Block_Load(&dummy,  nullptr, "x.bin"));
  ASSERT_NULL(Block_Load(&dummy,  &out,    nullptr));
  printf("[PASS] Test_BlockSaveNullGuards\n\n");
}

/* =========================================================================
 * Test_kInvalidBlock
 * kInvalidBlock has magic != CHECK_SUM, so Block_IsValid returns false.
 * ========================================================================= */
void Test_kInvalidBlock()
{
  printf("--- Test_kInvalidBlock ---\n");
  ASSERT(!Block_IsValid(&kInvalidBlock));
  ASSERT(kInvalidBlock.magic != CHECK_SUM);
  ASSERT(kInvalidBlock.type  == (uint32_t)TYPE_NONE);
  printf("[PASS] Test_kInvalidBlock\n\n");
}

/* =========================================================================
 * Test_BlockIsValid_NullGuard
 * Block_IsValid(nullptr) must return false without crashing.
 * ========================================================================= */
void Test_BlockIsValid_NullGuard()
{
  printf("--- Test_BlockIsValid_NullGuard ---\n");
  ASSERT(!Block_IsValid(nullptr));
  printf("[PASS] Test_BlockIsValid_NullGuard\n\n");
}

/* =========================================================================
 * Test_BlockIsValid_ValidBlock
 * A block initialised by the zone allocator has magic == CHECK_SUM.
 * ========================================================================= */
void Test_BlockIsValid_ValidBlock()
{
  printf("--- Test_BlockIsValid_ValidBlock ---\n");
  void*      pBuf  = malloc(1024 * 4);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, 1024 * 4, PU_STATIC, "BIV");
  void*      pData = Z_Malloc(pZone, 64, PU_STATIC, nullptr);
  ASSERT_NOT_NULL(pData);
  MemBlock_t* pBlock = GetBlockHeader(pData);
  ASSERT(Block_IsValid(pBlock));
  free(pBuf);
  printf("[PASS] Test_BlockIsValid_ValidBlock\n\n");
}

/* =========================================================================
 * main
 * ========================================================================= */
int main()
{
  Test_Align();
  Test_BlockPointerRoundtrip();
  Test_BlockPointerNullGuards();
  Test_BlockSize();
  Test_BlockSaveLoad();
  Test_BlockSaveNullGuards();
  Test_kInvalidBlock();
  Test_BlockIsValid_NullGuard();
  Test_BlockIsValid_ValidBlock();

  printf("--- ALL MEMBLOCK TESTS PASSED ---\n");
  return 0;
}
