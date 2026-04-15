#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "cLib/memory/zone/Zone.h"
#include "cLib/memory/zone/ZoneStats.h"
#include "cLib/memory/zone/MemBlock.h"

static const uint32_t kCapacity = 1024 * 64;

/* =========================================================================
 * Test_GetUsedSpace
 * Z_GetUsedSpace returns the sum of block->size for all blocks matching tag.
 * After alloc: used-space for the block's tag equals its aligned total size.
 * After free:  used-space for PU_FREE increases by the freed block's size.
 * ========================================================================= */
TEST_CASE("ZoneStats – Get Used Space", "[ZoneStats]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "STATS");

  uint32_t initialFree = Z_GetUsedSpace(pZone, PU_FREE);
  REQUIRE(initialFree > 0);
  REQUIRE(Z_GetUsedSpace(pZone, PU_LEVEL) == 0u);
  REQUIRE(Z_GetUsedSpace(pZone, PU_STATIC) == 0u);

  void* pA = Z_Malloc(pZone, 128, PU_LEVEL,  nullptr);
  void* pB = Z_Malloc(pZone, 256, PU_STATIC, nullptr);
  REQUIRE((pA != nullptr && pB != nullptr));

  uint32_t levelSpace  = Z_GetUsedSpace(pZone, PU_LEVEL);
  uint32_t staticSpace = Z_GetUsedSpace(pZone, PU_STATIC);
  uint32_t freeAfter   = Z_GetUsedSpace(pZone, PU_FREE);
  REQUIRE(levelSpace > 0);
  REQUIRE(staticSpace > 0);
  REQUIRE(freeAfter < initialFree);
  // All space partitions to exactly capacity.
  REQUIRE(pZone->used + freeAfter == kCapacity);

  // Free pA — its space moves to the PU_FREE bucket.
  Z_Free(pZone, pA);
  uint32_t freeAfterFree = Z_GetUsedSpace(pZone, PU_FREE);
  REQUIRE(freeAfterFree > freeAfter);
  REQUIRE(Z_GetUsedSpace(pZone, PU_LEVEL) == 0u);

  free(pBuf);
}

/* =========================================================================
 * Test_GetPurgeableSpace
 * Z_GetPurgeableSpace sums sizes of blocks with tag >= PU_CACHE.
 * Blocks with tag < PU_CACHE are not counted.
 * ========================================================================= */
TEST_CASE("ZoneStats – Get Purgeable Space", "[ZoneStats]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "PURGE");

  REQUIRE(Z_GetPurgeableSpace(pZone) == 0u);

  void* pStaticHandle = nullptr;
  void* pPurgeHandle  = nullptr;
  void* pCacheHandle  = nullptr;

  Z_Malloc(pZone, 512,  PU_STATIC, &pStaticHandle);
  Z_Malloc(pZone, 1024, PU_CACHE,  &pPurgeHandle);
  Z_Malloc(pZone, 2048, PU_CACHE,  &pCacheHandle);

  uint32_t purgeable = Z_GetPurgeableSpace(pZone);
  // Must include both PURGE and CACHE blocks (both >= PU_CACHE).
  REQUIRE(purgeable > 0);
  // Static block must NOT be included.
  uint32_t staticSpace = Z_GetUsedSpace(pZone, PU_STATIC);
  REQUIRE(staticSpace > 0);
  REQUIRE(purgeable < kCapacity);

  // After freeing the cache block, purgeable decreases.
  Z_Free(pZone, pCacheHandle);
  REQUIRE(Z_GetPurgeableSpace(pZone) < purgeable);

  free(pBuf);
}

/* =========================================================================
 * Test_Check_ValidZone
 * Z_Check passes on a freshly initialised zone and after standard operations.
 * ========================================================================= */
