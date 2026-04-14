#include <catch2/catch_test_macros.hpp>
#include <cLib/memory/allocator/DefaultAllocator.h>
#include <cLib/string/cString.h>
#include <cLib/string/cStringBuilder.h>
#include <cstring>

// ─────────────────────────────────────────────
//  DefaultAllocator — basic round-trips
// ─────────────────────────────────────────────
TEST_CASE("DefaultAllocator – Malloc returns valid pointer", "[DefaultAllocator]") {
    Allocator_t alloc = Default_GetAllocator();
    Handle_t h = alloc.Malloc(alloc.pContext, 64, nullptr);
    REQUIRE(h.pData != nullptr);
    alloc.Free(alloc.pContext, &h);
    REQUIRE(h.pData == nullptr);
}

TEST_CASE("DefaultAllocator – Malloc with pUser back-pointer", "[DefaultAllocator]") {
    Allocator_t alloc = Default_GetAllocator();
    void* backPtr = nullptr;
    Handle_t h = alloc.Malloc(alloc.pContext, 32, &backPtr);
    REQUIRE(h.pData != nullptr);
    REQUIRE(backPtr == h.pData);
    alloc.Free(alloc.pContext, &h);
}

TEST_CASE("DefaultAllocator – Realloc grows buffer", "[DefaultAllocator]") {
    Allocator_t alloc = Default_GetAllocator();
    Handle_t h = alloc.Malloc(alloc.pContext, 16, nullptr);
    REQUIRE(h.pData != nullptr);
    memcpy(h.pData, "hello", 6);

    Handle_t h2 = alloc.Realloc(alloc.pContext, &h, 64);
    REQUIRE(h2.pData != nullptr);
    REQUIRE(strcmp((const char*)h2.pData, "hello") == 0);
    alloc.Free(alloc.pContext, &h2);
}

TEST_CASE("DefaultAllocator – GetData returns handle pData", "[DefaultAllocator]") {
    Allocator_t alloc = Default_GetAllocator();
    Handle_t h = alloc.Malloc(alloc.pContext, 8, nullptr);
    REQUIRE(alloc.GetData(alloc.pContext, &h) == h.pData);
    alloc.Free(alloc.pContext, &h);
}

// ─────────────────────────────────────────────
//  Allocator structure validation
// ─────────────────────────────────────────────
TEST_CASE("DefaultAllocator – Default_GetAllocator returns non-null function pointers", "[DefaultAllocator]") {
    Allocator_t alloc = Default_GetAllocator();
    REQUIRE(alloc.Malloc   != nullptr);
    REQUIRE(alloc.Realloc  != nullptr);
    REQUIRE(alloc.Free     != nullptr);
    REQUIRE(alloc.GetData  != nullptr);
    REQUIRE(alloc.Clear    != nullptr);
}

TEST_CASE("DefaultAllocator – sequential Malloc calls produce increasing IDs", "[DefaultAllocator]") {
    Allocator_t alloc = Default_GetAllocator();
    Handle_t h1 = alloc.Malloc(alloc.pContext, 8, nullptr);
    Handle_t h2 = alloc.Malloc(alloc.pContext, 8, nullptr);
    REQUIRE(h2.id > h1.id);
    alloc.Free(alloc.pContext, &h1);
    alloc.Free(alloc.pContext, &h2);
}

TEST_CASE("DefaultAllocator – Malloc(size=0) does not crash", "[DefaultAllocator]") {
    Allocator_t alloc = Default_GetAllocator();
    // Behaviour is implementation-defined for size=0; we only require it does not crash.
    Handle_t h = alloc.Malloc(alloc.pContext, 0, nullptr);
    // id should still be assigned (sequential counter always increments)
    REQUIRE(h.id > 0);
    alloc.Free(alloc.pContext, &h);
}

TEST_CASE("DefaultAllocator – Realloc on null handle behaves like Malloc", "[DefaultAllocator]") {
    Allocator_t alloc = Default_GetAllocator();
    Handle_t null_h = { 0, 0, nullptr };
    Handle_t h = alloc.Realloc(alloc.pContext, &null_h, 32);
    REQUIRE(h.pData != nullptr);
    alloc.Free(alloc.pContext, &h);
}

TEST_CASE("DefaultAllocator – Free with null handle pointer does not crash", "[DefaultAllocator]") {
    Allocator_t alloc = Default_GetAllocator();
    // Pass nullptr as the handle pointer — must not crash.
    alloc.Free(alloc.pContext, nullptr);
}

TEST_CASE("DefaultAllocator – Free on handle with null pData does not crash", "[DefaultAllocator]") {
    Allocator_t alloc = Default_GetAllocator();
    Handle_t h = { 0, 0, nullptr };
    // pData is already null — Free must be a no-op.
    alloc.Free(alloc.pContext, &h);
}

// ─────────────────────────────────────────────
//  string_create_alloc / string_destroy_alloc
// ─────────────────────────────────────────────
TEST_CASE("string_create_alloc – SSO string does not touch allocator", "[DefaultAllocator][string]") {
    Allocator_t alloc = Default_GetAllocator();
    // "Hello" (5 chars) is SSO — stays on stack
    String_t s = string_create_alloc("Hello", &alloc);
    REQUIRE(string_length(&s) == 5);
    REQUIRE(strcmp(string_to_c_str(&s), "Hello") == 0);
    string_destroy_alloc(&s, &alloc);
    REQUIRE(string_is_empty(&s));
}

