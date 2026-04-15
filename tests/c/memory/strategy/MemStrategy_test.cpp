#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "cLib/memory/strategy/MemStrategy.h"
#include "cLib/memory/strategy/StackStrategy.h"
#include "cLib/memory/strategy/ZoneStrategy.h"
#include "cLib/memory/zone/Zone.h"
#include "cLib/memory/zone/ZoneStats.h"
#include "cLib/memory/zone/MemBlock.h"
#include "cLib/memory/allocator/StackAllocator.h"
#include "cLib/memory/allocator/ZoneAllocator.h"
#include "cLib/memory/allocator/NullAllocator.h"
#include "cLib/string/cName.h"

static const uint32_t kCapacity = 1024 * 16;

/* =========================================================================
 * Test_TableWiring
 * Every function pointer in all four rows must be non-null.
 * ========================================================================= */
TEST_CASE("MemStrategy – Table Wiring", "[MemStrategy]") {
  for (int t = 0; t < 4; ++t) {
    REQUIRE(kMemStrategies[t].Init != nullptr);
    REQUIRE(kMemStrategies[t].Clear != nullptr);
    REQUIRE(kMemStrategies[t].Purge != nullptr);
    REQUIRE(kMemStrategies[t].Check != nullptr);
    REQUIRE(kMemStrategies[t].Remap != nullptr);
    REQUIRE(kMemStrategies[t].Save != nullptr);
    REQUIRE(kMemStrategies[t].Load != nullptr);
    REQUIRE(kMemStrategies[t].GetAllocator != nullptr);
    REQUIRE(kMemStrategies[t].GetBlockName != nullptr);
    REQUIRE(kMemStrategies[t].FindBlock != nullptr);
  }
}

/* =========================================================================
 * Test_MacroDispatch
 * MEM_STRATEGY(pBlock) must select the row matching pBlock->type.
 * ========================================================================= */
TEST_CASE("MemStrategy – Macro Dispatch", "[MemStrategy]") {
  MemBlock_t block = {};
  block.magic = CHECK_SUM;

  block.type = TYPE_DATA;
  REQUIRE((void*)MEM_STRATEGY(&block).Check == (void*)kMemStrategies[TYPE_DATA].Check);

  block.type = TYPE_ZONE;
  REQUIRE((void*)MEM_STRATEGY(&block).Check == (void*)kMemStrategies[TYPE_ZONE].Check);

  block.type = TYPE_STACK;
  REQUIRE((void*)MEM_STRATEGY(&block).Check == (void*)kMemStrategies[TYPE_STACK].Check);
}

/* =========================================================================
 * Test_DataStrategy_Check
 * TYPE_DATA Check always returns true — no sub-structure to validate.
 * ========================================================================= */
TEST_CASE("MemStrategy – Data Strategy Check", "[MemStrategy]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "DS_CHK");

  void* pData = Z_Malloc(pZone, 256, PU_STATIC, nullptr);
  REQUIRE(pData != nullptr);
  MemBlock_t* pBlock = GetBlockHeader(pData);
  REQUIRE(pBlock->type == (uint32_t)TYPE_DATA);
  REQUIRE(MEM_STRATEGY(pBlock).Check(pBlock));

  free(pBuf);
}

/* =========================================================================
 * Test_DataStrategy_Remap
 * TYPE_DATA Remap is a no-op — must not crash and must not modify the block.
 * ========================================================================= */
TEST_CASE("MemStrategy – Data Strategy Remap", "[MemStrategy]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "DS_REMAP");

  void* pData = Z_Malloc(pZone, 128, PU_STATIC, nullptr);
  MemBlock_t* pBlock = GetBlockHeader(pData);
  uint32_t sizeBefore = pBlock->size;
  uint32_t tagBefore  = pBlock->tag;

  MEM_STRATEGY(pBlock).Remap(pBlock); // no-op, must not crash

  REQUIRE(pBlock->size == sizeBefore);
  REQUIRE(pBlock->tag == tagBefore);

  free(pBuf);
}

