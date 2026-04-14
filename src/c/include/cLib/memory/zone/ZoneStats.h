#ifndef ZONE_STATS_H
#define ZONE_STATS_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cLib/memory/zone/MemBlock.h"
#include "cLib/memory/allocator/ZoneAllocator.h"

// --- utility functions ---
uint32_t  Z_GetPurgeableSpace(MemZone_t* pZone);
uint32_t  Z_GetUsedSpace(MemZone_t* pZone, uint32_t tag);
bool      Z_CheckRange(MemZone_t* pZone, uint32_t minTag, uint32_t maxTag);
bool      Z_Check(MemZone_t* pZone);
bool      Z_Verify(MemZone_t* pZone);

#ifdef __cplusplus
} // extern "C"
#endif
#endif // ZONE_STATS
