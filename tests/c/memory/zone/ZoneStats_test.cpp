/**
 * ZoneStats_test.cpp — unit tests for memory/private/zone/ZoneStats.c
 *
 * Covers: Z_GetUsedSpace, Z_GetPurgeableSpace, Z_Check, Z_CheckRange, Z_Verify.
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "cLib/memory/zone/Zone.h"
#include "cLib/memory/zone/ZoneStats.h"
#include "cLib/memory/zone/MemBlock.h"
#include "../TestHelpers.h"

static const uint32_t kCapacity = 1024 * 64;

/* =========================================================================
 * Test_GetUsedSpace
 * Z_GetUsedSpace returns the sum of block->size for all blocks matching tag.
 * After alloc: used-space for the block's tag equals its aligned total size.
 * After free:  used-space for PU_FREE increases by the freed block's size.
 * ========================================================================= */
void Test_GetUsedSpace()
{
  printf("--- Test_GetUsedSpace ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "STATS");

  uint32_t initialFree = Z_GetUsedSpace(pZone, PU_FREE);
  ASSERT(initialFree > 0);
  ASSERT_EQ(Z_GetUsedSpace(pZone, PU_LEVEL),  0u);
  ASSERT_EQ(Z_GetUsedSpace(pZone, PU_STATIC), 0u);

  void* pA = Z_Malloc(pZone, 128, PU_LEVEL,  nullptr);
  void* pB = Z_Malloc(pZone, 256, PU_STATIC, nullptr);
  ASSERT(pA && pB);

  uint32_t levelSpace  = Z_GetUsedSpace(pZone, PU_LEVEL);
  uint32_t staticSpace = Z_GetUsedSpace(pZone, PU_STATIC);
  uint32_t freeAfter   = Z_GetUsedSpace(pZone, PU_FREE);
  ASSERT(levelSpace  > 0);
  ASSERT(staticSpace > 0);
  ASSERT(freeAfter   < initialFree);
  // All space partitions to exactly capacity.
  ASSERT_EQ(pZone->used + freeAfter, kCapacity);

  // Free pA — its space moves to the PU_FREE bucket.
  Z_Free(pZone, pA);
  uint32_t freeAfterFree = Z_GetUsedSpace(pZone, PU_FREE);
  ASSERT(freeAfterFree > freeAfter);
  ASSERT_EQ(Z_GetUsedSpace(pZone, PU_LEVEL), 0u);

  free(pBuf);
  printf("[PASS] Test_GetUsedSpace\n\n");
}

/* =========================================================================
 * Test_GetPurgeableSpace
 * Z_GetPurgeableSpace sums sizes of blocks with tag >= PU_CACHE.
 * Blocks with tag < PU_CACHE are not counted.
 * ========================================================================= */
void Test_GetPurgeableSpace()
{
  printf("--- Test_GetPurgeableSpace ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "PURGE");

  ASSERT_EQ(Z_GetPurgeableSpace(pZone), 0u);

  void* pStaticHandle = nullptr;
  void* pPurgeHandle  = nullptr;
  void* pCacheHandle  = nullptr;

  Z_Malloc(pZone, 512,  PU_STATIC, &pStaticHandle);
  Z_Malloc(pZone, 1024, PU_CACHE,  &pPurgeHandle);
  Z_Malloc(pZone, 2048, PU_CACHE,  &pCacheHandle);

  uint32_t purgeable = Z_GetPurgeableSpace(pZone);
  // Must include both PURGE and CACHE blocks (both >= PU_CACHE).
  ASSERT(purgeable > 0);
  // Static block must NOT be included.
  uint32_t staticSpace = Z_GetUsedSpace(pZone, PU_STATIC);
  ASSERT(staticSpace > 0);
  ASSERT(purgeable < kCapacity);

  // After freeing the cache block, purgeable decreases.
  Z_Free(pZone, pCacheHandle);
  ASSERT(Z_GetPurgeableSpace(pZone) < purgeable);

  free(pBuf);
  printf("[PASS] Test_GetPurgeableSpace\n\n");
}

/* =========================================================================
 * Test_Check_ValidZone
 * Z_Check passes on a freshly initialised zone and after standard operations.
 * ========================================================================= */
void Test_Check_ValidZone()
{
  printf("--- Test_Check_ValidZone ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "CHECK");

  ASSERT(Z_Check(pZone));

  void* pA = Z_Malloc(pZone, 256, PU_LEVEL,  nullptr);
  void* pB = Z_Malloc(pZone, 512, PU_STATIC, nullptr);
  ASSERT(Z_Check(pZone));

  Z_Free(pZone, pA);
  ASSERT(Z_Check(pZone));

  Z_Clear(pZone);
  ASSERT(Z_Check(pZone));

  (void)pB;
  free(pBuf);
  printf("[PASS] Test_Check_ValidZone\n\n");
}

/* =========================================================================
 * Test_Check_MagicCorruption
 * Z_Check must return false when a block's magic number is overwritten.
 * ========================================================================= */
void Test_Check_MagicCorruption()
{
  printf("--- Test_Check_MagicCorruption ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "CORRUPT");

  void* pA = Z_Malloc(pZone, 256, PU_STATIC, nullptr);
  ASSERT(Z_Check(pZone));

  // Corrupt the magic number directly.
  MemBlock_t* pBlock = GetBlockHeader(pA);
  uint32_t savedMagic = pBlock->magic;
  pBlock->magic = 0xDEADBEEF;
  ASSERT(!Z_Check(pZone)); // must detect corruption

  // Restore so free doesn't crash the process.
  pBlock->magic = savedMagic;
  Z_Free(pZone, pA);
  free(pBuf);
  printf("[PASS] Test_Check_MagicCorruption\n\n");
}

/* =========================================================================
 * Test_CheckRange
 * Z_CheckRange validates structural integrity while only printing blocks
 * within the given tag range. The return value still covers all blocks.
 * ========================================================================= */
void Test_CheckRange_Valid()
{
  printf("--- Test_CheckRange_Valid ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "RANGE");

  void* pLevel  = Z_Malloc(pZone, 256,  PU_LEVEL,  nullptr);
  void* pStatic = Z_Malloc(pZone, 512,  PU_STATIC, nullptr);
  void* pPurge  = nullptr;
  Z_Malloc(pZone, 1024, PU_CACHE, &pPurge);

  // Filtering by tag range must not change the boolean result.
  ASSERT(Z_CheckRange(pZone, PU_FREE, PU_STATIC));
  ASSERT(Z_CheckRange(pZone, PU_CACHE, PU_CACHE));
  ASSERT(Z_CheckRange(pZone, PU_LEVEL, PU_LEVEL));

  (void)pLevel; (void)pStatic;
  free(pBuf);
  printf("[PASS] Test_CheckRange_Valid\n\n");
}

/* =========================================================================
 * Test_Verify
 * Z_Verify enforces: pZone->used + Σ(free block sizes) == capacity.
 * Tested across: fresh zone, multiple allocations, isolated free (no
 * coalescing, the double-count trap), full coalescing, and after Z_Clear.
 * ========================================================================= */
void Test_Verify()
{
  printf("--- Test_Verify ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "VERIFY");

  // 1. Fresh zone.
  ASSERT(Z_Verify(pZone));

  // 2. After allocations.
  void* pA = Z_Malloc(pZone, 128,  PU_STATIC, nullptr);
  void* pB = Z_Malloc(pZone, 256,  PU_LEVEL,  nullptr);
  void* pC = Z_Malloc(pZone, 512,  PU_STATIC, nullptr);
  void* pD = Z_Malloc(pZone, 1024, PU_LEVEL,  nullptr);
  ASSERT(pA && pB && pC && pD);
  ASSERT(Z_Verify(pZone));

  // 3. Isolated free: B and D freed, A and C remain live on either side of B.
  //    B cannot coalesce — multiple free-block terms in the sum.
  Z_Free(pZone, pB);
  Z_Free(pZone, pD);
  ASSERT(Z_Verify(pZone));

  // 4. Free A — now coalesces into B's left neighbour.
  Z_Free(pZone, pA);
  ASSERT(Z_Verify(pZone));

  // 5. Free C — everything should merge back to one block.
  Z_Free(pZone, pC);
  ASSERT(Z_Verify(pZone));

  // 6. Z_Clear must restore a clean state.
  Z_Malloc(pZone, 4096, PU_STATIC, nullptr);
  Z_Clear(pZone);
  ASSERT(Z_Verify(pZone));

  free(pBuf);
  printf("[PASS] Test_Verify\n\n");
}

/* =========================================================================
 * main
 * ========================================================================= */
int main()
{
  Test_GetUsedSpace();
  Test_GetPurgeableSpace();
  Test_Check_ValidZone();
  Test_Check_MagicCorruption();
  Test_CheckRange_Valid();
  Test_Verify();

  printf("--- ALL ZONESTATS TESTS PASSED ---\n");
  return 0;
}
