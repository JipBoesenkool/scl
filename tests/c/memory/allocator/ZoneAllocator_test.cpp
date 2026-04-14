/**
 * ZoneAllocator_test.cpp — unit tests for memory/private/allocator/ZoneAllocator.c
 *
 * Covers: Zone_GetAllocator vtable wiring and every Allocator_t operation:
 *         GetData, Malloc, Realloc, Free, Clear.
 *
 * Note: Check/Purge/Remap are on MemStrategy_t, not Allocator_t.
 *       Zone-level Z_Check is exercised in Zone_test.cpp.
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "cLib/memory/allocator/ZoneAllocator.h"
#include "cLib/memory/allocator/Allocator.h"
#include "cLib/memory/zone/Zone.h"
#include "cLib/memory/zone/ZoneStats.h"
#include "cLib/memory/zone/MemBlock.h"
#include "../TestHelpers.h"

static const uint32_t kCapacity = 1024 * 32;

/* =========================================================================
 * Test_GetAllocator_VtableWiring
 * Every function pointer in the returned Allocator_t must be non-null and
 * pContext must equal the zone pointer.
 * ========================================================================= */
void Test_GetAllocator_VtableWiring()
{
  printf("--- Test_GetAllocator_VtableWiring ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "VTBL");

  Allocator_t a = Zone_GetAllocator(pZone);

  ASSERT_NOT_NULL((void*)a.GetData);
  ASSERT_NOT_NULL((void*)a.Malloc);
  ASSERT_NOT_NULL((void*)a.Realloc);
  ASSERT_NOT_NULL((void*)a.Free);
  ASSERT_NOT_NULL((void*)a.Clear);
  ASSERT_EQ(a.pContext, (void*)pZone);

  free(pBuf);
  printf("[PASS] Test_GetAllocator_VtableWiring\n\n");
}

/* =========================================================================
 * Test_Vtable_Malloc_Free
 * Malloc returns a valid Handle_t. Free invalidates it.
 * ========================================================================= */
void Test_Vtable_Malloc_Free()
{
  printf("--- Test_Vtable_Malloc_Free ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "VTB_MF");
  Allocator_t a    = Zone_GetAllocator(pZone);

  Handle_t h = a.Malloc(a.pContext, 512, nullptr);
  ASSERT_NOT_NULL(h.pData);
  ASSERT(Z_Check(pZone));

  a.Free(a.pContext, &h);
  ASSERT(h.id == kINVALID_HANDLE); // freed handle must be invalidated
  ASSERT(Z_Check(pZone));

  free(pBuf);
  printf("[PASS] Test_Vtable_Malloc_Free\n\n");
}

/* =========================================================================
 * Test_Vtable_GetData_VersionCheck
 * GetData returns the payload pointer and patches the handle on version change.
 * ========================================================================= */
void Test_Vtable_GetData_VersionCheck()
{
  printf("--- Test_Vtable_GetData_VersionCheck ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "VTB_GD");
  Allocator_t a    = Zone_GetAllocator(pZone);

  Handle_t h = a.Malloc(a.pContext, 128, nullptr);
  ASSERT_NOT_NULL(h.pData);

  // GetData on a fresh handle must return the payload.
  void* pData = a.GetData(a.pContext, &h);
  ASSERT_EQ(pData, h.pData);

  // After Z_Compact the zone version changes — GetData must re-patch the handle.
  Z_Compact(pZone);
  void* pAfter = a.GetData(a.pContext, &h);
  ASSERT_NOT_NULL(pAfter); // still valid — block exists

  free(pBuf);
  printf("[PASS] Test_Vtable_GetData_VersionCheck\n\n");
}

/* =========================================================================
 * Test_Vtable_Realloc
 * Realloc through vtable grows a block (in-place when possible).
 * ========================================================================= */
void Test_Vtable_Realloc()
{
  printf("--- Test_Vtable_Realloc ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "VTB_RL");
  Allocator_t a    = Zone_GetAllocator(pZone);

  Handle_t h = a.Malloc(a.pContext, 128, nullptr);
  ASSERT_NOT_NULL(h.pData);
  strcpy((char*)h.pData, "persist");

  Handle_t h2 = a.Realloc(a.pContext, &h, 512);
  ASSERT_NOT_NULL(h2.pData);
  ASSERT(strcmp((char*)h2.pData, "persist") == 0);
  ASSERT(Z_Check(pZone));

  free(pBuf);
  printf("[PASS] Test_Vtable_Realloc\n\n");
}

/* =========================================================================
 * Test_Vtable_Clear
 * Clear through vtable resets the zone to its initial empty state.
 * ========================================================================= */
void Test_Vtable_Clear()
{
  printf("--- Test_Vtable_Clear ---\n");
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "VTB_CLR");
  Allocator_t a    = Zone_GetAllocator(pZone);

  a.Malloc(a.pContext, 1024, nullptr);
  a.Malloc(a.pContext, 2048, nullptr);
  ASSERT(pZone->used > kMEM_ZONE_SIZE);

  a.Clear(a.pContext);
  ASSERT_EQ(pZone->used, kMEM_ZONE_SIZE);
  ASSERT(Z_Check(pZone));

  free(pBuf);
  printf("[PASS] Test_Vtable_Clear\n\n");
}

/* =========================================================================
 * Test_Vtable_MultipleZones
 * Two independent Allocator_t values from different zones must not interfere.
 * ========================================================================= */
void Test_Vtable_MultipleZones()
{
  printf("--- Test_Vtable_MultipleZones ---\n");
  void*      pBufA  = malloc(kCapacity);
  void*      pBufB  = malloc(kCapacity);
  MemZone_t* pZoneA = Z_Init((MemZone_t*)pBufA, kCapacity, PU_STATIC, "ZONE_A");
  MemZone_t* pZoneB = Z_Init((MemZone_t*)pBufB, kCapacity, PU_STATIC, "ZONE_B");
  Allocator_t aA    = Zone_GetAllocator(pZoneA);
  Allocator_t aB    = Zone_GetAllocator(pZoneB);

  Handle_t hA = aA.Malloc(aA.pContext, 1024, nullptr);
  Handle_t hB = aB.Malloc(aB.pContext, 2048, nullptr);
  ASSERT(hA.pData && hB.pData);
  ASSERT(Z_Check(pZoneA));
  ASSERT(Z_Check(pZoneB));

  aA.Free(aA.pContext, &hA);
  ASSERT(Z_Check(pZoneA));
  ASSERT(Z_Check(pZoneB)); // Zone B unaffected

  free(pBufA);
  free(pBufB);
  printf("[PASS] Test_Vtable_MultipleZones\n\n");
}

/* =========================================================================
 * main
 * ========================================================================= */
int main()
{
  Test_GetAllocator_VtableWiring();
  Test_Vtable_Malloc_Free();
  Test_Vtable_GetData_VersionCheck();
  Test_Vtable_Realloc();
  Test_Vtable_Clear();
  Test_Vtable_MultipleZones();

  printf("--- ALL ZONE ALLOCATOR TESTS PASSED ---\n");
  return 0;
}
