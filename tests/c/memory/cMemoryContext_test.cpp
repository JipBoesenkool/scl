#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "cLib/memory/cMemoryContext.h"
#include "cLib/memory/zone/Zone.h"
#include "cLib/memory/zone/MemBlock.h"
#include "cLib/memory/strategy/MemStrategy.h"
#include "cLib/memory/allocator/NullAllocator.h"
#include "cLib/string/cName.h"

static const uint32_t kCapacity = 1024 * 16;

/* -------------------------------------------------------------------------
 * MakeSubZone — identical helper to Zone_test.cpp.
 * Allocates a TYPE_ZONE block inside pParent, names it, and returns the
 * embedded MemZone_t*.
 * ------------------------------------------------------------------------- */
static MemZone_t* MakeSubZone(MemZone_t* pParent, uint32_t size, uint32_t tag,
                               const char* name, MemZone_t** ppSub)
{
  void* pMem = Z_Malloc(pParent, size, tag, ppSub);
  if (!pMem) return NULL;
  MemBlock_t* pBlock = GetBlockHeader(pMem);
  pBlock->type = TYPE_ZONE;
  MEM_STRATEGY(pBlock).Init(pBlock, 0, tag, name);
  return *ppSub;
}

/* =========================================================================
 * Test_NoContext
 * With no context set, returns INVALID_ALLOCATOR — safe to inspect,
 * Allocator_IsValid returns false.
 * ========================================================================= */
TEST_CASE("MemoryContext – No Context", "[MemoryContext]") {
  /* Ensure no context is active by pushing NULL-block context and popping it. */
  Allocator_t result = context_get_allocator_by_name(name_from_cstr("anything"));
  REQUIRE(!Allocator_IsValid(result));
}

/* =========================================================================
 * Test_DirectMatch
 * Root context has a TYPE_ZONE child named "strings".
 * context_get_allocator_by_name("strings") returns a valid allocator.
 * ========================================================================= */
TEST_CASE("MemoryContext – Direct Match", "[MemoryContext]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init(pBuf, kCapacity, PU_STATIC, "ROOT");

  MemZone_t* pStrings = NULL;
  MakeSubZone(pZone, 2048, PU_STATIC, "strings", &pStrings);
  REQUIRE(pStrings != NULL);

  /* Wire a root context pointing at the ROOT zone's anchor block. */
  RuntimeContext_t ctx = { Z_ZoneAsBlock(pZone), NULL };
  context_set_root(&ctx);

  Allocator_t result = context_get_allocator_by_name(name_from_cstr("strings"));
  REQUIRE(Allocator_IsValid(result));

  free(pBuf);
}

/* =========================================================================
 * Test_MissingName
 * Root context has a child named "other", not "strings".
 * Returns INVALID_ALLOCATOR — no crash.
 * ========================================================================= */
TEST_CASE("MemoryContext – Missing Name", "[MemoryContext]") {
  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init(pBuf, kCapacity, PU_STATIC, "ROOT");

  MemZone_t* pOther = NULL;
  MakeSubZone(pZone, 2048, PU_STATIC, "other", &pOther);

  RuntimeContext_t ctx = { Z_ZoneAsBlock(pZone), NULL };
  context_set_root(&ctx);

  Allocator_t result = context_get_allocator_by_name(name_from_cstr("strings"));
  REQUIRE(!Allocator_IsValid(result));

  free(pBuf);
}

/* =========================================================================
 * Test_ParentClimb
 * Child context has no match; parent context does.
 * context_get_allocator_by_name climbs to the parent and returns its allocator.
 * ========================================================================= */
TEST_CASE("MemoryContext – Parent Climb", "[MemoryContext]") {
  void*      pRootBuf  = malloc(kCapacity);
  MemZone_t* pRootZone = Z_Init(pRootBuf, kCapacity, PU_STATIC, "ROOT");

  MemZone_t* pStrings = NULL;
  MakeSubZone(pRootZone, 2048, PU_STATIC, "strings", &pStrings);

  void*      pChildBuf  = malloc(kCapacity);
  MemZone_t* pChildZone = Z_Init(pChildBuf, kCapacity, PU_STATIC, "CHILD");
  /* Child zone has no "strings" sub-zone */

  RuntimeContext_t rootCtx  = { Z_ZoneAsBlock(pRootZone),  NULL };
  RuntimeContext_t childCtx = { Z_ZoneAsBlock(pChildZone), NULL };
  context_set_root(&rootCtx);
  context_push(&childCtx);

  Allocator_t result = context_get_allocator_by_name(name_from_cstr("strings"));
  REQUIRE(Allocator_IsValid(result));

  context_pop();
  free(pRootBuf);
  free(pChildBuf);
}

/* =========================================================================
 * Test_InnermostWins
 * Both child and parent have a "strings" sub-zone.
 * The child allocator (innermost scope) must be returned, not the parent's.
 * ========================================================================= */
TEST_CASE("MemoryContext – Innermost Wins", "[MemoryContext]") {
  void*      pRootBuf  = malloc(kCapacity);
  MemZone_t* pRootZone = Z_Init(pRootBuf, kCapacity, PU_STATIC, "ROOT");

  MemZone_t* pRootStrings = NULL;
  MakeSubZone(pRootZone, 1024, PU_STATIC, "strings", &pRootStrings);

  void*      pChildBuf  = malloc(kCapacity);
  MemZone_t* pChildZone = Z_Init(pChildBuf, kCapacity, PU_STATIC, "CHILD");

  MemZone_t* pChildStrings = NULL;
  MakeSubZone(pChildZone, 1024, PU_STATIC, "strings", &pChildStrings);

  RuntimeContext_t rootCtx  = { Z_ZoneAsBlock(pRootZone),  NULL };
  RuntimeContext_t childCtx = { Z_ZoneAsBlock(pChildZone), NULL };
  context_set_root(&rootCtx);
  context_push(&childCtx);

  Allocator_t result = context_get_allocator_by_name(name_from_cstr("strings"));
  REQUIRE(Allocator_IsValid(result));
  /* The child allocator's pContext is pChildStrings; root's is pRootStrings. */
  REQUIRE(result.pContext == (void*)pChildStrings);

  context_pop();
  free(pRootBuf);
  free(pChildBuf);
}

/* =========================================================================
 * Test_InvalidBlock
 * Context is set but pBlock is NULL (Block_IsValid == false).
 * Should skip gracefully and return INVALID_ALLOCATOR.
 * ========================================================================= */
TEST_CASE("MemoryContext – Invalid Block", "[MemoryContext]") {
  RuntimeContext_t ctx = { NULL, NULL };
  context_set_root(&ctx);

  Allocator_t result = context_get_allocator_by_name(name_from_cstr("strings"));
  REQUIRE(!Allocator_IsValid(result));
}
