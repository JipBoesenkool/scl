#include "cLib/memory/Memory.h"

/**
 * Mem_Init
 * A high-level wrapper to turn a raw malloc'd chunk into a managed Zone.
 */
/*
MemBlock_t* Mem_Init(uint8_t* pBaseAddress, uint32_t capacity, uint32_t type, const char* name) {
  if (!pBaseAddress) return NULL;

  MemBlock_t* pBlock = (MemBlock_t*)pBaseAddress;
  pBlock->capacity = capacity;

  // Initialize the block as a Zone using our strategy
  kZoneAllocator.Init(pBlock, type, PU_STATIC, name);
  return pBlock;
}
*/
