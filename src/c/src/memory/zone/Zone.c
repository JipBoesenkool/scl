#include "cLib/memory/zone/Zone.h"
#include "cLib/memory/strategy/MemStrategy.h"
#include "cLib/memory/allocator/NullAllocator.h"
#include "cLib/string/cName.h"
#include "cLib/Log.h"

#include <string.h>
#include <stdio.h>

#define MIN_FRAGMENT 64
static void* kUNOWNED_ADDRESS = (void*)2;

// --- Helpers ---
MemBlock_t* Z_GetBlock(MemZone_t* pZone, uint32_t offset)
{
  return (MemBlock_t*)( (uint8_t*)pZone + offset );
}

uint32_t Z_GetOffset(MemZone_t* pZone, MemBlock_t* pBlock)
{
  return (uint32_t)( (uint8_t*)pBlock - (uint8_t*)pZone);
}

// Same address — blocklist is the first member of MemZone_t
MemBlock_t* Z_ZoneAsBlock(MemZone_t* pZone)
{
  return (MemBlock_t*)pZone;
}
// Same address — block pointer aliases the MemZone_t it heads
MemZone_t* Z_BlockAsZone(MemBlock_t* pBlock)
{
  return (MemZone_t*)pBlock;
}

// --- Interface ---
MemZone_t* Z_Init(void* pMemory, uint32_t capacity, uint32_t tag, const char* name)
{
  if (!pMemory) return NULL;
  Log_Add("[STATUS] Z_InitZone: %s", name);
  MemZone_t* pZone = (MemZone_t*)pMemory;
  memset(pZone, 0, capacity);
  // This should be set by init
  pZone->capacity       = capacity;
  pZone->blocklist.tag  = tag;
  pZone->blocklist.type = TYPE_ZONE;
  pZone->name = name_from_cstr(name);
  // rest should be set by Z_Clear
  Z_Clear(pZone);
  return pZone;
};


void* Z_Malloc(MemZone_t* pZone, uint32_t size, uint32_t tag, void* pUser) {
  uint32_t totalNeeded = Align(size) + kMEM_BLOCK_SIZE;
  uint32_t anchorOffset = Z_GetOffset(pZone, &pZone->blocklist);
  // 1. Rover Backup: maximize consolidation
  MemBlock_t* pBaseBlock = Z_GetBlock(pZone, pZone->rover);
  MemBlock_t* pBack = Z_GetBlock(pZone, pBaseBlock->prev);
  if (pBack->tag == PU_FREE && pBack != &pZone->blocklist) {
    pBaseBlock = pBack;
  }
  MemBlock_t* pRover = pBaseBlock;
  MemBlock_t* pStart = pBaseBlock;
  // 2. Search & Purge Loop
  do {
    if (pRover->tag != PU_FREE) {
      if (pRover->tag <= PU_LOCKED) {
        // Skip non-purgeable blocks (PU_STATIC, PU_LEVEL, PU_LOCAL, PU_LOCKED)
        pBaseBlock = pRover = Z_GetBlock(pZone, pRover->next);
      }
      else {
        // tag > PU_LOCKED: purgeable — free it to reclaim space
        Z_Free(pZone,  GetBlockData(pRover));
        // Reset search pointers to the newly enlarged free block
        pBaseBlock = Z_GetBlock(pZone, pZone->rover);
        pRover = pBaseBlock->next != anchorOffset ? Z_GetBlock(pZone, pBaseBlock->next) : pBaseBlock;
      }
    }
    else {
      pRover = Z_GetBlock(pZone, pRover->next);
    }

    if (pRover == pStart) {
      Log_Add("Z_Malloc: Zone %s failed on allocation of %i bytes", pZone->name.name, totalNeeded);
      return NULL;
    }
  } while (pBaseBlock->tag != PU_FREE || pBaseBlock->size < totalNeeded);
  // 3. Splitting logic
  uint32_t extra = pBaseBlock->size - totalNeeded;
  if (extra > MIN_FRAGMENT) {
    uint32_t baseOffset = Z_GetOffset(pZone, pBaseBlock);
    MemBlock_t* pNewFree = (MemBlock_t*)((uint8_t*)pBaseBlock + totalNeeded);

    pNewFree->magic = CHECK_SUM;
    pNewFree->size = extra;
    pNewFree->tag = PU_FREE;
    pNewFree->ppUser = NULL;
    pNewFree->prev = baseOffset;
    pNewFree->next = pBaseBlock->next;

    Z_GetBlock(pZone, pNewFree->next)->prev = Z_GetOffset(pZone, pNewFree);
    pBaseBlock->next = Z_GetOffset(pZone, pNewFree);
    pBaseBlock->size = totalNeeded;
  }
  pZone->used += totalNeeded;
  // 4. Finalize
  pBaseBlock->tag = tag;
  pBaseBlock->magic = CHECK_SUM;
  if (pUser != NULL) {
    pBaseBlock->ppUser = (void**)pUser;
    *(void**)pUser =  GetBlockData(pBaseBlock);
  }
  else {
    pBaseBlock->ppUser = (void**)&kUNOWNED_ADDRESS;
  }
  pZone->rover = pBaseBlock->next;
  return  GetBlockData(pBaseBlock);
}

