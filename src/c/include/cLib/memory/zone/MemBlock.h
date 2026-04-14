#ifndef MEM_BLOCK_H
#define MEM_BLOCK_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cLib/cTypes.h"

#define CHECK_SUM         0x00C0FFEE
#define ALIGN_BYTES       8
#define ALIGN_MASK        (ALIGN_BYTES - 1)

//fwd decl
typedef struct cAllocator Allocator_t;

// --- Allocation Strategy Types ---
typedef enum {
  TYPE_NONE  = 0,  // NullStrategy
  TYPE_DATA  = 1,  // Raw leaf data (no sub-allocator)
  TYPE_ZONE  = 2,  // Payload is a MemZone_t
  TYPE_STACK = 3,  // Payload is a MemStack_t
} eMemType_t;

// --- Lifespan / Purge Tags ---
typedef enum {
  PU_FREE   = 0,    // Available to be claimed
  PU_STATIC = 1,    // Cannot be moved or purged
  PU_LEVEL  = 2,    // static until level exited
  PU_LOCAL  = 4,    // local to a function
  // Tags > 100 are purgable whenever needed.
  PU_LOCKED = 100,  // Purgable, but not right now
  PU_CACHE  = 101,  // Automatic purge if low memory
} eTag_t;

// TODO: Can we move *cName into MemBlock_t? 
// we need 8 bytes extra....
typedef struct cMemBlock
{
  uint32_t    magic;
  uint32_t    size;     // including the header
  uint32_t    tag;      // 4, eTag_t: Ownership and lifespan
  uint32_t    type;     // 4, eMemType_t: Strategy (Data, Zone, Stack)
  // navigation
  void**      ppUser;   // Pointer to the user's pointer (for remapping)
  uint32_t    next;     // next block or last
  uint32_t    prev;     // next block or offset
} MemBlock_t; // 32bytes
static_assert(sizeof(MemBlock_t) == 32, "MemBlock_t must be exactly 32 bytes");

static const uint32_t kMEM_BLOCK_SIZE = (sizeof(MemBlock_t) + ALIGN_MASK) & ~ALIGN_MASK;

// NullObject sentinel for "block not found". magic == 0 (not CHECK_SUM).
// Use Block_IsValid to test. Safe to pass to MEM_STRATEGY — kNullStrategy handles it.
extern const MemBlock_t kInvalidBlock;

static inline bool Block_IsValid(const MemBlock_t* pBlock) {
  return pBlock != NULL && pBlock->magic == CHECK_SUM;
}

uint32_t    Align(uint32_t size);
MemBlock_t* GetBlockHeader(void* pData);
void*       GetBlockData(MemBlock_t* pBlock);

bool  Block_Save(MemBlock_t* pBlock, const char* filename);
void* Block_Load(Allocator_t* pAllocator, MemBlock_t** outBlock, const char *filename);

#ifdef __cplusplus
} // extern "C"
#endif
#endif // MEM_BLOCK_H
