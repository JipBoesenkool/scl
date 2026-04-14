#ifndef ZONE_STRATEGY_H
#define ZONE_STRATEGY_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cLib/memory/strategy/MemStrategy.h"

/* ============================================================
 * TYPE_ZONE strategy — payload is a MemZone_t sub-zone.
 * ============================================================ */
// -- lifetime management ---
void        ZoneStrategy_Init        (MemBlock_t* pBlock, uint32_t capacity, uint32_t tag, const char* name);
void        ZoneStrategy_Clear       (MemBlock_t* pBlock);
// --- Management ---
void        ZoneStrategy_Purge       (MemBlock_t* pBlock);
bool        ZoneStrategy_Check       (MemBlock_t* pBlock);
void        ZoneStrategy_Remap       (MemBlock_t* pBlock);
// --- Serialize ---
void        ZoneStrategy_Save        (MemBlock_t* pBlock, const char* filename);
MemBlock_t* ZoneStrategy_Load        (const char* filename);
// --- Allocator ---
Allocator_t ZoneStrategy_GetAllocator(MemBlock_t* pBlock);
// --- Identity ---
const char* ZoneStrategy_GetBlockName (MemBlock_t* pBlock);

extern const MemStrategy_t kZoneStrategy;

#ifdef __cplusplus
} // extern "C"
#endif
#endif // ZONE_STRATEGY_H