void* Z_Realloc(MemZone_t* pZone, void* pData, uint32_t newSize, uint32_t tag) {
  if (!pData) {
    return Z_Malloc(pZone, newSize, tag, NULL);
  }
  MemBlock_t* pBlock = GetBlockHeader(pData);
  uint32_t totalNeeded = Align(newSize) + kMEM_BLOCK_SIZE;
  // 1. Check if the current block is already big enough
  if (pBlock->size >= totalNeeded) {
    // Optional: We could split here if (pBlock->size - totalNeeded) > MIN_FRAGMENT
    return pData;
  }
  // 2. Try to expand into the NEXT block if it is free
  uint32_t anchorOffset = Z_GetOffset(pZone, &pZone->blocklist);
  if (pBlock->next != anchorOffset) {
    MemBlock_t* pNext = Z_GetBlock(pZone, pBlock->next);
    if (pNext->tag == PU_FREE && (pBlock->size + pNext->size) >= totalNeeded) {
      // Absorb the next block
      uint32_t combinedSize = pBlock->size + pNext->size;
      pZone->used += pNext->size; // We are consuming the free block's size

      pBlock->next = pNext->next;
      Z_GetBlock(pZone, pBlock->next)->prev = Z_GetOffset(pZone, pBlock);
      pBlock->size = combinedSize;

      // 3. Handle splitting if we absorbed way more than needed
      uint32_t extra = pBlock->size - totalNeeded;
      if (extra > MIN_FRAGMENT) {
        uint32_t baseOffset = Z_GetOffset(pZone, pBlock);
        MemBlock_t* pNewFree = (MemBlock_t*)((uint8_t*)pBlock + totalNeeded);

        pNewFree->magic = CHECK_SUM;
        pNewFree->size = extra;
        pNewFree->tag = PU_FREE;
        pNewFree->ppUser = NULL;
        pNewFree->prev = baseOffset;
        pNewFree->next = pBlock->next;

        Z_GetBlock(pZone, pNewFree->next)->prev = Z_GetOffset(pZone, pNewFree);
        pBlock->next = Z_GetOffset(pZone, pNewFree);
        pBlock->size = totalNeeded;
        pZone->used -= extra;
      }
      
      return pData;
    }
  }
  // 4. Fallback: Malloc, Copy, and Free
  // We pass pBlock->ppUser to the new allocation to maintain handle integrity
  void* pNew = Z_Malloc(pZone, newSize, tag, pBlock->ppUser);
  if (pNew) {
    memcpy(pNew, pData, pBlock->size - kMEM_BLOCK_SIZE);
    // prevent Z_Free from NULLing the handle we just set
    pBlock->ppUser = NULL; 
    Z_Free(pZone, pData);
  }
  return pNew;
}

