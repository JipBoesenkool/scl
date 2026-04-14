#include "cLib/memory/allocator/ZoneAllocator.h"
#include "cLib/memory/zone/Zone.h"

//TODO:
// We need to decide how to deal with tags, we changed the interface to not have a tag.
// might be confusing if for instance a StackAllocator returns an item which had a tag set, but it does nothing.

// Helper to get data start
static inline uint8_t* GetData(MemZone_t* pZone) {
  return (uint8_t*)pZone + kMEM_ZONE_SIZE;
}

void* Zone_GetData(void* pContext, Handle_t* pHandle)
{
  MemZone_t* pZone = (MemZone_t*)pContext;
  if(!pHandle || pHandle->id == kCLOSED_HANDLE || pHandle->id == kINVALID_HANDLE) {
    return NULL;
  }
  // Handle versioning/patching
  if(pZone->version != pHandle->version)
  {
    // Note: 'id' stores the zone-relative offset used to find the block
    MemBlock_t* pBlock = Z_GetBlock(pZone, pHandle->id);
    // Check checksum/magic to ensure block is still valid
    if(pBlock->magic != CHECK_SUM)
    {
      *pHandle = kInvalidHandle;
      return NULL;
    }
    pHandle->version  = pZone->version;
    pHandle->pData    = GetBlockData(pBlock);
  }
  return pHandle->pData;
}

/* --- Internal Implementation --- */
  //NOTE: we need to keep in mind the Z_Malloc default to PU_LOCKED!
Handle_t Zone_Malloc(void* pContext, uint32_t size, void* pUser)
{
  MemZone_t* pZone = (MemZone_t*)pContext;
  void* pData = Z_Malloc(pZone, size, PU_LOCKED, pUser);
  if (!pData) return kInvalidHandle;
  MemBlock_t* pBlock = GetBlockHeader(pData);
  Handle_t handle{
    .id = Z_GetOffset(pZone, pBlock),
    .version = pZone->version,
    .pData = pData,
  };
  return handle;
}

Handle_t Zone_Realloc(void* pContext, Handle_t* pHandle, uint32_t size) {
  MemZone_t* pZone = (MemZone_t*)pContext;
  MemBlock_t* pOldBlock = GetBlockHeader(pHandle->pData);
  if (pOldBlock->magic != CHECK_SUM)
  {
    return Zone_Malloc(pZone, size, NULL);
  }
  void* pNew = Z_Realloc(pZone, pHandle->pData, size, pOldBlock->tag);
  if (!pNew) return kInvalidHandle;
  MemBlock_t* pBlock = GetBlockHeader(pNew);
  Handle_t handle{
    .id = Z_GetOffset(pZone, pBlock),
    .version = pZone->version,
    .pData = pNew,
  };
  return handle;
}

void Zone_Free(void* pContext, Handle_t* pHandle) {
  if (!pHandle || !pHandle->pData) return;
  Z_Free((MemZone_t*)pContext, pHandle->pData);
  *pHandle = kClosedHandle;
}

void Zone_Clear(void* pContext) {
  Z_Clear((MemZone_t*)pContext);
}

Allocator_t Zone_GetAllocator(MemZone_t* pZone) {
  Allocator_t alloc = {
    .pContext = (void*)pZone,
    .GetData  = Zone_GetData,
    .Clear    = Zone_Clear,
    .Malloc   = Zone_Malloc,
    .Realloc  = Zone_Realloc,
    .Free     = Zone_Free
  };
  return alloc;
}
