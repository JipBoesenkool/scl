#include <catch2/catch_test_macros.hpp>
#include <cLib/string/cStringBuilder.h>
#include <string.h>

// Helper: compare builder contents to a C string literal
static bool sb_eq(const StringBuilder_t* sb, const char* lit) {
    StringView_t sv = sb_to_sv(sb);
    size_t litLen = strlen(lit);
    if (sv.length != litLen) return false;
    return memcmp(sv.data, lit, litLen) == 0;
}

// ─────────────────────────────────────────────
//  Lifecycle
// ─────────────────────────────────────────────
TEST_CASE("StringBuilder – init sets up clean state", "[StringBuilder]") {
    char raw[64];
    Buffer_t mem = { .pData = (uint8_t*)raw, .size = sizeof(raw) };
    StringBuilder_t sb;
    sb_init(&sb, mem);

    REQUIRE(sb.length == 0);
    REQUIRE(sb.capacity == 64);
    REQUIRE(raw[0] == '\0');
    REQUIRE(sv_is_empty(sb_to_sv(&sb)));
}

TEST_CASE("StringBuilder – sb_clear resets without freeing", "[StringBuilder]") {
    char raw[64];
    Buffer_t mem = { .pData = (uint8_t*)raw, .size = sizeof(raw) };
    StringBuilder_t sb;
    sb_init(&sb, mem);

    sb_append_sv(&sb, SV("Hello"));
    REQUIRE(sb.length == 5);

    sb_clear(&sb);
    REQUIRE(sb.length == 0);
    REQUIRE(raw[0] == '\0');
    REQUIRE(sb.capacity == 64); // buffer still intact
}

// ─────────────────────────────────────────────
//  Appending
// ─────────────────────────────────────────────
TEST_CASE("StringBuilder – sb_append_sv", "[StringBuilder]") {
    char raw[64];
    Buffer_t mem = { .pData = (uint8_t*)raw, .size = sizeof(raw) };
    StringBuilder_t sb;
    sb_init(&sb, mem);

    sb_append_sv(&sb, SV("Hello"));
    sb_append_sv(&sb, SV(", "));
    sb_append_sv(&sb, SV("World"));

    REQUIRE(sb.length == 12);
    REQUIRE(sb_eq(&sb, "Hello, World"));

    SECTION("appending INVALID_SV is a no-op") {
        size_t before = sb.length;
        sb_append_sv(&sb, INVALID_SV);
        REQUIRE(sb.length == before);
    }
}

TEST_CASE("StringBuilder – sb_append_format", "[StringBuilder]") {
    char raw[64];
    Buffer_t mem = { .pData = (uint8_t*)raw, .size = sizeof(raw) };
    StringBuilder_t sb;
    sb_init(&sb, mem);

    sb_append_sv(&sb, SV("v"));
    sb_append_format(&sb, "%d.%d.%d", 1, 2, 3);

    REQUIRE(sb_eq(&sb, "v1.2.3"));

    SECTION("empty format string is a no-op") {
        sb_clear(&sb);
        sb_append_format(&sb, "%s", "");
        REQUIRE(sb.length == 0);
        REQUIRE(sv_is_empty(sb_to_sv(&sb)));
    }

    SECTION("format-only append with no prior content") {
        sb_clear(&sb);
        sb_append_format(&sb, "x=%d", 42);
        REQUIRE(sb_eq(&sb, "x=42"));
    }
}

TEST_CASE("StringBuilder – mixed append", "[StringBuilder]") {
    char raw[128];
    Buffer_t mem = { .pData = (uint8_t*)raw, .size = sizeof(raw) };
    StringBuilder_t sb;
    sb_init(&sb, mem);

    sb_append_sv(&sb, SV("Gemini"));
    sb_append_sv(&sb, SV(" Protocol"));
    sb_append_format(&sb, " v%d.%d", 3, 0);

    REQUIRE(sb_eq(&sb, "Gemini Protocol v3.0"));
}

