/**
 * StackAllocator_test.cpp — unit tests for memory/private/allocator/StackAllocator.c
 *
 * Covers: Stack_Malloc, Stack_Realloc, Stack_Free, Stack_Clear,
 *         Stack_GetData, Stack_GetAllocator, and vtable dispatch.
 *
 * Setup: allocate a zone, carve a block, stamp TYPE_STACK, initialise via
 *        StackStrategy_Init (Stack_Init lives in StackStrategy now).
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "cLib/memory/allocator/StackAllocator.h"
#include "cLib/memory/strategy/StackStrategy.h"
#include "cLib/memory/zone/Zone.h"
#include "cLib/memory/zone/MemBlock.h"
#include "../TestHelpers.h"

static const uint32_t kZoneCap  = 1024 * 32;
static const uint32_t kStackCap = 1024 * 4;

/* Helper: carve a properly-initialised MemStack_t from a parent zone. */
static MemStack_t* MakeStack(MemZone_t* pZone, uint32_t capacity, const char* name)
{
  void* pMem = Z_Malloc(pZone, capacity, PU_STATIC, nullptr);
  if (!pMem) return nullptr;
  MemBlock_t* pBlk = GetBlockHeader(pMem);
  pBlk->type = TYPE_STACK;
  StackStrategy_Init(pBlk, capacity, PU_STATIC, name);
  return (MemStack_t*)GetBlockData(pBlk);
}

/* =========================================================================
 * Test_Malloc_Sequential
 * Each allocation bumps `used` by the aligned request size.
 * Returned Handle_t::pData must be 8-byte aligned and non-overlapping.
 * ========================================================================= */
void Test_Malloc_Sequential()
{
  printf("--- Test_Malloc_Sequential ---\n");
  void*      pBuf   = malloc(kZoneCap);
  MemZone_t* pZone  = Z_Init((MemZone_t*)pBuf, kZoneCap, PU_STATIC, "SEQ");
  MemStack_t* pStack = MakeStack(pZone, kStackCap, "SEQ");
  ASSERT_NOT_NULL(pStack);

  Handle_t hA = Stack_Malloc(pStack, 16, nullptr);
  Handle_t hB = Stack_Malloc(pStack, 32, nullptr);
  Handle_t hC = Stack_Malloc(pStack,  7, nullptr);
  ASSERT_NOT_NULL(hA.pData);
  ASSERT_NOT_NULL(hB.pData);
  ASSERT_NOT_NULL(hC.pData);

  ASSERT((uintptr_t)hA.pData % ALIGN_BYTES == 0);
  ASSERT((uintptr_t)hB.pData % ALIGN_BYTES == 0);
  ASSERT((uintptr_t)hC.pData % ALIGN_BYTES == 0);

  ASSERT_EQ((uint8_t*)hB.pData, (uint8_t*)hA.pData + Align(16));
  ASSERT_EQ((uint8_t*)hC.pData, (uint8_t*)hB.pData + Align(32));
  ASSERT_EQ(pStack->used, Align(16) + Align(32) + Align(7));

  free(pBuf);
  printf("[PASS] Test_Malloc_Sequential\n\n");
}

/* =========================================================================
 * Test_Malloc_HandleWriteback
 * When pUser is non-null, Stack_Malloc writes the address into *pUser.
 * ========================================================================= */
void Test_Malloc_HandleWriteback()
{
  printf("--- Test_Malloc_HandleWriteback ---\n");
  void*       pBuf   = malloc(kZoneCap);
  MemZone_t*  pZone  = Z_Init((MemZone_t*)pBuf, kZoneCap, PU_STATIC, "WB");
  MemStack_t* pStack = MakeStack(pZone, kStackCap, "WB");
  ASSERT_NOT_NULL(pStack);

  void*    pHandle = nullptr;
  Handle_t h       = Stack_Malloc(pStack, 64, &pHandle);
  ASSERT_NOT_NULL(h.pData);
  ASSERT_EQ(pHandle, h.pData);

  free(pBuf);
  printf("[PASS] Test_Malloc_HandleWriteback\n\n");
}

/* =========================================================================
 * Test_Malloc_Overflow
 * Requesting more than remaining capacity returns kInvalidHandle.
 * ========================================================================= */
void Test_Malloc_Overflow()
{
  printf("--- Test_Malloc_Overflow ---\n");
  void*       pBuf   = malloc(kZoneCap);
  MemZone_t*  pZone  = Z_Init((MemZone_t*)pBuf, kZoneCap, PU_STATIC, "OVF");
  MemStack_t* pStack = MakeStack(pZone, kStackCap, "OVF");
  ASSERT_NOT_NULL(pStack);

  Handle_t hFull  = Stack_Malloc(pStack, pStack->capacity, nullptr);
  ASSERT_NOT_NULL(hFull.pData);

  Handle_t hExtra = Stack_Malloc(pStack, 1, nullptr);
  ASSERT_NULL(hExtra.pData); // overflow → invalid handle

  free(pBuf);
  printf("[PASS] Test_Malloc_Overflow\n\n");
}