/* =========================================================================
 * Test_DataStrategy_Purge
 * TYPE_DATA Purge is a no-op — must not crash.
 * ========================================================================= */
TEST_CASE("MemStrategy – Data Strategy Purge", "[MemStrategy]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "DS_PURGE");

  void* pData = Z_Malloc(pZone, 64, PU_STATIC, nullptr);
  MemBlock_t* pBlock = GetBlockHeader(pData);

  MEM_STRATEGY(pBlock).Purge(pBlock); // no-op, must not crash
  REQUIRE(pBlock->magic == (uint32_t)CHECK_SUM); // block untouched

  free(pBuf);
}

/* =========================================================================
 * Test_StackStrategy_Init_Check
 * StackStrategy_Init sets up the embedded MemStack_t.
 * StackStrategy_Check returns true for a healthy stack.
 * ========================================================================= */
TEST_CASE("MemStrategy – Stack Strategy Init and Check", "[MemStrategy]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "SK_CHK");

  const uint32_t kStackTotal = 1024;
  void* pStackMem = Z_Malloc(pZone, kStackTotal, PU_STATIC, nullptr);
  REQUIRE(pStackMem != nullptr);

  MemBlock_t* pBlock = GetBlockHeader(pStackMem);
  pBlock->type = TYPE_STACK;

  MEM_STRATEGY(pBlock).Init(pBlock, kStackTotal, PU_STATIC, "SUB");
  REQUIRE(MEM_STRATEGY(pBlock).Check(pBlock)); // freshly initialised → valid

  // Corrupt used counter so it exceeds capacity.
  MemStack_t* pStack = (MemStack_t*)GetBlockData(pBlock);
  uint32_t saved = pStack->used;
  pStack->used = pStack->capacity + 1;
  REQUIRE_FALSE(MEM_STRATEGY(pBlock).Check(pBlock)); // overflow → invalid

  pStack->used = saved;
  free(pBuf);
}

/* =========================================================================
 * Test_GetBlockName
 * TYPE_DATA / TYPE_NONE blocks return NULL (no embedded name).
 * TYPE_ZONE blocks return the name passed to Init.
 * TYPE_STACK blocks return the name passed to Init.
 * ========================================================================= */
TEST_CASE("MemStrategy – Get Block Name", "[MemStrategy]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "ROOT");

  // TYPE_DATA block — no name
  void* pData = Z_Malloc(pZone, 128, PU_STATIC, nullptr);
  REQUIRE(pData != nullptr);
  MemBlock_t* pDataBlk = GetBlockHeader(pData);
  REQUIRE(MEM_STRATEGY(pDataBlk).GetBlockName(pDataBlk) == nullptr);

  // TYPE_ZONE block — returns the sub-zone's name
  MemZone_t* pSub = NULL;
  void* pSubMem = Z_Malloc(pZone, 1024 * 4, PU_STATIC, &pSub);
  REQUIRE(pSubMem != nullptr);
  MemBlock_t* pZoneBlk = GetBlockHeader(pSubMem);
  pZoneBlk->type = TYPE_ZONE;
  MEM_STRATEGY(pZoneBlk).Init(pZoneBlk, 0, PU_STATIC, "SUBZONE");
  const char* zoneName = MEM_STRATEGY(pZoneBlk).GetBlockName(pZoneBlk);
  REQUIRE(zoneName != nullptr);
  REQUIRE(strcmp(zoneName, "SUBZONE") == 0);

  // TYPE_STACK block — returns the sub-stack's name
  void* pStkMem = Z_Malloc(pZone, 1024 * 2, PU_STATIC, nullptr);
  REQUIRE(pStkMem != nullptr);
  MemBlock_t* pStkBlk = GetBlockHeader(pStkMem);
  pStkBlk->type = TYPE_STACK;
  MEM_STRATEGY(pStkBlk).Init(pStkBlk, 0, PU_STATIC, "MYSTACK");
  const char* stkName = MEM_STRATEGY(pStkBlk).GetBlockName(pStkBlk);
  REQUIRE(stkName != nullptr);
  REQUIRE(strcmp(stkName, "MYSTACK") == 0);

  free(pBuf);
}