TEST_CASE("string_create_alloc – heap string allocated via allocator", "[DefaultAllocator][string]") {
    Allocator_t alloc = Default_GetAllocator();
    // 24+ chars forces heap allocation
    const char* long_str = "123456789012345678901234"; // 24 chars
    String_t s = string_create_alloc(long_str, &alloc);
    REQUIRE(string_length(&s) == 24);
    REQUIRE(strcmp(string_to_c_str(&s), long_str) == 0);
    string_destroy_alloc(&s, &alloc);
    REQUIRE(string_is_empty(&s));
}

TEST_CASE("string_create_alloc – NULL allocator falls back to malloc", "[DefaultAllocator][string]") {
    String_t s = string_create_alloc("Hello", nullptr);
    REQUIRE(string_length(&s) == 5);
    string_destroy_alloc(&s, nullptr);
    REQUIRE(string_is_empty(&s));
}

TEST_CASE("string_create_alloc – SSO string: allocator Malloc is NOT called", "[DefaultAllocator][string]") {
    // We verify indirectly: create a string that fits in SSO, then check capacity
    // reflects the stack layout (capacity == length, not length+1).
    Allocator_t alloc = Default_GetAllocator();
    String_t s = string_create_alloc("Short", &alloc); // 5 chars — SSO
    REQUIRE(s.capacity == 5);  // SSO sets capacity = length (not length+1)
    string_destroy_alloc(&s, &alloc);
}

TEST_CASE("string_destroy_alloc – SSO string with non-null allocator does not crash", "[DefaultAllocator][string]") {
    Allocator_t alloc = Default_GetAllocator();
    String_t s = string_create_alloc("SSO", &alloc); // 3 chars — SSO
    REQUIRE(s.capacity == 3);
    // Must not call pAlloc->Free (stack data, no heap block to free)
    string_destroy_alloc(&s, &alloc);
    REQUIRE(string_is_empty(&s));
}

TEST_CASE("string_destroy_alloc – heap string with nullptr allocator falls back to free()", "[DefaultAllocator][string]") {
    const char* long_str = "this_is_definitely_a_heap_string_x"; // 34 chars
    String_t s = string_create_alloc(long_str, nullptr); // malloc fallback
    REQUIRE(string_length(&s) == 34);
    string_destroy_alloc(&s, nullptr); // must not crash — falls back to free()
    REQUIRE(string_is_empty(&s));
}

TEST_CASE("string_create_alloc – inspection functions work on allocator-managed strings", "[DefaultAllocator][string]") {
    Allocator_t alloc = Default_GetAllocator();
    const char* long_str = "123456789012345678901234"; // 24 chars — heap
    String_t s = string_create_alloc(long_str, &alloc);

    REQUIRE(string_length(&s)    == 24);
    REQUIRE(string_at(&s, 0)     == '1');
    REQUIRE(string_at(&s, 23)    == '4');
    REQUIRE(string_at(&s, 99)    == '\0'); // out-of-bounds → '\0'

    String_t s2 = string_create_alloc(long_str, &alloc);
    REQUIRE(string_compare(&s, &s2) == 0);

    StringView_t sv = string_to_sv(&s);
    REQUIRE(sv.length == 24);
    REQUIRE(memcmp(sv.data, long_str, 24) == 0);

    string_destroy_alloc(&s,  &alloc);
    string_destroy_alloc(&s2, &alloc);
}

TEST_CASE("string_copy_alloc – produces independent copy", "[DefaultAllocator][string]") {
    Allocator_t alloc = Default_GetAllocator();
    String_t src = string_create("Original");
    String_t dst = string_copy_alloc(&src, &alloc);
    REQUIRE(string_compare(&src, &dst) == 0);
    REQUIRE(string_to_c_str(&src) != string_to_c_str(&dst));
    string_destroy(&src);
    string_destroy_alloc(&dst, &alloc);
}

// ─────────────────────────────────────────────
//  sb_to_string_alloc
// ─────────────────────────────────────────────
TEST_CASE("sb_to_string_alloc – produces correct string via allocator", "[DefaultAllocator][StringBuilder]") {
    Allocator_t alloc = Default_GetAllocator();
    char buf[64];
    Buffer_t mem64 = { .pData = (uint8_t*)buf, .size = sizeof(buf) };
    StringBuilder_t sb;
    sb_init(&sb, mem64);
    sb_append_cstr(&sb, "foo");
    sb_append_cstr(&sb, "bar");
    String_t s = sb_to_string_alloc(&sb, &alloc);
    REQUIRE(strcmp(string_to_c_str(&s), "foobar") == 0);
    string_destroy_alloc(&s, &alloc);
}

TEST_CASE("sb_to_string_alloc – NULL allocator falls back to malloc", "[DefaultAllocator][StringBuilder]") {
    char buf[32];
    Buffer_t mem32 = { .pData = (uint8_t*)buf, .size = sizeof(buf) };
    StringBuilder_t sb;
    sb_init(&sb, mem32);
    sb_append_cstr(&sb, "test");
    String_t s = sb_to_string_alloc(&sb, nullptr);
    REQUIRE(strcmp(string_to_c_str(&s), "test") == 0);
    string_destroy(&s);
}
