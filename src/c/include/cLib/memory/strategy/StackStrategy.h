#ifndef STACK_STRATEGY_H
#define STACK_STRATEGY_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cLib/memory/strategy/MemStrategy.h"

/* ============================================================
 * TYPE_STACK strategy — payload is a MemStack_t linear allocator.
 * Stubs: implement when nested stacks are in use.
 * ============================================================ */
// -- lifetime management ---
void StackStrategy_Init(MemBlock_t* pBlock, uint32_t capacity, uint32_t tag, const char* name);
void StackStrategy_Clear(MemBlock_t* pBlock);
// --- Management ---
void StackStrategy_Purge(MemBlock_t* pBlock);
bool StackStrategy_Check(MemBlock_t* pBlock);
void StackStrategy_Remap(MemBlock_t* pBlock);
// --- Serialize ---
void        StackStrategy_Save(MemBlock_t* pBlock, const char* filename);
MemBlock_t* StackStrategy_Load(const char* filename);
// --- Allocator ---
Allocator_t StackStrategy_GetAllocator(MemBlock_t* pBlock);
// --- Identity ---
const char* StackStrategy_GetBlockName (MemBlock_t* pBlock);

extern const MemStrategy_t kStackStrategy;

#ifdef __cplusplus
} // extern "C"
#endif
#endif // STACK_STRATEGY_H
