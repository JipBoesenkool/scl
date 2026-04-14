#include "cLib/memory/zone/MemBlock.h"

#include "cLib/Log.h"
#include "cLib/memory/allocator/Allocator.h"
#include "cLib/cTypes.h"

#include <stdio.h>

const MemBlock_t kInvalidBlock = { 0, 0, PU_FREE, TYPE_NONE, NULL, 0, 0 };

// Helper to align sizes up to the next 8-byte boundary
uint32_t Align(uint32_t size)
{
  return (size + ALIGN_MASK) & ~ALIGN_MASK;
}

// Helper to go from Data Ptr -> Header Ptr
MemBlock_t *GetBlockHeader(void *pData)
{
  //TODO: support invalid block
  if (!pData) return NULL;
  return (MemBlock_t *)((uint8_t *)pData - kMEM_BLOCK_SIZE);
}

// Helper to go from Header Ptr -> Data Ptr
void* GetBlockData(MemBlock_t *pBlock)
{
  if (!pBlock) return NULL;
  return (void *)((uint8_t *)pBlock + kMEM_BLOCK_SIZE);
}

/* Saves only the payload and the ppUser address of a specific block */
bool Block_Save(MemBlock_t *pBlock, const char *filename)
{
  //TODO: LOCK!
  if (!pBlock || !filename) return false;
  FILE *pFile = fopen(filename, "wb");  
  uint32_t size = pBlock->size - kMEM_BLOCK_SIZE;
  // Write the block data we need
  fwrite(&pBlock->ppUser, sizeof(void **), 1, pFile);
  fwrite(&size, sizeof(uint32_t), 1, pFile); 
  fwrite(&pBlock->tag, sizeof(uint32_t), 1, pFile);
  fwrite(&pBlock->type, sizeof(uint32_t), 1, pFile);
  // Write actual data
  void *pData = GetBlockData(pBlock);
  fwrite(pData, size, 1, pFile);

  fclose(pFile);
  //TODO: Reset TAG!
  return true;
}

/* Allocates a new block in pZone and populates it from disk */
void* Block_Load(Allocator_t* pAllocator, MemBlock_t** outBlock, const char *filename)
{
  //TODO: if we have the allocator we don't need an out block.
  if (!pAllocator || !outBlock || !filename) return NULL;  
  FILE *pFile = fopen(filename, "rb");
  if (!pFile) return NULL;
  // Variables we need to read
  MemBlock_t tempBlock;
  fread(&tempBlock.ppUser, sizeof(void **), 1, pFile);
  fread(&tempBlock.size, sizeof(uint32_t), 1, pFile);
  fread(&tempBlock.tag, sizeof(uint32_t), 1, pFile);
  fread(&tempBlock.type, sizeof(uint32_t), 1, pFile);
  // Allocate fresh memory in the current zone
  Handle_t h = pAllocator->Malloc(pAllocator->pContext, tempBlock.size, NULL);
  void* pData = h.pData;
  if (pData)
  {
    fread(pData, tempBlock.size, 1, pFile);
  }
  *outBlock = GetBlockHeader(pData);
  (*outBlock)->tag = tempBlock.tag;
  fclose(pFile);
  //TODO: Reset TAG!
  return pData;
}
