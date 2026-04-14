/**
 * MemStrategy_test.cpp — unit tests for memory/private/strategy/MemStrategy.c
 *
 * Covers: kMemStrategies table wiring, MEM_STRATEGY macro dispatch,
 *         and all four strategy rows (Null, Data, Zone, Stack).
 */
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
#include "../TestHelpers.h"

static const uint32_t kCapacity = 1024 * 16;

/* =========================================================================
 * Test_TableWiring
 * Every function pointer in all four rows must be non-null.
 * ========================================================================= */
void Test_TableWiring()
{
  printf("--- Test_TableWiring ---\n");
  for (int t = 0; t < 4; ++t) {
    ASSERT_NOT_NULL((void*)kMemStrategies[t].Init);
    ASSERT_NOT_NULL((void*)kMemStrategies[t].Clear);
    ASSERT_NOT_NULL((void*)kMemStrategies[t].Purge);
    ASSERT_NOT_NULL((void*)kMemStrategies[t].Check);
    ASSERT_NOT_NULL((void*)kMemStrategies[t].Remap);
    ASSERT_NOT_NULL((void*)kMemStrategies[t].Save);
    ASSERT_NOT_NULL((void*)kMemStrategies[t].Load);
    ASSERT_NOT_NULL((void*)kMemStrategies[t].GetAllocator);
    ASSERT_NOT_NULL((void*)kMemStrategies[t].GetBlockName);
    ASSERT_NOT_NULL((void*)kMemStrategies[t].FindBlock);
  }
  printf("[PASS] Test_TableWiring\n\n");
}

/* =========================================================================
 * Test_MacroDispatch
 * MEM_STRATEGY(pBlock) must select the row matching pBlock->type.
 * ========================================================================= */
void Test_MacroDispatch()
{
  printf("--- Test_MacroDispatch ---\n");
  MemBlock_t block = {};
  block.magic = CHECK_SUM;

  block.type = TYPE_DATA;
  ASSERT_EQ((void*)MEM_STRATEGY(&block).Check, (void*)kMemStrategies[TYPE_DATA].Check);

  block.type = TYPE_ZONE;
  ASSERT_EQ((void*)MEM_STRATEGY(&block).Check, (void*)kMemStrategies[TYPE_ZONE].Check);

  block.type = TYPE_STACK;
  ASSERT_EQ((void*)MEM_STRATEGY(&block).Check, (void*)kMemStrategies[TYPE_STACK].Check);

  printf("[PASS] Test_MacroDispatch\n\n");
}

/* =========================================================================
 * Test_DataStrategy_Check
 * TYPE_DATA Check always returns true — no sub-structure to validate.
 * ========================================================================= */