// ─────────────────────────────────────────────
//  Boundary
// ─────────────────────────────────────────────
TEST_CASE("StringBuilder – fill to capacity-1", "[StringBuilder]") {
    char raw[128];
    Buffer_t mem = { .pData = (uint8_t*)raw, .size = sizeof(raw) };
    StringBuilder_t sb;
    sb_init(&sb, mem);

    // 12 × 10 = 120 chars
    for (int i = 0; i < 12; ++i) {
        sb_append_sv(&sb, SV("1234567890"));
    }
    REQUIRE(sb.length == 120);

    // 7 more to reach 127 (capacity-1)
    sb_append_sv(&sb, SV("ABCDEFG"));
    REQUIRE(sb.length == 127);
    REQUIRE(raw[127] == '\0');
}

// ─────────────────────────────────────────────
//  Conversion
// ─────────────────────────────────────────────
TEST_CASE("StringBuilder – sb_to_sv reflects current content", "[StringBuilder]") {
    char raw[32];
    Buffer_t mem = { .pData = (uint8_t*)raw, .size = sizeof(raw) };
    StringBuilder_t sb;
    sb_init(&sb, mem);

    sb_append_sv(&sb, SV("Test"));
    StringView_t sv = sb_to_sv(&sb);
    REQUIRE(sv.length == 4);
    REQUIRE(sv_compare(sv, SV("Test")) == 0);
}

TEST_CASE("StringBuilder – sb_to_sv on empty builder returns INVALID_SV", "[StringBuilder]") {
    char raw[32];
    Buffer_t mem = { .pData = (uint8_t*)raw, .size = sizeof(raw) };
    StringBuilder_t sb;
    sb_init(&sb, mem);

    REQUIRE(sv_is_empty(sb_to_sv(&sb)));
}

// ─────────────────────────────────────────────
//  Typed appenders
// ─────────────────────────────────────────────
TEST_CASE("StringBuilder – sb_append_char", "[StringBuilder]") {
    char raw[32];
    Buffer_t mem = { .pData = (uint8_t*)raw, .size = sizeof(raw) };
    StringBuilder_t sb;
    sb_init(&sb, mem);

    sb_append_char(&sb, 'A');
    sb_append_char(&sb, 'B');
    sb_append_char(&sb, 'C');

    REQUIRE(sb.length == 3);
    REQUIRE(sb_eq(&sb, "ABC"));

    SECTION("appending many chars builds correct string") {
        sb_clear(&sb);
        for (int i = 0; i < 5; ++i) sb_append_char(&sb, 'x');
        REQUIRE(sb_eq(&sb, "xxxxx"));
    }
}

TEST_CASE("StringBuilder – sb_append_cstr", "[StringBuilder]") {
    char raw[64];
    Buffer_t mem = { .pData = (uint8_t*)raw, .size = sizeof(raw) };
    StringBuilder_t sb;
    sb_init(&sb, mem);

    sb_append_cstr(&sb, "Hello");
    sb_append_cstr(&sb, ", ");
    sb_append_cstr(&sb, "World");
    REQUIRE(sb_eq(&sb, "Hello, World"));

    SECTION("empty string is a no-op") {
        size_t before = sb.length;
        sb_append_cstr(&sb, "");
        REQUIRE(sb.length == before);
    }

    SECTION("nullptr is a no-op") {
        size_t before = sb.length;
        sb_append_cstr(&sb, nullptr);
        REQUIRE(sb.length == before);
    }
}

TEST_CASE("StringBuilder – sb_append_string", "[StringBuilder]") {
    char raw[64];
    Buffer_t mem = { .pData = (uint8_t*)raw, .size = sizeof(raw) };
    StringBuilder_t sb;
    sb_init(&sb, mem);

    String_t sso  = string_create("SSO");       // stack-allocated
    String_t heap = string_create("123456789012345678901234"); // heap (24 chars)

    sb_append_string(&sb, &sso);
    sb_append_char(&sb, '-');
    sb_append_string(&sb, &heap);
    REQUIRE(sb_eq(&sb, "SSO-123456789012345678901234"));

    SECTION("INVALID_STRING is a no-op") {
        sb_clear(&sb);
        String_t empty = INVALID_STRING;
        sb_append_string(&sb, &empty);
        REQUIRE(sb.length == 0);
    }

    string_destroy(&sso);
    string_destroy(&heap);
}

