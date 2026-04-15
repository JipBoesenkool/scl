#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "cLib/memory/allocator/ZoneAllocator.h"
#include "cLib/memory/allocator/Allocator.h"
#include "cLib/memory/zone/Zone.h"
#include "cLib/memory/zone/ZoneStats.h"
#include "cLib/memory/zone/MemBlock.h"

static const uint32_t kCapacity = 1024 * 32;

/* =========================================================================
 * Test_GetAllocator_VtableWiring
 * Every function pointer in the returned Allocator_t must be non-null and
 * pContext must equal the zone pointer.
 * ========================================================================= */
TEST_CASE("ZoneAllocator – GetAllocator Vtable Wiring", "[ZoneAllocator]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "VTBL");

  Allocator_t a = Zone_GetAllocator(pZone);

  REQUIRE(a.GetData != nullptr);
  REQUIRE(a.Malloc != nullptr);
  REQUIRE(a.Realloc != nullptr);
  REQUIRE(a.Free != nullptr);
  REQUIRE(a.Clear != nullptr);
  REQUIRE(a.pContext == (void*)pZone);

  free(pBuf);
}

/* =========================================================================
 * Test_Vtable_Malloc_Free
 * Malloc returns a valid Handle_t. Free invalidates it.
 * ========================================================================= */
TEST_CASE("ZoneAllocator – Vtable Malloc and Free", "[ZoneAllocator]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "VTB_MF");
  Allocator_t a    = Zone_GetAllocator(pZone);

  Handle_t h = a.Malloc(a.pContext, 512, nullptr);
  REQUIRE(h.pData != nullptr);
  REQUIRE(Z_Check(pZone));

  a.Free(a.pContext, &h);
  REQUIRE(h.id == kCLOSED_HANDLE); // freed handle must be invalidated
  REQUIRE(Z_Check(pZone));

  free(pBuf);
}

/* =========================================================================
 * Test_Vtable_GetData_VersionCheck
 * GetData returns the payload pointer and patches the handle on version change.
 * ========================================================================= */
TEST_CASE("ZoneAllocator – Vtable GetData Version Check", "[ZoneAllocator]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "VTB_GD");
  Allocator_t a    = Zone_GetAllocator(pZone);

  Handle_t h = a.Malloc(a.pContext, 128, nullptr);
  REQUIRE(h.pData != nullptr);

  // GetData on a fresh handle must return the payload.
  void* pData = a.GetData(a.pContext, &h);
  REQUIRE(pData == h.pData);

  // After Z_Compact the zone version changes — GetData must re-patch the handle.
  Z_Compact(pZone);
  void* pAfter = a.GetData(a.pContext, &h);
  REQUIRE(pAfter != nullptr); // still valid — block exists

  free(pBuf);
}

/* =========================================================================
 * Test_Vtable_Realloc
 * Realloc through vtable grows a block (in-place when possible).
 * ========================================================================= */
TEST_CASE("ZoneAllocator – Vtable Realloc", "[ZoneAllocator]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "VTB_RL");
  Allocator_t a    = Zone_GetAllocator(pZone);

  Handle_t h = a.Malloc(a.pContext, 128, nullptr);
  REQUIRE(h.pData != nullptr);
  strcpy((char*)h.pData, "persist");

  Handle_t h2 = a.Realloc(a.pContext, &h, 512);
  REQUIRE(h2.pData != nullptr);
  REQUIRE(strcmp((char*)h2.pData, "persist") == 0);
  REQUIRE(Z_Check(pZone));

  free(pBuf);
}

/* =========================================================================
 * Test_Vtable_Clear
 * Clear through vtable resets the zone to its initial empty state.
 * ========================================================================= */
TEST_CASE("ZoneAllocator – Vtable Clear", "[ZoneAllocator]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, kCapacity, PU_STATIC, "VTB_CLR");
  Allocator_t a    = Zone_GetAllocator(pZone);

  a.Malloc(a.pContext, 1024, nullptr);
  a.Malloc(a.pContext, 2048, nullptr);
  REQUIRE(pZone->used > kMEM_ZONE_SIZE);

  a.Clear(a.pContext);
  REQUIRE(pZone->used == kMEM_ZONE_SIZE);
  REQUIRE(Z_Check(pZone));

  free(pBuf);
}

/* =========================================================================
 * Test_Vtable_MultipleZones
 * Two independent Allocator_t values from different zones must not interfere.
 * ========================================================================= */
TEST_CASE("ZoneAllocator – Vtable Multiple Zones", "[ZoneAllocator]") {
  void*      pBufA  = malloc(kCapacity);
  void*      pBufB  = malloc(kCapacity);
  MemZone_t* pZoneA = Z_Init((MemZone_t*)pBufA, kCapacity, PU_STATIC, "ZONE_A");
  MemZone_t* pZoneB = Z_Init((MemZone_t*)pBufB, kCapacity, PU_STATIC, "ZONE_B");
  Allocator_t aA    = Zone_GetAllocator(pZoneA);
  Allocator_t aB    = Zone_GetAllocator(pZoneB);

  Handle_t hA = aA.Malloc(aA.pContext, 1024, nullptr);
  Handle_t hB = aB.Malloc(aB.pContext, 2048, nullptr);
  REQUIRE(((hA.pData != nullptr) && (hB.pData != nullptr)));
  REQUIRE(Z_Check(pZoneA));
  REQUIRE(Z_Check(pZoneB));

  aA.Free(aA.pContext, &hA);
  REQUIRE(Z_Check(pZoneA));
  REQUIRE(Z_Check(pZoneB)); // Zone B unaffected

  free(pBufA);
  free(pBufB);
}
