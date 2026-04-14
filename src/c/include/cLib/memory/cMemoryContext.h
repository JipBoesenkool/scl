#ifndef CONTEXT_H
#define CONTEXT_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cLib/memory/allocator/Allocator.h"
#include "cLib/string/cName.h"

typedef struct RuntimeContext {
  MemBlock_t*             pBlock;  // Root block for this scope (TYPE_ZONE in practice)
  struct RuntimeContext*  pParent; // Link to the previous scope
} RuntimeContext_t;

// --- Context Management ---

// Sets the global "Root" context (usually called at Engine Start)
void context_set_root(RuntimeContext_t* pRoot);

// Pushes a new scope (e.g., when entering a Module)
void context_push(RuntimeContext_t* pNewCtx);

// Pops the current scope, returning to the parent
void context_pop(void);

// Retrieves the active allocator for the current thread
Allocator_t* context_get_active_allocator(void);

// Searches the active context's block tree for 'name', then climbs pParent.
// Innermost scope wins. Returns INVALID_ALLOCATOR (safe NullObject) if not found.
// NOT thread-safe — context is not yet thread-local.
Allocator_t context_get_allocator_by_name(Name_t name);

#ifdef __cplusplus
} // extern "C"
#endif
#endif // CONTEXT_H