/* =========================================================================
 * Test_Free_IsNoop
 * Stack_Free does not change `used` (stacks free via Clear only).
 * ========================================================================= */
void Test_Free_IsNoop()
{
  printf("--- Test_Free_IsNoop ---\n");
  void*       pBuf   = malloc(kZoneCap);
  MemZone_t*  pZone  = Z_Init((MemZone_t*)pBuf, kZoneCap, PU_STATIC, "NOOP");
  MemStack_t* pStack = MakeStack(pZone, kStackCap, "NOOP");
  ASSERT_NOT_NULL(pStack);

  Handle_t h = Stack_Malloc(pStack, 64, nullptr);
  uint32_t usedBefore = pStack->used;
  Stack_Free(pStack, &h);
  ASSERT_EQ(pStack->used, usedBefore); // unchanged

  free(pBuf);
  printf("[PASS] Test_Free_IsNoop\n\n");
}

/* =========================================================================
 * Test_Clear
 * Stack_Clear resets `used` to 0 and bumps the version.
 * ========================================================================= */
void Test_Clear()
{
  printf("--- Test_Clear ---\n");
  void*       pBuf   = malloc(kZoneCap);
  MemZone_t*  pZone  = Z_Init((MemZone_t*)pBuf, kZoneCap, PU_STATIC, "CLR");
  MemStack_t* pStack = MakeStack(pZone, kStackCap, "CLR");
  ASSERT_NOT_NULL(pStack);

  uint32_t vBefore = pStack->version;
  Stack_Malloc(pStack, 256, nullptr);
  Stack_Malloc(pStack, 128, nullptr);
  ASSERT(pStack->used > 0);

  Stack_Clear(pStack);
  ASSERT_EQ(pStack->used, 0u);
  ASSERT(pStack->version > vBefore); // version bumped on clear

  free(pBuf);
  printf("[PASS] Test_Clear\n\n");
}

/* =========================================================================
 * Test_GetData_VersionCheck
 * Stack_GetData returns NULL and invalidates the handle when versions mismatch.
 * ========================================================================= */
void Test_GetData_VersionCheck()
{
  printf("--- Test_GetData_VersionCheck ---\n");
  void*       pBuf   = malloc(kZoneCap);
  MemZone_t*  pZone  = Z_Init((MemZone_t*)pBuf, kZoneCap, PU_STATIC, "VER");
  MemStack_t* pStack = MakeStack(pZone, kStackCap, "VER");
  ASSERT_NOT_NULL(pStack);

  Handle_t h = Stack_Malloc(pStack, 64, nullptr);
  ASSERT_NOT_NULL(Stack_GetData(pStack, &h));

  Stack_Clear(pStack); // bumps version — handle becomes stale
  void* pResult = Stack_GetData(pStack, &h);
  ASSERT_NULL(pResult);        // version mismatch → NULL
  ASSERT(h.id == kINVALID_HANDLE || h.pData == nullptr); // handle invalidated

  free(pBuf);
  printf("[PASS] Test_GetData_VersionCheck\n\n");
}

/* =========================================================================
 * Test_GetAllocator_Functional
 * Exercises Malloc and Clear through the vtable.
 * ========================================================================= */
void Test_GetAllocator_Functional()
{
  printf("--- Test_GetAllocator_Functional ---\n");
  void*       pBuf   = malloc(kZoneCap);
  MemZone_t*  pZone  = Z_Init((MemZone_t*)pBuf, kZoneCap, PU_STATIC, "VTBL");
  MemStack_t* pStack = MakeStack(pZone, kStackCap, "VTBL");
  ASSERT_NOT_NULL(pStack);

  Allocator_t a = Stack_GetAllocator(pStack);
  ASSERT_EQ(a.pContext, (void*)pStack);
  ASSERT_NOT_NULL((void*)a.Malloc);
  ASSERT_NOT_NULL((void*)a.Clear);
  ASSERT_NOT_NULL((void*)a.Free);

  Handle_t h = a.Malloc(a.pContext, 128, nullptr);
  ASSERT_NOT_NULL(h.pData);

  a.Clear(a.pContext);
  ASSERT_EQ(pStack->used, 0u);

  free(pBuf);
  printf("[PASS] Test_GetAllocator_Functional\n\n");
}

/* =========================================================================
 * main
 * ========================================================================= */
int main()
{
  Test_Malloc_Sequential();
  Test_Malloc_HandleWriteback();
  Test_Malloc_Overflow();
  Test_Free_IsNoop();
  Test_Clear();
  Test_GetData_VersionCheck();
  Test_GetAllocator_Functional();

  printf("--- ALL STACK ALLOCATOR TESTS PASSED ---\n");
  return 0;
}
