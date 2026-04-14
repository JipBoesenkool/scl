/**
 * cMemoryContext_test.cpp — unit tests for memory/cMemoryContext.c
 *
 * Covers: context_get_allocator_by_name — no context, direct match,
 *         missing name, parent-climb, innermost-scope priority, invalid pBlock.
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "cLib/memory/cMemoryContext.h"
#include "cLib/memory/zone/Zone.h"
#include "cLib/memory/zone/MemBlock.h"
#include "cLib/memory/strategy/MemStrategy.h"
#include "cLib/memory/allocator/NullAllocator.h"
#include "cLib/string/cName.h"
#include "TestHelpers.h"

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
void Test_NoContext()
{
  printf("--- Test_NoContext ---\n");

  /* Ensure no context is active by pushing NULL-block context and popping it. */
  Allocator_t result = context_get_allocator_by_name(name_from_cstr("anything"));
  ASSERT(!Allocator_IsValid(result));

  printf("[PASS] Test_NoContext\n");
}

/* =========================================================================
 * Test_DirectMatch
 * Root context has a TYPE_ZONE child named "strings".
 * context_get_allocator_by_name("strings") returns a valid allocator.
 * ========================================================================= */
void Test_DirectMatch()
{
  printf("--- Test_DirectMatch ---\n");

  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init(pBuf, kCapacity, PU_STATIC, "ROOT");

  MemZone_t* pStrings = NULL;
  MakeSubZone(pZone, 2048, PU_STATIC, "strings", &pStrings);
  ASSERT(pStrings != NULL);

  /* Wire a root context pointing at the ROOT zone's anchor block. */
  RuntimeContext_t ctx = { Z_ZoneAsBlock(pZone), NULL };
  context_set_root(&ctx);

  Allocator_t result = context_get_allocator_by_name(name_from_cstr("strings"));
  ASSERT(Allocator_IsValid(result));

  free(pBuf);
  printf("[PASS] Test_DirectMatch\n");
}

/* =========================================================================
 * Test_MissingName
 * Root context has a child named "other", not "strings".
 * Returns INVALID_ALLOCATOR — no crash.
 * ========================================================================= */
void Test_MissingName()
{
  printf("--- Test_MissingName ---\n");

  void*      pBuf  = malloc(kCapacity);
  MemZone_t* pZone = Z_Init(pBuf, kCapacity, PU_STATIC, "ROOT");

  MemZone_t* pOther = NULL;
  MakeSubZone(pZone, 2048, PU_STATIC, "other", &pOther);

  RuntimeContext_t ctx = { Z_ZoneAsBlock(pZone), NULL };
  context_set_root(&ctx);

  Allocator_t result = context_get_allocator_by_name(name_from_cstr("strings"));
  ASSERT(!Allocator_IsValid(result));

  free(pBuf);
  printf("[PASS] Test_MissingName\n");
}

/* =========================================================================
 * Test_ParentClimb
 * Child context has no match; parent context does.
 * context_get_allocator_by_name climbs to the parent and returns its allocator.
 * ========================================================================= */
void Test_ParentClimb()
{
  printf("--- Test_ParentClimb ---\n");

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
  ASSERT(Allocator_IsValid(result));

  context_pop();
  free(pRootBuf);
  free(pChildBuf);
  printf("[PASS] Test_ParentClimb\n");
}

/* =========================================================================
 * Test_InnermostWins
 * Both child and parent have a "strings" sub-zone.
 * The child allocator (innermost scope) must be returned, not the parent's.
 * ========================================================================= */
void Test_InnermostWins()
{
  printf("--- Test_InnermostWins ---\n");

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
  ASSERT(Allocator_IsValid(result));
  /* The child allocator's pContext is pChildStrings; root's is pRootStrings. */
  ASSERT(result.pContext == (void*)pChildStrings);

  context_pop();
  free(pRootBuf);
  free(pChildBuf);
  printf("[PASS] Test_InnermostWins\n");
}

/* =========================================================================
 * Test_InvalidBlock
 * Context is set but pBlock is NULL (Block_IsValid == false).
 * Should skip gracefully and return INVALID_ALLOCATOR.
 * ========================================================================= */
void Test_InvalidBlock()
{
  printf("--- Test_InvalidBlock ---\n");

  RuntimeContext_t ctx = { NULL, NULL };
  context_set_root(&ctx);

  Allocator_t result = context_get_allocator_by_name(name_from_cstr("strings"));
  ASSERT(!Allocator_IsValid(result));

  printf("[PASS] Test_InvalidBlock\n");
}

int main()
{
  printf("=== cMemoryContext_test ===\n");
  Test_NoContext();
  Test_DirectMatch();
  Test_MissingName();
  Test_ParentClimb();
  Test_InnermostWins();
  Test_InvalidBlock();
  printf("=== ALL PASSED ===\n");
  return 0;
}