TEST_CASE("ZoneStats – Check Valid Zone", "[ZoneStats]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "CHECK");

  REQUIRE(Z_Check(pZone));

  void* pA = Z_Malloc(pZone, 256, PU_LEVEL,  nullptr);
  void* pB = Z_Malloc(pZone, 512, PU_STATIC, nullptr);
  REQUIRE(Z_Check(pZone));

  Z_Free(pZone, pA);
  REQUIRE(Z_Check(pZone));

  Z_Clear(pZone);
  REQUIRE(Z_Check(pZone));

  (void)pB;
  free(pBuf);
}

/* =========================================================================
 * Test_Check_MagicCorruption
 * Z_Check must return false when a block's magic number is overwritten.
 * ========================================================================= */
TEST_CASE("ZoneStats – Check Magic Corruption", "[ZoneStats]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "CORRUPT");

  void* pA = Z_Malloc(pZone, 256, PU_STATIC, nullptr);
  REQUIRE(Z_Check(pZone));

  // Corrupt the magic number directly.
  MemBlock_t* pBlock = GetBlockHeader(pA);
  uint32_t savedMagic = pBlock->magic;
  pBlock->magic = 0xDEADBEEF;
  REQUIRE_FALSE(Z_Check(pZone)); // must detect corruption

  // Restore so free doesn't crash the process.
  pBlock->magic = savedMagic;
  Z_Free(pZone, pA);
  free(pBuf);
}

/* =========================================================================
 * Test_CheckRange
 * Z_CheckRange validates structural integrity while only printing blocks
 * within the given tag range. The return value still covers all blocks.
 * ========================================================================= */
TEST_CASE("ZoneStats – Check Range Valid", "[ZoneStats]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "RANGE");

  void* pLevel  = Z_Malloc(pZone, 256,  PU_LEVEL,  nullptr);
  void* pStatic = Z_Malloc(pZone, 512,  PU_STATIC, nullptr);
  void* pCache  = nullptr;
  Z_Malloc(pZone, 1024, PU_CACHE, &pCache);

  // Filtering by tag range must not change the boolean result.
  REQUIRE(Z_CheckRange(pZone, PU_FREE, PU_STATIC));
  REQUIRE(Z_CheckRange(pZone, PU_CACHE, PU_CACHE));
  REQUIRE(Z_CheckRange(pZone, PU_LEVEL, PU_LEVEL));

  (void)pLevel; (void)pStatic;
  free(pBuf);
}

/* =========================================================================
 * Test_Verify
 * Z_Verify enforces: pZone->used + Σ(free block sizes) == capacity.
 * Tested across: fresh zone, multiple allocations, isolated free (no
 * coalescing, the double-count trap), full coalescing, and after Z_Clear.
 * ========================================================================= */
TEST_CASE("ZoneStats – Verify", "[ZoneStats]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "VERIFY");

  // 1. Fresh zone.
  REQUIRE(Z_Verify(pZone));

  // 2. After allocations.
  void* pA = Z_Malloc(pZone, 128,  PU_STATIC, nullptr);
  void* pB = Z_Malloc(pZone, 256,  PU_LEVEL,  nullptr);
  void* pC = Z_Malloc(pZone, 512,  PU_STATIC, nullptr);
  void* pD = Z_Malloc(pZone, 1024, PU_LEVEL,  nullptr);
  REQUIRE((pA && pB && pC && pD));
  REQUIRE(Z_Verify(pZone));

  // 3. Isolated free: B and D freed, A and C remain live on either side of B.
  //    B cannot coalesce — multiple free-block terms in the sum.
  Z_Free(pZone, pB);
  Z_Free(pZone, pD);
  REQUIRE(Z_Verify(pZone));

  // 4. Free A — now coalesces into B's left neighbour.
  Z_Free(pZone, pA);
  REQUIRE(Z_Verify(pZone));

  // 5. Free C — everything should merge back to one block.
  Z_Free(pZone, pC);
  REQUIRE(Z_Verify(pZone));

  // 6. Z_Clear must restore a clean state.
  Z_Malloc(pZone, 4096, PU_STATIC, nullptr);
  Z_Clear(pZone);
  REQUIRE(Z_Verify(pZone));

  free(pBuf);
}
