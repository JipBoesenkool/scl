#include "cLib/memory/zone/ZoneStats.h"
#include "cLib/memory/zone/Zone.h"
#include "cLib/Log.h"

uint32_t Z_GetPurgeableSpace(MemZone_t* pZone)
{
  uint32_t result = 0;
  const uint32_t anchorOffset = Z_GetOffset(pZone, &pZone->blocklist);
  uint32_t currOffset = pZone->blocklist.next;

  while (currOffset != anchorOffset) {
    MemBlock_t* pBlock = Z_GetBlock(pZone, currOffset);
    // ONLY count blocks that are truly empty
    if (pBlock->tag > PU_LOCKED) {
      result += pBlock->size;
    }
    currOffset = pBlock->next;
  }
  return result;
}

uint32_t Z_GetUsedSpace(MemZone_t* pZone, uint32_t tag)
{
  uint32_t result = 0;
  const uint32_t anchorOffset = Z_GetOffset(pZone, &pZone->blocklist);
  uint32_t currOffset = pZone->blocklist.next;

  while (currOffset != anchorOffset) {
    MemBlock_t* pBlock = Z_GetBlock(pZone, currOffset);
    // ONLY count blocks that are truly empty
    if (pBlock->tag == tag) {
      result += pBlock->size;
    }
    currOffset = pBlock->next;
  }
  return result;
}

// --- Debugging ---
bool Z_CheckRange(MemZone_t* pZone, uint32_t minTag, uint32_t maxTag)
{
  Log_Add("[STATUS] Z_CheckZone: %s.", pZone->name.name);
  Log_Add("\tZone size: %i\tlocation: %p\n", pZone->capacity, pZone);
  Log_Add("\tTag range: %3i - %3i\n", minTag, maxTag);

  /* Start at the first real block after the anchor */
  uint32_t anchorOffset = Z_GetOffset(pZone, &pZone->blocklist);
  uint32_t currOffset = pZone->blocklist.next;
  while (currOffset != anchorOffset) { // 0 is the offset of the blocklist anchor
    MemBlock_t* pCurrBlock = Z_GetBlock(pZone, currOffset);
    // Only log if within the range
    if (pCurrBlock->tag >= minTag && pCurrBlock->tag <= maxTag)
    {
      Log_Add("\tblock:%p\tsize:%7i\tuser:%p\ttag:%3i",
        pCurrBlock, pCurrBlock->size, pCurrBlock->ppUser, pCurrBlock->tag);
    }
    /* 1. Validate Magic Number */
    if (pCurrBlock->magic != CHECK_SUM) {
      Log_Add("[ERROR] Z_CheckZone: memory corruption at %p (invalid magic)\n", pCurrBlock);
      return false;
    }
    /* 2. Check Physical Contiguity: Does this block touch the next one? */
    MemBlock_t* pNextBlock = Z_GetBlock(pZone, pCurrBlock->next);
    // If next isn't the anchor, ensure the memory is contiguous
    if (pCurrBlock->next != anchorOffset) {
      if (Z_GetBlock(pZone, currOffset + pCurrBlock->size) != pNextBlock) {
        Log_Add("[ERROR] Z_CheckZone: block at %p does not touch next block\n", pCurrBlock);
        return false;
      }
    }
    /* 3. Check Back-Link Integrity */
    if (Z_GetBlock(pZone, pNextBlock->prev) != pCurrBlock) {
      Log_Add("[ERROR] Z_CheckZone: next block %p has broken back-link (prev)\n", pNextBlock);
      return false;
    }
    /* 4. Check for illegal double-free (consecutive free blocks) */
    if (pCurrBlock->tag == PU_FREE && pNextBlock->tag == PU_FREE && pCurrBlock->next != anchorOffset) {
      Log_Add("[ERROR] Z_CheckZone: non-consolidated free blocks at %p and %p\n", pCurrBlock, pNextBlock);
      return false;
    }
    /* Move to the next block in the chain */
    currOffset = pCurrBlock->next;
  }
  Log_Add("[STATUS] Z_CheckZone: %s is valid!", pZone->name.name);
  return true;
}

bool Z_Check(MemZone_t* pZone)
{
  Log_Add("[STATUS] Z_CheckZone: %s.", pZone->name.name);
  /* Start at the first real block after the anchor */
  uint32_t anchorOffset = Z_GetOffset(pZone, &pZone->blocklist);
  uint32_t currOffset = pZone->blocklist.next;
  while (currOffset != anchorOffset) { // 0 is the offset of the blocklist anchor
    MemBlock_t* pCurrBlock = Z_GetBlock(pZone, currOffset);

    Log_Add("\tblock:%p\tsize:%7i\tuser:%p\ttag:%3i",
      pCurrBlock, pCurrBlock->size, pCurrBlock->ppUser, pCurrBlock->tag);

    /* 1. Validate Magic Number */
    if (pCurrBlock->magic != CHECK_SUM) {
      Log_Add("[ERROR] Z_CheckZone: memory corruption at %p (invalid magic)\n", pCurrBlock);
      return false;
    }

    /* 2. Check Physical Contiguity: Does this block touch the next one? */
    MemBlock_t* pNextBlock = Z_GetBlock(pZone, pCurrBlock->next);

    // If next isn't the anchor, ensure the memory is contiguous
    if (pCurrBlock->next != anchorOffset) {
      if (Z_GetBlock(pZone, currOffset + pCurrBlock->size) != pNextBlock) {
        Log_Add("[ERROR] Z_CheckZone: block at %p does not touch next block\n", pCurrBlock);
        return false;
      }
    }

    /* 3. Check Back-Link Integrity */
    if (Z_GetBlock(pZone, pNextBlock->prev) != pCurrBlock) {
      Log_Add("[ERROR] Z_CheckZone: next block %p has broken back-link (prev)\n", pNextBlock);
      return false;
    }

    /* 4. Check for illegal double-free (consecutive free blocks) */
    if (pCurrBlock->tag == PU_FREE && pNextBlock->tag == PU_FREE && pCurrBlock->next != anchorOffset) {
      Log_Add("[ERROR] Z_CheckZone: non-consolidated free blocks at %p and %p\n", pCurrBlock, pNextBlock);
      return false;
    }

    /* Move to the next block in the chain */
    currOffset = pCurrBlock->next;
  }

  Log_Add("[STATUS] Z_CheckZone: %s is valid!", pZone->name.name);
  return true;
}

/**
 * Z_Verify
 * Internal consistency check: Total Capacity == Used + Free.
 */
bool Z_Verify(MemZone_t* pZone) {
  uint32_t freeSpace = Z_GetUsedSpace(pZone, PU_FREE);
  uint32_t totalAccounted = pZone->used + freeSpace;
  if (totalAccounted != pZone->capacity) {
    Log_Add("[ERROR] Accounting Mismatch in %s!", pZone->name.name);
    Log_Add("        Expected: %u | Actual: %u (Delta: %d)",
      pZone->capacity, totalAccounted, (int)pZone->capacity - (int)totalAccounted);
    return false;
  }
  return true;
}
