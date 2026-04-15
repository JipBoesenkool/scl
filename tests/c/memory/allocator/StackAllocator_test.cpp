#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "cLib/memory/allocator/StackAllocator.h"
#include "cLib/memory/strategy/StackStrategy.h"
#include "cLib/memory/zone/Zone.h"
#include "cLib/memory/zone/MemBlock.h"

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
TEST_CASE("StackAllocator – Malloc Sequential", "[StackAllocator]") {
  void*      pBuf   = malloc(kZoneCap);
  MemZone_t* pZone  = Z_Init((MemZone_t*)pBuf, kZoneCap, PU_STATIC, "SEQ");
  MemStack_t* pStack = MakeStack(pZone, kStackCap, "SEQ");
  REQUIRE(pStack != nullptr);

  Handle_t hA = Stack_Malloc(pStack, 16, nullptr);
  Handle_t hB = Stack_Malloc(pStack, 32, nullptr);
  Handle_t hC = Stack_Malloc(pStack,  7, nullptr);
  REQUIRE(hA.pData != nullptr);
  REQUIRE(hB.pData != nullptr);
  REQUIRE(hC.pData != nullptr);

  REQUIRE((uintptr_t)hA.pData % ALIGN_BYTES == 0);
  REQUIRE((uintptr_t)hB.pData % ALIGN_BYTES == 0);
  REQUIRE((uintptr_t)hC.pData % ALIGN_BYTES == 0);

  REQUIRE((uint8_t*)hB.pData == (uint8_t*)hA.pData + Align(16));
  REQUIRE((uint8_t*)hC.pData == (uint8_t*)hB.pData + Align(32));
  REQUIRE(pStack->used == Align(16) + Align(32) + Align(7));

  free(pBuf);
}

/* =========================================================================
 * Test_Malloc_HandleWriteback
 * When pUser is non-null, Stack_Malloc writes the address into *pUser.
 * ========================================================================= */
TEST_CASE("StackAllocator – Malloc Handle Writeback", "[StackAllocator]") {
  void*       pBuf   = malloc(kZoneCap);
  MemZone_t*  pZone  = Z_Init((MemZone_t*)pBuf, kZoneCap, PU_STATIC, "WB");
  MemStack_t* pStack = MakeStack(pZone, kStackCap, "WB");
  REQUIRE(pStack != nullptr);

  void*    pHandle = nullptr;
  Handle_t h       = Stack_Malloc(pStack, 64, &pHandle);
  REQUIRE(h.pData != nullptr);
  REQUIRE(pHandle == h.pData);

  free(pBuf);
}

/* =========================================================================
 * Test_Malloc_Overflow
 * Requesting more than remaining capacity returns kInvalidHandle.
 * ========================================================================= */
TEST_CASE("StackAllocator – Malloc Overflow", "[StackAllocator]") {
  void*       pBuf   = malloc(kZoneCap);
  MemZone_t*  pZone  = Z_Init((MemZone_t*)pBuf, kZoneCap, PU_STATIC, "OVF");
  MemStack_t* pStack = MakeStack(pZone, kStackCap, "OVF");
  REQUIRE(pStack != nullptr);

  Handle_t hFull  = Stack_Malloc(pStack, pStack->capacity, nullptr);
  REQUIRE(hFull.pData != nullptr);

  Handle_t hExtra = Stack_Malloc(pStack, 1, nullptr);
  REQUIRE(hExtra.pData == nullptr); // overflow → invalid handle

  free(pBuf);
}

/* =========================================================================
 * Test_Free_IsNoop
 * Stack_Free does not change `used` (stacks free via Clear only).
 * ========================================================================= */
TEST_CASE("StackAllocator – Free Is Noop", "[StackAllocator]") {
  void*       pBuf   = malloc(kZoneCap);
  MemZone_t*  pZone  = Z_Init((MemZone_t*)pBuf, kZoneCap, PU_STATIC, "NOOP");
  MemStack_t* pStack = MakeStack(pZone, kStackCap, "NOOP");
  REQUIRE(pStack != nullptr);

  Handle_t h = Stack_Malloc(pStack, 64, nullptr);
  uint32_t usedBefore = pStack->used;
  Stack_Free(pStack, &h);
  REQUIRE(pStack->used == usedBefore); // unchanged

  free(pBuf);
}

/* =========================================================================
 * Test_Clear
 * Stack_Clear resets `used` to 0 and bumps the version.
 * ========================================================================= */
TEST_CASE("StackAllocator – Clear", "[StackAllocator]") {
  void*       pBuf   = malloc(kZoneCap);
  MemZone_t*  pZone  = Z_Init((MemZone_t*)pBuf, kZoneCap, PU_STATIC, "CLR");
  MemStack_t* pStack = MakeStack(pZone, kStackCap, "CLR");
  REQUIRE(pStack != nullptr);

  uint32_t vBefore = pStack->version;
  Stack_Malloc(pStack, 256, nullptr);
  Stack_Malloc(pStack, 128, nullptr);
  REQUIRE(pStack->used > 0);

  Stack_Clear(pStack);
  REQUIRE(pStack->used == 0u);
  REQUIRE(pStack->version > vBefore); // version bumped on clear

  free(pBuf);
}

/* =========================================================================
 * Test_GetData_VersionCheck
 * Stack_GetData returns NULL and invalidates the handle when versions mismatch.
 * ========================================================================= */
TEST_CASE("StackAllocator – GetData Version Check", "[StackAllocator]") {
  void*       pBuf   = malloc(kZoneCap);
  MemZone_t*  pZone  = Z_Init((MemZone_t*)pBuf, kZoneCap, PU_STATIC, "VER");
  MemStack_t* pStack = MakeStack(pZone, kStackCap, "VER");
  REQUIRE(pStack != nullptr);

  Handle_t h = Stack_Malloc(pStack, 64, nullptr);
  REQUIRE(Stack_GetData(pStack, &h) != nullptr);

  Stack_Clear(pStack); // bumps version — handle becomes stale
  void* pResult = Stack_GetData(pStack, &h);
  REQUIRE(pResult == nullptr);        // version mismatch → NULL
  REQUIRE((h.id == kINVALID_HANDLE || h.pData == nullptr)); // handle invalidated

  free(pBuf);
}

/* =========================================================================
 * Test_GetAllocator_Functional
 * Exercises Malloc and Clear through the vtable.
 * ========================================================================= */
TEST_CASE("StackAllocator – GetAllocator Functional", "[StackAllocator]") {
  void*       pBuf   = malloc(kZoneCap);
  MemZone_t*  pZone  = Z_Init((MemZone_t*)pBuf, kZoneCap, PU_STATIC, "VTBL");
  MemStack_t* pStack = MakeStack(pZone, kStackCap, "VTBL");
  REQUIRE(pStack != nullptr);

  Allocator_t a = Stack_GetAllocator(pStack);
  REQUIRE(a.pContext == (void*)pStack);
  REQUIRE((void*)a.Malloc != nullptr);
  REQUIRE((void*)a.Clear != nullptr);
  REQUIRE((void*)a.Free != nullptr);

  Handle_t h = a.Malloc(a.pContext, 128, nullptr);
  REQUIRE(h.pData != nullptr);

  a.Clear(a.pContext);
  REQUIRE(pStack->used == 0u);

  free(pBuf);
}
