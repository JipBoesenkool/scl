#ifndef MEM_STRATEGY_H
#define MEM_STRATEGY_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cLib/cTypes.h"
#include "cLib/memory/zone/MemBlock.h"
#include "cLib/memory/allocator/Allocator.h"

/*
 * MemStrategy_t — type-level dispatch table for MemBlock operations.
 *
 * Indexed by eMemType_t (TYPE_DATA=0, TYPE_ZONE=1, TYPE_STACK=2).
 * Zero per-block overhead: eMemType_t is already stored in MemBlock_t::type.
 *
 * Contrast with Allocator_t:
 *   Allocator_t  — instance vtable, carries pContext (which zone/stack?), used when allocating.
 *   MemStrategy_t — type vtable, stateless, used when operating on an existing block.
 *
 * All operations receive MemBlock_t*. Sub-allocator state (MemZone_t*, MemStack_t*)
 * is derived on demand via GetBlockData(pBlock) — no extra context needed.
 */
typedef struct cMemStrategy
{
  // -- lifetime management ---
  void (*Init) (MemBlock_t* pBlock, uint32_t capacity, uint32_t tag, const char* name);
  void (*Clear)(MemBlock_t* pBlock);
  // --- Management ---
  void (*Purge)(MemBlock_t* pBlock);
  bool (*Check)(MemBlock_t* pBlock);
  void (*Remap)(MemBlock_t* pBlock);
  // --- Serialize ---
  void        (*Save)(MemBlock_t* pBlock, const char* filename);
  MemBlock_t* (*Load)(const char* filename);
  // --- Allocator ---
  Allocator_t  (*GetAllocator)(MemBlock_t* pBlock);
  // --- Identity ---
  const char*  (*GetBlockName)(MemBlock_t* pBlock); /* NULL for TYPE_DATA/TYPE_NONE */
  // --- Search ---
  /* Walk this block's children for the first whose name matches.
   * Returns &kInvalidBlock (never NULL) if no match or no children. */
  const MemBlock_t* (*FindBlock)(MemBlock_t* pBlock, Name_t name);
} MemStrategy_t;

/* Global dispatch table, defined in memory/private/strategy/MemStrategy.c.
 * Indexed by eMemType_t. */
extern const MemStrategy_t kMemStrategies[4];

/* Dispatch shorthand.
 * Usage: MEM_STRATEGY(pBlock).Check(pBlock) */
#define MEM_STRATEGY(pBlock) (kMemStrategies[(pBlock)->type])

#ifdef __cplusplus
} // extern "C"
#endif
#endif // MEM_STRATEGY_H
