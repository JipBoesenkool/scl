#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "cLib/memory/zone/MemBlock.h"
#include "cLib/memory/allocator/Allocator.h"
#include "cLib/memory/allocator/ZoneAllocator.h"
#include "cLib/memory/zone/Zone.h"

/* =========================================================================
 * Test_Align
 * Align rounds up to the nearest ALIGN_BYTES (8) boundary.
 * ========================================================================= */
TEST_CASE("MemBlock – Align", "[MemBlock]") {
  REQUIRE(Align(0) == 0u);
  REQUIRE(Align(1) == 8u);
  REQUIRE(Align(7) == 8u);
  REQUIRE(Align(8) == 8u);
  REQUIRE(Align(9) == 16u);
  REQUIRE(Align(15) == 16u);
  REQUIRE(Align(16) == 16u);
  REQUIRE(Align(17) == 24u);
  // Already-aligned large value stays the same.
  REQUIRE(Align(1024) == 1024u);
  // One byte over a boundary rounds up by a full ALIGN_BYTES.
  REQUIRE(Align(1025) == 1032u);
  // Result must always be a multiple of ALIGN_BYTES.
  for (uint32_t s = 0; s <= 256; ++s)
    REQUIRE(Align(s) % ALIGN_BYTES == 0);
}

/* =========================================================================
 * Test_BlockPointerRoundtrip
 * GetBlockData(GetBlockHeader(pData)) == pData
 * GetBlockHeader(GetBlockData(pBlock)) == pBlock
 * ========================================================================= */
TEST_CASE("MemBlock – Pointer Roundtrip", "[MemBlock]") {
  // Create a properly aligned byte buffer large enough for a header + payload.
  alignas(8) uint8_t buf[128] = {};
  MemBlock_t* pBlock = reinterpret_cast<MemBlock_t*>(buf);
  void*       pData  = GetBlockData(pBlock);

  REQUIRE(pData != nullptr);
  REQUIRE((uint8_t*)pData == (uint8_t*)pBlock + kMEM_BLOCK_SIZE);
  REQUIRE(GetBlockHeader(pData) == pBlock);
}

/* =========================================================================
 * Test_BlockPointerNullGuards
 * GetBlockHeader(NULL) and GetBlockData(NULL) must return NULL.
 * ========================================================================= */
TEST_CASE("MemBlock – Pointer Null Guards", "[MemBlock]") {
  REQUIRE(GetBlockHeader(nullptr) == nullptr);
  REQUIRE(GetBlockData(nullptr) == nullptr);
}

/* =========================================================================
 * Test_BlockSize
 * kMEM_BLOCK_SIZE must equal sizeof(MemBlock_t) rounded up to ALIGN_BYTES.
 * The static_assert in the header already enforces sizeof == 32, but this
 * test makes the relationship explicit and detectable at runtime.
 * ========================================================================= */
TEST_CASE("MemBlock – Block Size", "[MemBlock]") {
  uint32_t expected = (sizeof(MemBlock_t) + ALIGN_MASK) & ~ALIGN_MASK;
  REQUIRE(kMEM_BLOCK_SIZE == expected);
  REQUIRE(kMEM_BLOCK_SIZE == 32u); // MemBlock_t is 32 bytes, already aligned.
}

/* =========================================================================
 * Test_BlockSaveLoad
 * Block_Save serialises header metadata + payload to disk.
 * Block_Load allocates a fresh block, reads back the data, and restores it.
 * ========================================================================= */
TEST_CASE("MemBlock – Save and Load", "[MemBlock]") {
  const char* kFile = "memblock_test.bin";
  const char* kMsg  = "BlockSaveLoadPayload";

  // Set up a zone to allocate from.
  uint32_t   capacity = 1024 * 8;
  void*      pRaw     = malloc(capacity);
  MemZone_t* pZone    = Z_Init((MemZone_t*)pRaw, capacity, PU_STATIC, "BLK_ZONE");

  void* pDataHandle = nullptr;
  void* pData = Z_Malloc(pZone, 64, PU_STATIC, &pDataHandle);
  REQUIRE(pData != nullptr);
  strcpy((char*)pData, kMsg);

  MemBlock_t* pBlock = GetBlockHeader(pData);
  REQUIRE(Block_Save(pBlock, kFile));

  // Free and null the handle so Load into a fresh slot.
  Z_Free(pZone, pData);
  pDataHandle = nullptr;

  // Load back via zone allocator.
  Allocator_t  alloc       = Zone_GetAllocator(pZone);
  MemBlock_t*  pLoadedBlock = nullptr;
  void*        pLoaded     = Block_Load(&alloc, &pLoadedBlock, kFile);

  REQUIRE(pLoaded != nullptr);
  REQUIRE(pLoadedBlock != nullptr);
  REQUIRE(pLoadedBlock->tag == (uint32_t)PU_STATIC);
  REQUIRE(strcmp((char*)pLoaded, kMsg) == 0);

  remove(kFile);
  free(pRaw);
}

/* =========================================================================
 * Test_BlockSaveNullGuards
 * Block_Save returns false on null arguments and must not crash.
 * ========================================================================= */
TEST_CASE("MemBlock – Save and Load Null Guards", "[MemBlock]") {
  REQUIRE_FALSE(Block_Save(nullptr, "x.bin"));
  // Block_Load with null args must return null.
  MemBlock_t* out = nullptr;
  Allocator_t dummy = {};
  REQUIRE(Block_Load(nullptr, &out, "x.bin") == nullptr);
  REQUIRE(Block_Load(&dummy, nullptr, "x.bin") == nullptr);
  REQUIRE(Block_Load(&dummy, &out, nullptr) == nullptr);
}

/* =========================================================================
 * Test_kInvalidBlock
 * kInvalidBlock has magic != CHECK_SUM, so Block_IsValid returns false.
 * ========================================================================= */
TEST_CASE("MemBlock – Invalid Block", "[MemBlock]") {
  REQUIRE_FALSE(Block_IsValid(&kInvalidBlock));
  REQUIRE(kInvalidBlock.magic != (uint32_t)CHECK_SUM);
  REQUIRE(kInvalidBlock.type == (uint32_t)TYPE_NONE);
}

/* =========================================================================
 * Test_BlockIsValid_NullGuard
 * Block_IsValid(nullptr) must return false without crashing.
 * ========================================================================= */
TEST_CASE("MemBlock – Block_IsValid Null Guard", "[MemBlock]") {
  REQUIRE_FALSE(Block_IsValid(nullptr));
}

/* =========================================================================
 * Test_BlockIsValid_ValidBlock
 * A block initialised by the zone allocator has magic == CHECK_SUM.
 * ========================================================================= */
TEST_CASE("MemBlock – Block_IsValid Valid Block", "[MemBlock]") {
  void*      pBuf  = malloc(1024 * 4);
  MemZone_t* pZone = Z_Init((MemZone_t*)pBuf, 1024 * 4, PU_STATIC, "BIV");
  void*      pData = Z_Malloc(pZone, 64, PU_STATIC, nullptr);
  REQUIRE(pData != nullptr);
  MemBlock_t* pBlock = GetBlockHeader(pData);
  REQUIRE(Block_IsValid(pBlock));
  free(pBuf);
}