void Z_Free(MemZone_t* pZone, void* pData) {
  if (!pData) return;
  /* 1. Get the header and validate */
  MemBlock_t* pBlock = GetBlockHeader(pData);
  if (pBlock->magic != CHECK_SUM) {
    Log_Add("[ERROR] Z_Free: Memory corruption or invalid pointer at %p", pData);
    return;
  }
  // free the memory for the zone
  pZone->used -= pBlock->size;
  /* 2. Mark as free and clear user handle */
  pBlock->tag = PU_FREE;
  if (pBlock->ppUser && pBlock->ppUser != (void**)&kUNOWNED_ADDRESS) {
    *pBlock->ppUser = NULL;
  }
  pBlock->ppUser = NULL;
  /* 3. Consolidate with the NEXT block */
  MemBlock_t* pNext = Z_GetBlock(pZone, pBlock->next);
  if (pNext->tag == PU_FREE && pNext != &pZone->blocklist) {
    pBlock->size += pNext->size;
    pBlock->next = pNext->next;
    /* Update the back-link of the new 'next' block */
    Z_GetBlock(pZone, pBlock->next)->prev = Z_GetOffset(pZone, pBlock);
  }
  /* 4. Consolidate with the PREVIOUS block */
  MemBlock_t* pPrev = Z_GetBlock(pZone, pBlock->prev);
  if (pPrev->tag == PU_FREE && pPrev != &pZone->blocklist) {
    pPrev->size += pBlock->size;
    pPrev->next = pBlock->next;
    /* Update the back-link of the new 'next' block */
    Z_GetBlock(pZone, pPrev->next)->prev = Z_GetOffset(pZone, pPrev);
    /* Move pBlock pointer so the rover update below is correct */
    pBlock = pPrev;
  }
  /* 5. Update the Rover to point to a known-valid free block */
  pZone->rover = Z_GetOffset(pZone, pBlock);
}

void Z_Clear(MemZone_t* pZone)
{
  Log_Add("[STATUS] Z_ClearZone: %s", pZone->name.name);
  uint32_t anchorOffset = Z_GetOffset(pZone, &pZone->blocklist);
  MemBlock_t* pFirstBlock = Z_GetBlock(pZone, kMEM_ZONE_SIZE);
  // Set up the sentinel anchor
  pZone->used             = kMEM_ZONE_SIZE;
  pZone->blocklist.magic  = CHECK_SUM;
  pZone->blocklist.size   = 0;
  //pZone->blocklist->tag; // We should keep the Tag set by caller
  pZone->blocklist.ppUser = (void**)&pZone;
  pZone->blocklist.next   = kMEM_ZONE_SIZE;
  pZone->blocklist.prev   = kMEM_ZONE_SIZE;
  // Initialize the first large free block
  pFirstBlock->magic  = CHECK_SUM;
  pFirstBlock->size   = (uint32_t)(pZone->capacity - kMEM_ZONE_SIZE);
  pFirstBlock->tag    = PU_FREE;
  pFirstBlock->type   = TYPE_DATA;
  pFirstBlock->ppUser = NULL;
  pFirstBlock->prev   = anchorOffset;
  pFirstBlock->next   = anchorOffset;
  // rover is now an offset; first block always sits at kMEM_ZONE_SIZE after clear
  pZone->rover        = kMEM_ZONE_SIZE;
};

void Z_Remap(MemZone_t* pZone)
{
  int32_t anchorOff = Z_GetOffset(pZone, &pZone->blocklist);
  uint32_t currOff   = pZone->blocklist.next;
  /* Walk the circular list and patch handles.
   * rover is a zone-relative offset so it survives Z_Load without fixup. */
  while (currOff != anchorOff) {
    MemBlock_t* pCurr = Z_GetBlock(pZone, currOff);
    /* * If ppUser is valid and not 'unowned', it points to a stable variable address.
     * We update that variable to point to our current physical data location.
     */
    if (pCurr->ppUser && pCurr->ppUser != (void**)&kUNOWNED_ADDRESS) {
      *(pCurr->ppUser) = GetBlockData(pCurr);
    }
    /* Recurse into sub-allocators (TYPE_ZONE, TYPE_STACK) so their internal
     * handles are also patched. Dispatch via MemStrategy — no manual type check. */
    if (pCurr->tag != PU_FREE) {
      MEM_STRATEGY(pCurr).Remap(pCurr);
    }
    currOff = pCurr->next;
  }
  Log_Add("[STATUS] Z_Remap: All handles remapped for %s", pZone->name.name);
}

// --- Utility ---
void Z_ChangeTag(MemBlock_t* pBlock, uint32_t tag)
{
  if (pBlock == NULL) return;
  // 1. Security Check: Ensure this is a valid zone block
  if (pBlock->magic != CHECK_SUM) {
    Log_Add("[ERROR] Z_ChangeTag: Pointer %p has invalid magic!", pBlock);
    return;
  }
  // 2. Validation: Purgeable blocks (tag > PU_LOCKED) MUST have a user handle.
  if (tag > PU_LOCKED && (pBlock->ppUser == NULL || pBlock->ppUser == (void**)&kUNOWNED_ADDRESS))
  {
    Log_Add("[ERROR] Z_ChangeTag: An owner is required for purgeable blocks.");
    return;
  }
  pBlock->tag = tag;
}