void Test_DataStrategy_Check()
{
  printf("--- Test_DataStrategy_Check ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "DS_CHK");

  void* pData = Z_Malloc(pZone, 256, PU_STATIC, nullptr);
  ASSERT_NOT_NULL(pData);
  MemBlock_t* pBlock = GetBlockHeader(pData);
  ASSERT_EQ(pBlock->type, (uint32_t)TYPE_DATA);
  ASSERT(MEM_STRATEGY(pBlock).Check(pBlock));

  free(pBuf);
  printf("[PASS] Test_DataStrategy_Check\n\n");
}

/* =========================================================================
 * Test_DataStrategy_Remap
 * TYPE_DATA Remap is a no-op — must not crash and must not modify the block.
 * ========================================================================= */
void Test_DataStrategy_Remap()
{
  printf("--- Test_DataStrategy_Remap ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "DS_REMAP");

  void* pData = Z_Malloc(pZone, 128, PU_STATIC, nullptr);
  MemBlock_t* pBlock = GetBlockHeader(pData);
  uint32_t sizeBefore = pBlock->size;
  uint32_t tagBefore  = pBlock->tag;

  MEM_STRATEGY(pBlock).Remap(pBlock); // no-op, must not crash

  ASSERT_EQ(pBlock->size, sizeBefore);
  ASSERT_EQ(pBlock->tag,  tagBefore);

  free(pBuf);
  printf("[PASS] Test_DataStrategy_Remap\n\n");
}

/* =========================================================================
 * Test_DataStrategy_Purge
 * TYPE_DATA Purge is a no-op — must not crash.
 * ========================================================================= */
void Test_DataStrategy_Purge()
{
  printf("--- Test_DataStrategy_Purge ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "DS_PURGE");

  void* pData = Z_Malloc(pZone, 64, PU_STATIC, nullptr);
  MemBlock_t* pBlock = GetBlockHeader(pData);

  MEM_STRATEGY(pBlock).Purge(pBlock); // no-op, must not crash
  ASSERT(pBlock->magic == CHECK_SUM); // block untouched

  free(pBuf);
  printf("[PASS] Test_DataStrategy_Purge\n\n");
}

/* =========================================================================
 * Test_StackStrategy_Init_Check
 * StackStrategy_Init sets up the embedded MemStack_t.
 * StackStrategy_Check returns true for a healthy stack.
 * ========================================================================= */
void Test_StackStrategy_Init_Check()
{
  printf("--- Test_StackStrategy_Init_Check ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "SK_CHK");

  const uint32_t kStackTotal = 1024;
  void* pStackMem = Z_Malloc(pZone, kStackTotal, PU_STATIC, nullptr);
  ASSERT_NOT_NULL(pStackMem);

  MemBlock_t* pBlock = GetBlockHeader(pStackMem);
  pBlock->type = TYPE_STACK;

  MEM_STRATEGY(pBlock).Init(pBlock, kStackTotal, PU_STATIC, "SUB");
  ASSERT(MEM_STRATEGY(pBlock).Check(pBlock)); // freshly initialised → valid

  // Corrupt used counter so it exceeds capacity.
  MemStack_t* pStack = (MemStack_t*)GetBlockData(pBlock);
  uint32_t saved = pStack->used;
  pStack->used = pStack->capacity + 1;
  ASSERT(!MEM_STRATEGY(pBlock).Check(pBlock)); // overflow → invalid

  pStack->used = saved;
  free(pBuf);
  printf("[PASS] Test_StackStrategy_Init_Check\n\n");
}

/* =========================================================================
 * Test_GetBlockName
 * TYPE_DATA / TYPE_NONE blocks return NULL (no embedded name).
 * TYPE_ZONE blocks return the name passed to Init.
 * TYPE_STACK blocks return the name passed to Init.
 * ========================================================================= */
void Test_GetBlockName()
{
  printf("--- Test_GetBlockName ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "ROOT");

  // TYPE_DATA block — no name
  void* pData = Z_Malloc(pZone, 128, PU_STATIC, nullptr);
  ASSERT_NOT_NULL(pData);
  MemBlock_t* pDataBlk = GetBlockHeader(pData);
  ASSERT_EQ(MEM_STRATEGY(pDataBlk).GetBlockName(pDataBlk), (const char*)nullptr);

  // TYPE_ZONE block — returns the sub-zone's name
  MemZone_t* pSub = NULL;
  void* pSubMem = Z_Malloc(pZone, 1024 * 4, PU_STATIC, &pSub);
  ASSERT_NOT_NULL(pSubMem);
  MemBlock_t* pZoneBlk = GetBlockHeader(pSubMem);
  pZoneBlk->type = TYPE_ZONE;
  MEM_STRATEGY(pZoneBlk).Init(pZoneBlk, 0, PU_STATIC, "SUBZONE");
  const char* zoneName = MEM_STRATEGY(pZoneBlk).GetBlockName(pZoneBlk);
  ASSERT_NOT_NULL(zoneName);
  ASSERT_EQ(strcmp(zoneName, "SUBZONE"), 0);

  // TYPE_STACK block — returns the sub-stack's name
  void* pStkMem = Z_Malloc(pZone, 1024 * 2, PU_STATIC, nullptr);
  ASSERT_NOT_NULL(pStkMem);
  MemBlock_t* pStkBlk = GetBlockHeader(pStkMem);
  pStkBlk->type = TYPE_STACK;
  MEM_STRATEGY(pStkBlk).Init(pStkBlk, 0, PU_STATIC, "MYSTACK");
  const char* stkName = MEM_STRATEGY(pStkBlk).GetBlockName(pStkBlk);
  ASSERT_NOT_NULL(stkName);
  ASSERT_EQ(strcmp(stkName, "MYSTACK"), 0);

  free(pBuf);
  printf("[PASS] Test_GetBlockName\n\n");
}

/* =========================================================================
 * Test_ZoneStrategy_Init
 * ZoneStrategy_Init must initialise a valid MemZone_t in the block payload.
 * The parent block header (magic, next, prev, ppUser) must be preserved.
 * ========================================================================= */
void Test_ZoneStrategy_Init()
{
  printf("--- Test_ZoneStrategy_Init ---\n");
  void*      pBuf    = malloc(kCapacity);
  MemZone_t* pParent = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "PAR_INIT");

  const uint32_t kSubSize = 1024 * 4;
  MemZone_t* pSub = NULL;
  void* pMem = Z_Malloc(pParent, kSubSize, PU_STATIC, &pSub);
  ASSERT_NOT_NULL(pMem);

  MemBlock_t* pBlock = GetBlockHeader(pMem);
  uint32_t savedNext = pBlock->next;
  uint32_t savedPrev = pBlock->prev;
  pBlock->type = TYPE_ZONE;

  MEM_STRATEGY(pBlock).Init(pBlock, 0, PU_STATIC, "SUBINIT");

  /* Parent block header must be intact */
  ASSERT_EQ(pBlock->magic, (uint32_t)CHECK_SUM);
  ASSERT_EQ(pBlock->next,  savedNext);
  ASSERT_EQ(pBlock->prev,  savedPrev);

  /* Sub-zone at payload must be valid */
  MemZone_t* pZ = (MemZone_t*)GetBlockData(pBlock);
  ASSERT(Z_Check(pZ));
  ASSERT(pZ->capacity > 0);

  free(pBuf);
  printf("[PASS] Test_ZoneStrategy_Init\n\n");
}

/* =========================================================================
 * Test_ZoneStrategy_Remap_Check
 * TYPE_ZONE Remap and Check operate on the embedded sub-zone.
 * ========================================================================= */
void Test_ZoneStrategy_Remap_Check()
{
  printf("--- Test_ZoneStrategy_Remap_Check ---\n");
  void*      pBuf    = malloc(kCapacity);
  MemZone_t* pParent = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "PAR");

  MemZone_t* pSub = NULL;
  void* pMem = Z_Malloc(pParent, 1024 * 4, PU_STATIC, &pSub);
  ASSERT_NOT_NULL(pMem);
  MemBlock_t* pBlkParent = GetBlockHeader(pMem);
  pBlkParent->type = TYPE_ZONE;
  MEM_STRATEGY(pBlkParent).Init(pBlkParent, 0, PU_STATIC, "SUB");
  ASSERT_NOT_NULL(pSub);

  MemBlock_t* pBlk = Z_ZoneAsBlock(pSub);
  ASSERT_EQ(pBlk->type, (uint32_t)TYPE_ZONE);

  ASSERT(MEM_STRATEGY(pBlk).Check(pBlk)); // valid sub-zone → true
  MEM_STRATEGY(pBlk).Remap(pBlk);         // must not crash on a valid zone

  free(pBuf);
  printf("[PASS] Test_ZoneStrategy_Remap_Check\n\n");
}

/* =========================================================================
 * Test_ZoneStrategy_GetAllocator
 * GetAllocator on a TYPE_ZONE block returns a wired Allocator_t whose
 * pContext is the embedded MemZone_t.
 * ========================================================================= */
void Test_ZoneStrategy_GetAllocator()
{
  printf("--- Test_ZoneStrategy_GetAllocator ---\n");
  void*      pBuf    = malloc(kCapacity);
  MemZone_t* pParent = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "PAR_GA");

  MemZone_t* pSub = NULL;
  void* pMemGA = Z_Malloc(pParent, 1024 * 4, PU_STATIC, &pSub);
  ASSERT_NOT_NULL(pMemGA);
  MemBlock_t* pBlkGA = GetBlockHeader(pMemGA);
  pBlkGA->type = TYPE_ZONE;
  MEM_STRATEGY(pBlkGA).Init(pBlkGA, 0, PU_STATIC, "SUB_GA");
  ASSERT_NOT_NULL(pSub);

  MemBlock_t* pBlk  = Z_ZoneAsBlock(pSub);
  Allocator_t alloc = MEM_STRATEGY(pBlk).GetAllocator(pBlk);
  ASSERT(alloc.pContext == pSub);
  ASSERT_NOT_NULL((void*)alloc.Malloc);

  Handle_t h = alloc.Malloc(alloc.pContext, 64, nullptr);
  ASSERT_NOT_NULL(h.pData);

  free(pBuf);
  printf("[PASS] Test_ZoneStrategy_GetAllocator\n\n");
}

/* =========================================================================
 * Test_DataStrategy_GetAllocator_IsNullObject
 * GetAllocator on a TYPE_DATA block returns a safe NullObject allocator.
 * Calling Malloc on it must not crash — returns kInvalidHandle.
 * ========================================================================= */
void Test_DataStrategy_GetAllocator_IsNullObject()
{
  printf("--- Test_DataStrategy_GetAllocator_IsNullObject ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "DS_NULL");

  void* pData = Z_Malloc(pZone, 64, PU_STATIC, nullptr);
  ASSERT_NOT_NULL(pData);
  MemBlock_t* pBlock = GetBlockHeader(pData);
  ASSERT_EQ(pBlock->type, (uint32_t)TYPE_DATA);

  Allocator_t alloc = MEM_STRATEGY(pBlock).GetAllocator(pBlock);
  // Must be callable — NullObject, not a crash
  Handle_t h = alloc.Malloc(alloc.pContext, 64, nullptr);
  ASSERT_EQ(h.id, kINVALID_HANDLE);  // logged warning, returned invalid handle

  free(pBuf);
  printf("[PASS] Test_DataStrategy_GetAllocator_IsNullObject\n\n");
}

/* =========================================================================
 * Test_NullStrategy_GetAllocator_IsNullObject
 * GetAllocator on a TYPE_NONE block returns a safe NullObject allocator.
 * ========================================================================= */
void Test_NullStrategy_GetAllocator_IsNullObject()
{
  printf("--- Test_NullStrategy_GetAllocator_IsNullObject ---\n");
  MemBlock_t block = {};
  block.magic = CHECK_SUM;
  block.type  = TYPE_NONE;

  Allocator_t alloc = MEM_STRATEGY(&block).GetAllocator(&block);
  Handle_t h = alloc.Malloc(alloc.pContext, 32, nullptr);
  ASSERT_EQ(h.id, kINVALID_HANDLE);

  printf("[PASS] Test_NullStrategy_GetAllocator_IsNullObject\n\n");
}

/* =========================================================================
 * Test_DataStrategy_FindBlock_ReturnsInvalid
 * DataStrategy has no children — FindBlock must return &kInvalidBlock, not NULL.
 * ========================================================================= */
void Test_DataStrategy_FindBlock_ReturnsInvalid()
{
  printf("--- Test_DataStrategy_FindBlock_ReturnsInvalid ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "FB_DATA");

  void* pData = Z_Malloc(pZone, 64, PU_STATIC, nullptr);
  ASSERT_NOT_NULL(pData);
  MemBlock_t* pBlock = GetBlockHeader(pData);
  ASSERT_EQ(pBlock->type, (uint32_t)TYPE_DATA);

  const MemBlock_t* pFound = MEM_STRATEGY(pBlock).FindBlock(pBlock, name_from_cstr("anything"));
  ASSERT(!Block_IsValid(pFound)); // kInvalidBlock, not NULL
  ASSERT(pFound == &kInvalidBlock);

  free(pBuf);
  printf("[PASS] Test_DataStrategy_FindBlock_ReturnsInvalid\n\n");
}

/* =========================================================================
 * Test_NullStrategy_FindBlock_ReturnsInvalid
 * NullStrategy has no children — FindBlock must return &kInvalidBlock.
 * ========================================================================= */
void Test_NullStrategy_FindBlock_ReturnsInvalid()
{
  printf("--- Test_NullStrategy_FindBlock_ReturnsInvalid ---\n");
  MemBlock_t block = {};
  block.magic = CHECK_SUM;
  block.type  = TYPE_NONE;

  const MemBlock_t* pFound = MEM_STRATEGY(&block).FindBlock(&block, name_from_cstr("x"));
  ASSERT(!Block_IsValid(pFound));
  ASSERT(pFound == &kInvalidBlock);

  printf("[PASS] Test_NullStrategy_FindBlock_ReturnsInvalid\n\n");
}

/* =========================================================================
 * main
 * ========================================================================= */
int main()
{
  Test_TableWiring();
  Test_MacroDispatch();
  Test_DataStrategy_Check();
  Test_DataStrategy_Remap();
  Test_DataStrategy_Purge();
  Test_StackStrategy_Init_Check();
  Test_GetBlockName();
  Test_ZoneStrategy_Init();
  Test_ZoneStrategy_Remap_Check();
  Test_ZoneStrategy_GetAllocator();
  Test_DataStrategy_GetAllocator_IsNullObject();
  Test_NullStrategy_GetAllocator_IsNullObject();
  Test_DataStrategy_FindBlock_ReturnsInvalid();
  Test_NullStrategy_FindBlock_ReturnsInvalid();

  printf("--- ALL MEM STRATEGY TESTS PASSED ---\n");
  return 0;
}
