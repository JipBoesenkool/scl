#ifndef NULL_STRATEGY_H
#define NULL_STRATEGY_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cLib/memory/strategy/MemStrategy.h"

// ============================================================
// TYPE_NULL strategy
// ============================================================
// -- lifetime management ---
void        NullStrategy_Init(MemBlock_t* pBlock, uint32_t capacity, uint32_t tag, const char* name);
void        NullStrategy_Clear(MemBlock_t* pBlock);
// --- Management ---
void        NullStrategy_Purge(MemBlock_t* pBlock);
bool        NullStrategy_Check(MemBlock_t* pBlock);
void        NullStrategy_Remap(MemBlock_t* pBlock);
// --- Serialize ---
void        NullStrategy_Save(MemBlock_t* pBlock, const char* filename);
MemBlock_t* NullStrategy_Load(const char* filename);
// --- Allocator ---
Allocator_t NullStrategy_GetAllocator(MemBlock_t* pBlock);

extern const MemStrategy_t kNullStrategy;

#ifdef __cplusplus
} // extern "C"
#endif
#endif // NULL_STRATEGY_H
