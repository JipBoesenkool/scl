#ifndef ZONE_H
#define ZONE_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cLib/memory/zone/MemBlock.h"
#include "cLib/memory/allocator/ZoneAllocator.h"

// --- Helpers ---
MemBlock_t* Z_GetBlock(MemZone_t* pZone, uint32_t offset);
uint32_t    Z_GetOffset(MemZone_t* pZone, MemBlock_t* pBlock);

MemBlock_t* Z_ZoneAsBlock(MemZone_t* pZone);   // same-address reinterpret cast
MemZone_t*  Z_BlockAsZone(MemBlock_t* pBlock);  // same-address reinterpret cast

// --- Interface ---
MemZone_t* Z_Init (void* pMemory, uint32_t capacity, uint32_t tag, const char* name);
void*	Z_Malloc(MemZone_t* pZone, uint32_t size, uint32_t tag, void* pUser);
void* Z_Realloc(MemZone_t* pZone, void* pData, uint32_t newSize, uint32_t tag);
void  Z_Free (MemZone_t* pZone, void* pData);
void	Z_Clear(MemZone_t* pZone);
void  Z_Remap(MemZone_t* pZone);

// --- Utility? ---
void  Z_ChangeTag(MemBlock_t* pBlock, uint32_t tag);
void  Z_PurgeTag(MemZone_t* pZone, uint32_t tag);
void  Z_PurgeTagRange(MemZone_t* pZone, uint32_t minTag, uint32_t maxTag);

void  Z_Save(MemZone_t* pZone, const char* filename);
MemZone_t* Z_Load(Allocator_t* pAllocator, const char *filename);

void       Z_Compact(MemZone_t* pZone);

// Search pZone's block tree for the first block whose name matches.
// Dispatches through MEM_STRATEGY — no hardcoded TYPE_ZONE assumption.
// Returns INVALID_ALLOCATOR if not found.
Allocator_t Z_FindAllocator(MemZone_t* pZone, Name_t name);

#ifdef __cplusplus
} // extern "C"
#endif
#endif // ZONE_H
