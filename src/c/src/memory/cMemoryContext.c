#include "cLib/memory/cMemoryContext.h"
#include "cLib/memory/zone/Zone.h"
#include "cLib/memory/allocator/NullAllocator.h"
#include "cLib/string/cName.h"
#include <assert.h>

// The "Head" of the stack for the current thread
//static _Thread_local RuntimeContext_t* g_pActiveContext = NULL;
static RuntimeContext_t* g_pActiveContext = NULL;

void context_set_root(RuntimeContext_t* pRoot) {
  // The root has no parent
  pRoot->pParent = NULL;
  g_pActiveContext = pRoot;
}

void context_push(RuntimeContext_t* pNewCtx) {
  assert(pNewCtx != NULL);
  // Link the new context to the current one
  pNewCtx->pParent = g_pActiveContext;
  // Move the head of the stack forward
  g_pActiveContext = pNewCtx;
}

void context_pop(void)
{
  if (g_pActiveContext && g_pActiveContext->pParent)
  {
    g_pActiveContext = g_pActiveContext->pParent;
  }
}

Allocator_t* context_get_active_allocator(void)
{
  // If no context is set, you might want to return a
  // fallback System Allocator (malloc) or assert.
  assert(g_pActiveContext != NULL && "No active context found!");
  return nullptr;
  //return g_pActiveContext->pDefault;
}

Allocator_t context_get_allocator_by_name(Name_t name)
{
  RuntimeContext_t* pCtx = g_pActiveContext;
  while (pCtx != NULL) {
    if (Block_IsValid(pCtx->pBlock)) {
      /* pCtx->pBlock is the root zone's anchor — use Z_FindAllocator which
       * handles the root-zone case correctly (block list walk, not sub-zone payload). */
      MemZone_t* pZone = Z_BlockAsZone(pCtx->pBlock);
      Allocator_t result = Z_FindAllocator(pZone, name);
      if (Allocator_IsValid(result)) return result;
    }
    pCtx = pCtx->pParent;
  }
  return INVALID_ALLOCATOR;
}