void Z_PurgeTag(MemZone_t* pZone, uint32_t tag) {
  uint32_t anchorOffset = Z_GetOffset(pZone, &pZone->blocklist);
  uint32_t currOffset = pZone->blocklist.next;
  while (currOffset != anchorOffset)
  {
    MemBlock_t* pBlock = Z_GetBlock(pZone, currOffset);
    // Save the next offset BEFORE we potentially merge this block
    uint32_t nextOffset = pBlock->next;
    /* Cascade purge into TYPE_ZONE sub-allocators with the same tag. */
    if (pBlock->type == TYPE_ZONE && pBlock->tag != PU_FREE) {
      Z_PurgeTag((MemZone_t*)GetBlockData(pBlock), tag);
    }
    // Purge if tag matches. PU_STATIC and PU_LOCKED are never freed by purge.
    if (pBlock->tag == tag && pBlock->tag != PU_STATIC && pBlock->tag != PU_LOCKED)
    {
      // Note: Z_Free will merge blocks, but since we already saved nextOffset, our loop remains stable.
      Z_Free(pZone, GetBlockData(pBlock));
    }
    currOffset = nextOffset;
  }
}

void Z_PurgeTagRange(MemZone_t* pZone, uint32_t minTag, uint32_t maxTag) {
  uint32_t anchorOffset = Z_GetOffset(pZone, &pZone->blocklist);
  uint32_t currOffset = pZone->blocklist.next;
  while (currOffset != anchorOffset)
  {
    MemBlock_t* pBlock = Z_GetBlock(pZone, currOffset);
    // Save the next offset BEFORE we potentially merge this block
    uint32_t nextOffset = pBlock->next;
    /* PurgeCascade carries a single tag; range cascading needs direct dispatch. */
    if (pBlock->type == TYPE_ZONE && pBlock->tag != PU_FREE) {
      Z_PurgeTagRange((MemZone_t*)GetBlockData(pBlock), minTag, maxTag);
    }
    // Only range-purge purgeable blocks (tag > PU_LOCKED). Never touch PU_LEVEL/PU_LOCAL/PU_LOCKED.
    if (pBlock->tag > PU_LOCKED &&
        pBlock->tag >= minTag && pBlock->tag <= maxTag)
    {
      Z_Free(pZone, GetBlockData(pBlock));
    }
    currOffset = nextOffset;
  }
}

/* Save the entire zone as a single binary blob */
void Z_Save(MemZone_t* pZone, const char* filename) {
  //TODO: This won't work, we need to recusivly call the save?
  FILE* pFile = fopen(filename, "wb");
  if (pFile) {
    fwrite(pZone, pZone->capacity, 1, pFile);
    fclose(pFile);
  }
}

/* Compact the zone: slide moveable allocated blocks to the low end so free
 * space coalesces.  PU_STATIC and PU_LOCKED blocks are immovable; all other
 * live blocks (PU_LEVEL, PU_LOCAL, purgeable) slide. Gaps before immovable
 * blocks become free blocks.
 * Prerequisite: rover must be a zone-relative offset (already the case). */
