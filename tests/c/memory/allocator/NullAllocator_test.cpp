#include <catch2/catch_test_macros.hpp>
#include "cLib/memory/allocator/NullAllocator.h"
#include "cLib/memory/allocator/Allocator.h"
#include "cLib/memory/allocator/ZoneAllocator.h"
#include "cLib/memory/zone/Zone.h"
#include "cLib/memory/zone/Handle.h"

TEST_CASE("INVALID_ALLOCATOR is callable and does not crash", "[allocator][null]") {
  Handle_t h = INVALID_ALLOCATOR.Malloc(INVALID_ALLOCATOR.pContext, 16, nullptr);
  CHECK(h.id == kINVALID_HANDLE);
}

TEST_CASE("Allocator_IsValid returns false for INVALID_ALLOCATOR", "[allocator][null]") {
  CHECK_FALSE(Allocator_IsValid(INVALID_ALLOCATOR));
}

TEST_CASE("Allocator_IsValid returns true for a zone allocator", "[allocator][null]") {
  static uint8_t mem[1024];
  MemZone_t* pZone = Z_Init((MemZone_t*)mem, sizeof(mem), PU_STATIC, "test");
  CHECK(Allocator_IsValid(Zone_GetAllocator(pZone)));
}

TEST_CASE("Null_GetAllocator with NULL pContext produces INVALID_ALLOCATOR equivalent", "[allocator][null]") {
  Allocator_t a = Null_GetAllocator(nullptr);
  CHECK_FALSE(Allocator_IsValid(a));
  Handle_t h = a.Malloc(a.pContext, 8, nullptr);
  CHECK(h.id == kINVALID_HANDLE);
}
