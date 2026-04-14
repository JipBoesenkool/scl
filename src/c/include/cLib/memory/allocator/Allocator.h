#ifndef ALLOCATOR_H
#define ALLOCATOR_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cLib/memory/zone/Handle.h"
#include "cLib/memory/zone/MemBlock.h"
#include "cLib/string/cName.h"

/*
 * Allocator_t — instance vtable for memory allocation strategies.
 *
 * This is a "fat pointer": pContext holds the concrete allocator instance
 * (MemZone_t* or MemStack_t*) alongside the function table. This lets
 * allocators be passed around as a single value without the caller knowing
 * the concrete type.
 *
 * Contrast with MemStrategy_t (memory/strategy/MemStrategy.h):
 *   Allocator_t   — answers "how do I allocate from this instance?"
 *   MemStrategy_t — answers "what do I do with this block's type?"
 *
 * Future allocator strategies (roadmap):
 *   FrameAllocator   — double-buffer arena, swap each frame
 *   PoolAllocator    — fixed-size slab with free-list index
 *   ScratchAllocator — alias for StackAllocator
 */
typedef struct cAllocator {
  void* pContext;  /* Allocator instance ("this"). Always first — mirrors implicit self. */
  void*     (*GetData)(void* pContext, Handle_t* pHandle);
  /* --- Memory Operations --- */
  void      (*Clear)  (void* pContext);
  Handle_t  (*Malloc) (void* pContext, uint32_t size, void* pUser);
  Handle_t  (*Realloc)(void* pContext, Handle_t* pHandle, uint32_t size);
  void      (*Free)   (void* pContext, Handle_t* pHandle);
} Allocator_t;

// False only for INVALID_ALLOCATOR (pContext == NULL, vtable is Null_*).
static inline bool Allocator_IsValid(Allocator_t a) { return a.pContext != NULL; }

#ifdef __cplusplus
} // extern "C"
#endif
#endif
