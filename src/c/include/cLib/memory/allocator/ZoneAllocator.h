#ifndef ZONE_ALLOCATOR_H
#define ZONE_ALLOCATOR_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cLib/memory/allocator/Allocator.h"

typedef struct cMemZone
{
  MemBlock_t  blocklist;  // 32 bytes
  uint32_t    rover;      //  4 bytes — zone-relative byte offset to current rover block
  uint32_t    used;       //  4 bytes
  uint32_t    capacity;   //  4 bytes
  uint32_t    version;    //  4 bytes 
  Name_t      name;       // 16 bytes (const char* ptr + FNV hash)
} MemZone_t; // 64 bytes
static_assert(sizeof(MemZone_t) == 64, "MemZone_t must be exactly 64 bytes");

static const uint32_t kMEM_ZONE_SIZE  = (sizeof(MemZone_t) + ALIGN_MASK) & ~ALIGN_MASK;

// Reinterpret the payload of a TYPE_ZONE block as its sub-zone.
// pBlock is the block header in the parent zone; the MemZone_t begins kMEM_BLOCK_SIZE bytes in.
static inline MemZone_t* Block_AsSubZone(MemBlock_t* pBlock) {
  return (MemZone_t*)GetBlockData(pBlock);
}

// --- Interface ---
void*     Zone_GetData(void* pContext, Handle_t* pHandle); /* pContext = MemZone_t* */
void      Zone_Clear  (void* pContext);
Handle_t  Zone_Malloc (void* pContext, uint32_t size, void* pUser);
Handle_t  Zone_Realloc(void* pContext, Handle_t* pHandle, uint32_t size);
void      Zone_Free   (void* pContext, Handle_t* pHandle);
// Returns a fully wired Allocator_t for the given zone.
Allocator_t Zone_GetAllocator(MemZone_t* pZone);

#ifdef __cplusplus
} // extern "C"
#endif
#endif // #define ZONE_ALLOCATOR_H