void Z_Compact(MemZone_t* pZone)
{
  uint32_t anchorOffset = Z_GetOffset(pZone, &pZone->blocklist);
  uint32_t writeOffset  = kMEM_ZONE_SIZE;
  uint32_t prevOffset   = anchorOffset;
  uint32_t currOffset   = pZone->blocklist.next;

  while (currOffset != anchorOffset) {
    MemBlock_t* pBlock    = Z_GetBlock(pZone, currOffset);
    uint32_t    blockSize = pBlock->size;
    uint32_t    nextOff   = pBlock->next; /* save before memmove may clobber */

    if (pBlock->tag == PU_FREE) {
      /* Free block — skip; its space will be reclaimed by writeOffset catching up */
    }
    else if (pBlock->tag == PU_STATIC || pBlock->tag == PU_LOCKED) {
      /* Immovable block (PU_STATIC = permanent, PU_LOCKED = temporarily pinned) — leave in place.  If there is a gap between writeOffset
       * and this block, emit a free block to fill it. */
      if (writeOffset < currOffset) {
        uint32_t gapSize = currOffset - writeOffset;
        MemBlock_t* pGap = Z_GetBlock(pZone, writeOffset);
        memset(pGap, 0, sizeof(MemBlock_t));
        pGap->magic = CHECK_SUM;
        pGap->size  = gapSize;
        pGap->tag   = PU_FREE;
        pGap->type  = TYPE_DATA;
        pGap->prev  = prevOffset;
        pGap->next  = currOffset;
        Z_GetBlock(pZone, prevOffset)->next = writeOffset;
        pBlock->prev = writeOffset;
        prevOffset   = currOffset;
      } else {
        /* No gap — just re-stitch prev link */
        Z_GetBlock(pZone, prevOffset)->next = currOffset;
        pBlock->prev = prevOffset;
        prevOffset   = currOffset;
      }
      writeOffset = currOffset + blockSize; /* advance past the immovable block */
    }
    else {
      /* Moveable block — slide it down to writeOffset */
      if (writeOffset != currOffset) {
        memmove(Z_GetBlock(pZone, writeOffset), pBlock, blockSize);
      }
      MemBlock_t* pDest = Z_GetBlock(pZone, writeOffset);
      pDest->prev = prevOffset;
      Z_GetBlock(pZone, prevOffset)->next = writeOffset;

      prevOffset   = writeOffset;
      writeOffset += blockSize;
    }
    currOffset = nextOff;
  }

  /* Create (or skip) the free tail after the last block */
  uint32_t freeSize = pZone->capacity - writeOffset;
  if (freeSize >= kMEM_BLOCK_SIZE) {
    MemBlock_t* pFree = Z_GetBlock(pZone, writeOffset);
    memset(pFree, 0, sizeof(MemBlock_t));
    pFree->magic = CHECK_SUM;
    pFree->size  = freeSize;
    pFree->tag   = PU_FREE;
    pFree->type  = TYPE_DATA;
    pFree->prev  = prevOffset;
    pFree->next  = anchorOffset;

    Z_GetBlock(pZone, prevOffset)->next = writeOffset;
    pZone->blocklist.prev = writeOffset;
    pZone->rover          = writeOffset;
  } else {
    Z_GetBlock(pZone, prevOffset)->next = anchorOffset;
    pZone->blocklist.prev = prevOffset;
    pZone->rover          = kMEM_ZONE_SIZE;
  }

  Z_Remap(pZone);
  Log_Add("[STATUS] Z_Compact: %s compacted, %u bytes free", pZone->name.name, freeSize);
}

Allocator_t Z_FindAllocator(MemZone_t* pZone, Name_t name)
{
  if (!pZone) return INVALID_ALLOCATOR;
  /* Walk pZone's own block list directly.
   * We cannot use Z_ZoneAsBlock + FindBlock here: the root zone's anchor block
   * lives at the same address as pZone, so Block_AsSubZone on it would point
   * into the middle of the zone header, not the block list.
   * Child TYPE_ZONE blocks embedded inside a zone DO have their MemZone_t in
   * their payload (GetBlockData), which is what ZoneStrategy_FindBlock uses. */
  uint32_t anchorOff = Z_GetOffset(pZone, &pZone->blocklist);
  uint32_t currOff   = pZone->blocklist.next;
  while (currOff != anchorOff) {
    MemBlock_t* pChild = Z_GetBlock(pZone, currOff);
    if (pChild->tag != PU_FREE) {
      const char* childName = MEM_STRATEGY(pChild).GetBlockName(pChild);
      if (childName != NULL && name_equals(name_from_cstr(childName), name)) {
        return MEM_STRATEGY(pChild).GetAllocator(pChild);
      }
      const MemBlock_t* pFound = MEM_STRATEGY(pChild).FindBlock(pChild, name);
      if (Block_IsValid(pFound)) {
        return MEM_STRATEGY(pFound).GetAllocator((MemBlock_t*)pFound);
      }
    }
    currOff = pChild->next;
  }
  return INVALID_ALLOCATOR;
}

/* Load the zone and immediately fix handles */
MemZone_t* Z_Load(Allocator_t* pAllocator, const char *filename){
  FILE* pFile = fopen(filename, "rb");
  if (!pFile) return NULL;
  // Read the header first to get the capacity 
  MemZone_t header;
  fread(&header, kMEM_ZONE_SIZE, 1, pFile);
  fseek(pFile, 0, SEEK_SET);
  // Allocate the full buffer (wherever the OS gives it to us)
  Handle_t h = pAllocator->Malloc(pAllocator->pContext, header.capacity, NULL);
  void* pData = h.pData;
  fread(pData, header.capacity, 1, pFile);
  fclose(pFile);

  MemZone_t* pZone = (MemZone_t*)pData;  
  // The handles in ppUser haven't changed, but the data has moved.
  Z_Remap(pZone);
  return pZone;
}
