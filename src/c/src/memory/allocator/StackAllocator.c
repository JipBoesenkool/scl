#include "cLib/memory/allocator/StackAllocator.h"
#include "cLib/Log.h"
#include <string.h>

// Helper to get data start
static inline uint8_t* GetData(MemStack_t* pStack) {
  return (uint8_t*)pStack + kMEM_STACK_SIZE;
}

void* Stack_GetData(MemStack_t* pStack, Handle_t* pHandle)
{
  if (!pHandle) return NULL;
  // Invalidate handle if versions don't match
  if(pStack->version != pHandle->version)
  {
    *pHandle = kInvalidHandle;
  }
  return pHandle->pData;
}

void Stack_Clear(MemStack_t* pStack) {
  pStack->used = 0;
  pStack->version++;
}

Handle_t Stack_Malloc(MemStack_t* pStack, uint32_t size, void* pUser) {
  uint32_t alignedSize = Align(size);
  if (pStack->used + alignedSize > pStack->capacity) {
    Log_Add("[ERROR] Stack_Malloc: StackOverflow!");
    return kInvalidHandle;
  }
  uint32_t index = pStack->used;
  // Calculate payload address relative to struct base
  uint8_t* pData = GetData(pStack);
  void* pAlloc = pData + pStack->used;
  pStack->used += alignedSize;
  //TODO: set pointer to pointer in handle?
  // Standard handle patching if requested
  if (pUser != NULL) {
    *(void**)pUser = pAlloc;
  }
  return Handle_t{
    .id = index,
    .version = pStack->version,
    .pData = pAlloc
  };
}

Handle_t Stack_Realloc(MemStack_t* pStack, Handle_t* pHandle, uint32_t newSize)
{
  if (!pHandle || !pHandle->pData) return Stack_Malloc(pStack, newSize, NULL);
  //// Linear stack realloc: copy to a new block at the top
  Handle_t handle = Stack_Malloc(pStack, newSize, NULL);
  if (handle.pData) {
    memcpy(handle.pData, pHandle->pData, newSize); 
  }
  *pHandle = handle;
  return handle;
}

void Stack_Free(MemStack_t* pStack, Handle_t* pHandle) {
  (void)pStack;
  if (pHandle) {
    *pHandle = kClosedHandle;
  }
}

Allocator_t Stack_GetAllocator(MemStack_t* pStack) {
  Allocator_t alloc = {
    .pContext = (void*)pStack,
    .GetData  = (void* (*)(void*, Handle_t*))               Stack_GetData,
    .Clear    = (void  (*)(void*))                          Stack_Clear,
    .Malloc   = (Handle_t (*)(void*, uint32_t, void*))      Stack_Malloc,
    .Realloc  = (Handle_t (*)(void*, Handle_t*, uint32_t))  Stack_Realloc,
    .Free     = (void  (*)(void*, Handle_t*))               Stack_Free
  };
  return alloc;
}