/* =========================================================================
 * Test_ZoneStrategy_Init
 * ZoneStrategy_Init must initialise a valid MemZone_t in the block payload.
 * The parent block header (magic, next, prev, ppUser) must be preserved.
 * ========================================================================= */
TEST_CASE("MemStrategy – Zone Strategy Init", "[MemStrategy]") {
  void*      pBuf    = malloc(kCapacity);
  MemZone_t* pParent = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "PAR_INIT");

  const uint32_t kSubSize = 1024 * 4;
  MemZone_t* pSub = NULL;
  void* pMem = Z_Malloc(pParent, kSubSize, PU_STATIC, &pSub);
  REQUIRE(pMem != nullptr);

  MemBlock_t* pBlock = GetBlockHeader(pMem);
  uint32_t savedNext = pBlock->next;
  uint32_t savedPrev = pBlock->prev;
  pBlock->type = TYPE_ZONE;

  MEM_STRATEGY(pBlock).Init(pBlock, 0, PU_STATIC, "SUBINIT");

  /* Parent block header must be intact */
  REQUIRE(pBlock->magic == (uint32_t)CHECK_SUM);
  REQUIRE(pBlock->next == savedNext);
  REQUIRE(pBlock->prev == savedPrev);

  /* Sub-zone at payload must be valid */
  MemZone_t* pZ = (MemZone_t*)GetBlockData(pBlock);
  REQUIRE(Z_Check(pZ));
  REQUIRE(pZ->capacity > 0);

  free(pBuf);
}

/* =========================================================================
 * Test_ZoneStrategy_Remap_Check
 * TYPE_ZONE Remap and Check operate on the embedded sub-zone.
 * ========================================================================= */
TEST_CASE("MemStrategy – Zone Strategy Remap and Check", "[MemStrategy]") {
  void*      pBuf    = malloc(kCapacity);
  MemZone_t* pParent = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "PAR");

  MemZone_t* pSub = NULL;
  void* pMem = Z_Malloc(pParent, 1024 * 4, PU_STATIC, &pSub);
  REQUIRE(pMem != nullptr);

  MemBlock_t* pBlkParent = GetBlockHeader(pMem);
  pBlkParent->type = TYPE_ZONE;
  MEM_STRATEGY(pBlkParent).Init(pBlkParent, 0, PU_STATIC, "SUB");
  REQUIRE(pSub != nullptr);

  MemBlock_t* pBlk = Z_ZoneAsBlock(pSub);
  REQUIRE(pBlk->type == (uint32_t)TYPE_ZONE);

  REQUIRE(MEM_STRATEGY(pBlk).Check(pBlk)); // valid sub-zone → true
  MEM_STRATEGY(pBlk).Remap(pBlk);         // must not crash on a valid zone

  free(pBuf);
}

/* =========================================================================
 * Test_ZoneStrategy_GetAllocator
 * GetAllocator on a TYPE_ZONE block returns a wired Allocator_t whose
 * pContext is the embedded MemZone_t.
 * ========================================================================= */