TEST_CASE("StringBuilder – sb_append_i32", "[StringBuilder]") {
    char raw[64];
    Buffer_t mem = { .pData = (uint8_t*)raw, .size = sizeof(raw) };
    StringBuilder_t sb;
    sb_init(&sb, mem);

    sb_append_i32(&sb, 42);
    REQUIRE(sb_eq(&sb, "42"));

    SECTION("zero") {
        sb_clear(&sb);
        sb_append_i32(&sb, 0);
        REQUIRE(sb_eq(&sb, "0"));
    }

    SECTION("negative") {
        sb_clear(&sb);
        sb_append_i32(&sb, -7);
        REQUIRE(sb_eq(&sb, "-7"));
    }

    SECTION("INT32_MAX") {
        sb_clear(&sb);
        sb_append_i32(&sb, 2147483647);
        REQUIRE(sb_eq(&sb, "2147483647"));
    }

    SECTION("INT32_MIN") {
        sb_clear(&sb);
        sb_append_i32(&sb, -2147483648);
        REQUIRE(sb_eq(&sb, "-2147483648"));
    }
}

TEST_CASE("StringBuilder – sb_append_u32", "[StringBuilder]") {
    char raw[32];
    Buffer_t mem = { .pData = (uint8_t*)raw, .size = sizeof(raw) };
    StringBuilder_t sb;
    sb_init(&sb, mem);

    sb_append_u32(&sb, 0);
    REQUIRE(sb_eq(&sb, "0"));

    SECTION("UINT32_MAX") {
        sb_clear(&sb);
        sb_append_u32(&sb, 4294967295u);
        REQUIRE(sb_eq(&sb, "4294967295"));
    }
}

TEST_CASE("StringBuilder – sb_append_f32", "[StringBuilder]") {
    char raw[32];
    Buffer_t mem = { .pData = (uint8_t*)raw, .size = sizeof(raw) };
    StringBuilder_t sb;
    sb_init(&sb, mem);

    SECTION("zero") {
        sb_append_f32(&sb, 0.0f);
        REQUIRE(sb_eq(&sb, "0"));
    }

    SECTION("positive fractional — uses %g formatting") {
        sb_append_f32(&sb, 1.5f);
        REQUIRE(sb_eq(&sb, "1.5"));
    }

    SECTION("negative") {
        sb_append_f32(&sb, -3.0f);
        REQUIRE(sb_eq(&sb, "-3"));
    }
}

TEST_CASE("StringBuilder – sb_append_bool", "[StringBuilder]") {
    char raw[16];
    Buffer_t mem = { .pData = (uint8_t*)raw, .size = sizeof(raw) };
    StringBuilder_t sb;
    sb_init(&sb, mem);

    sb_append_bool(&sb, true);
    REQUIRE(sb_eq(&sb, "true"));

    sb_clear(&sb);
    sb_append_bool(&sb, false);
    REQUIRE(sb_eq(&sb, "false"));
}

TEST_CASE("StringBuilder – sb_to_string_alloc", "[StringBuilder]") {
    char raw[64];
    Buffer_t mem = { .pData = (uint8_t*)raw, .size = sizeof(raw) };
    StringBuilder_t sb;
    sb_init(&sb, mem);

    SECTION("empty builder returns INVALID_STRING") {
        String_t s = sb_to_string_alloc(&sb, nullptr);
        REQUIRE(string_is_empty(&s));
        string_destroy(&s);
    }

    SECTION("nullptr allocator falls back to malloc") {
        sb_append_cstr(&sb, "hello");
        String_t s = sb_to_string_alloc(&sb, nullptr);
        REQUIRE(strcmp(string_to_c_str(&s), "hello") == 0);
        string_destroy(&s);
    }
}

TEST_CASE("StringBuilder – sb_to_string produces deep copy", "[StringBuilder]") {
    char raw[64];
    Buffer_t mem = { .pData = (uint8_t*)raw, .size = sizeof(raw) };
    StringBuilder_t sb;
    sb_init(&sb, mem);

    sb_append_sv(&sb, SV("Gemini Protocol v3.0"));
    String_t result = sb_to_string(&sb);

    REQUIRE(string_length(&result) == sb.length);
    REQUIRE(strcmp(string_to_c_str(&result), "Gemini Protocol v3.0") == 0);

    // Deep copy — clearing builder must not affect string
    sb_clear(&sb);
    REQUIRE(string_length(&result) == 20);

    string_destroy(&result);
}
