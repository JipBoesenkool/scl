# --- root ---
add_clib_test(LogTest memory/Log_test.cpp)

# Memory
# --- zone/ ---
add_clib_test(ZoneTest          memory/zone/Zone_test.cpp)
add_clib_test(ZoneStatsTest     memory/zone/ZoneStats_test.cpp)
add_clib_test(MemBlockTest      memory/zone/MemBlock_test.cpp)
# --- allocator/ ---
add_clib_test(ZoneAllocatorTest    memory/allocator/ZoneAllocator_test.cpp)
add_clib_test(StackAllocatorTest   memory/allocator/StackAllocator_test.cpp)
add_clib_test(DefaultAllocatorTest memory/allocator/DefaultAllocator_test.cpp)
add_clib_test(NullAllocatorTest    memory/allocator/NullAllocator_test.cpp)
# --- strategy/ ---
add_clib_test(MemStrategyTest memory/strategy/MemStrategy_test.cpp)
# --- context/ ---
add_clib_test(MemoryContextTest memory/cMemoryContext_test.cpp)

# STL
# string
add_clib_test(C-StringTest           string/cString_test.cpp)
add_clib_test(C-StringViewTest       string/cStringView_test.cpp)
add_clib_test(C-StringBuilderTest    string/cStringBuilder_test.cpp)
add_clib_test(C-NameTest             string/cName_test.cpp)

#collection
add_clib_test(C-CollectionTest       collection/Collection_test.cpp)
add_clib_test(C-ArrayTest            collection/cArray_test.cpp)

add_clib_test(C-NameTableTest        string/cNameTable_test.cpp)