TEST_CASE("MemStrategy – Zone Strategy Get Allocator", "[MemStrategy]") {
  void*      pBuf    = malloc(kCapacity);
  MemZone_t* pParent = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "PAR_GA");

  MemZone_t* pSub = NULL;
  void* pMemGA = Z_Malloc(pParent, 1024 * 4, PU_STATIC, &pSub);
  REQUIRE(pMemGA != nullptr);
  MemBlock_t* pBlkGA = GetBlockHeader(pMemGA);
  pBlkGA->type = TYPE_ZONE;
  MEM_STRATEGY(pBlkGA).Init(pBlkGA, 0, PU_STATIC, "SUB_GA");
  REQUIRE(pSub != nullptr);

  MemBlock_t* pBlk  = Z_ZoneAsBlock(pSub);
  Allocator_t alloc = MEM_STRATEGY(pBlk).GetAllocator(pBlk);
  REQUIRE(alloc.pContext == pSub);
  REQUIRE(alloc.Malloc != nullptr);

  Handle_t h = alloc.Malloc(alloc.pContext, 64, nullptr);
  REQUIRE(h.pData != nullptr);

  free(pBuf);
}

/* =========================================================================
 * Test_DataStrategy_GetAllocator_IsNullObject
 * GetAllocator on a TYPE_DATA block returns a safe NullObject allocator.
 * Calling Malloc on it must not crash — returns kInvalidHandle.
 * ========================================================================= */
TEST_CASE("MemStrategy – Data Strategy Get Allocator Is Null Object", "[MemStrategy]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "DS_NULL");

  void* pData = Z_Malloc(pZone, 64, PU_STATIC, nullptr);
  REQUIRE(pData != nullptr);
  MemBlock_t* pBlock = GetBlockHeader(pData);
  REQUIRE(pBlock->type == (uint32_t)TYPE_DATA);

  Allocator_t alloc = MEM_STRATEGY(pBlock).GetAllocator(pBlock);
  // Must be callable — NullObject, not a crash
  Handle_t h = alloc.Malloc(alloc.pContext, 64, nullptr);
  REQUIRE(h.id == kINVALID_HANDLE);  // logged warning, returned invalid handle

  free(pBuf);
}

/* =========================================================================
 * Test_NullStrategy_GetAllocator_IsNullObject
 * GetAllocator on a TYPE_NONE block returns a safe NullObject allocator.
 * ========================================================================= */
TEST_CASE("MemStrategy – Null Strategy Get Allocator Is Null Object", "[MemStrategy]") {
  MemBlock_t block = {};
  block.magic = CHECK_SUM;
  block.type  = TYPE_NONE;

  Allocator_t alloc = MEM_STRATEGY(&block).GetAllocator(&block);
  Handle_t h = alloc.Malloc(alloc.pContext, 32, nullptr);
  REQUIRE(h.id == kINVALID_HANDLE);
}

/* =========================================================================
 * Test_DataStrategy_FindBlock_ReturnsInvalid
 * DataStrategy has no children — FindBlock must return &kInvalidBlock, not NULL.
 * ========================================================================= */
TEST_CASE("MemStrategy – Data Strategy Find Block Returns Invalid", "[MemStrategy]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "FB_DATA");

  void* pData = Z_Malloc(pZone, 64, PU_STATIC, nullptr);
  REQUIRE(pData != nullptr);
  MemBlock_t* pBlock = GetBlockHeader(pData);
  REQUIRE(pBlock->type == (uint32_t)TYPE_DATA);

  const MemBlock_t* pFound = MEM_STRATEGY(pBlock).FindBlock(pBlock, name_from_cstr("anything"));
  REQUIRE_FALSE(Block_IsValid(pFound)); // kInvalidBlock, not NULL
  REQUIRE(pFound == &kInvalidBlock);

  free(pBuf);
}

/* =========================================================================
 * Test_NullStrategy_FindBlock_ReturnsInvalid
 * NullStrategy has no children — FindBlock must return &kInvalidBlock.
 * ========================================================================= */
TEST_CASE("MemStrategy – Null Strategy Find Block Returns Invalid", "[MemStrategy]") {
  MemBlock_t block = {};
  block.magic = CHECK_SUM;
  block.type  = TYPE_NONE;

  const MemBlock_t* pFound = MEM_STRATEGY(&block).FindBlock(&block, name_from_cstr("x"));
  REQUIRE_FALSE(Block_IsValid(pFound));
  REQUIRE(pFound == &kInvalidBlock);
}
