#ifndef DATA_STRATEGY_H
#define DATA_STRATEGY_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cLib/memory/strategy/MemStrategy.h"

/* ============================================================
 * TYPE_DATA strategy — raw leaf data, no sub-allocator.
 * ============================================================ */
// -- lifetime management ---
void        DataStrategy_Init        (MemBlock_t* pBlock, uint32_t capacity, uint32_t tag, const char* name);
void        DataStrategy_Clear       (MemBlock_t* pBlock);
// --- Management ---
void        DataStrategy_Purge       (MemBlock_t* pBlock);
bool        DataStrategy_Check       (MemBlock_t* pBlock);
void        DataStrategy_Remap       (MemBlock_t* pBlock);
// --- Serialize ---
void        DataStrategy_Save        (MemBlock_t* pBlock, const char* filename);
MemBlock_t* DataStrategy_Load        (const char* filename);
// --- Allocator ---
Allocator_t DataStrategy_GetAllocator(MemBlock_t* pBlock);

extern const MemStrategy_t kDataStrategy;

#ifdef __cplusplus
} // extern "C"
#endif
#endif // DATA_STRATEGY_H
