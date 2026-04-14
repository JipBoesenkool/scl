#include <catch2/catch_test_macros.hpp>
#include <cLib/string/cStringView.h>
#include <string.h>

// ─────────────────────────────────────────────
//  Creation & null states
// ─────────────────────────────────────────────
TEST_CASE("cStringView – creation", "[cStringView]") {
    SECTION("SV macro sets length from literal") {
        StringView_t v = SV("Rigid");
        REQUIRE(v.length == 5);
        REQUIRE(memcmp(v.data, "Rigid", 5) == 0);
    }

    SECTION("create_sv from raw pointer") {
        const char* ptr = "Engine";
        StringView_t v = create_sv(ptr);
        REQUIRE(v.length == 6);
        REQUIRE(v.data == ptr);
    }

    SECTION("create_sv(nullptr) returns INVALID_SV") {
        StringView_t v = create_sv(nullptr);
        REQUIRE(v.data == INVALID_SV.data);
        REQUIRE(v.length == 0);
        REQUIRE(sv_is_empty(v));
    }

    SECTION("empty literal yields length 0") {
        StringView_t v = SV("");
        REQUIRE(v.length == 0);
    }
}

// ─────────────────────────────────────────────
//  Inspection
// ─────────────────────────────────────────────
TEST_CASE("cStringView – sv_is_empty", "[cStringView]") {
    REQUIRE(sv_is_empty(INVALID_SV));
    REQUIRE(sv_is_empty(StringView_t{ nullptr, 0 }));
    REQUIRE_FALSE(sv_is_empty(SV("x")));
}

TEST_CASE("cStringView – sv_compare", "[cStringView]") {
    SECTION("equal views return 0") {
        REQUIRE(sv_compare(SV("Rigid"), SV("Rigid")) == 0);
    }

    SECTION("lexicographic ordering") {
        // 'R'(82) > 'E'(69)
        REQUIRE(sv_compare(SV("Rigid"), SV("Engine")) > 0);
        REQUIRE(sv_compare(SV("Engine"), SV("Rigid")) < 0);
    }

    SECTION("prefix is less than full string") {
        REQUIRE(sv_compare(SV("ab"), SV("abc")) < 0);
        REQUIRE(sv_compare(SV("abc"), SV("ab")) > 0);
    }
}

TEST_CASE("cStringView – sv_compare null-data guard", "[cStringView]") {
    StringView_t null_view = { nullptr, 5 }; // pathological: null data, non-zero length
    StringView_t valid     = SV("hello");
    StringView_t empty     = SV("");

    SECTION("both null-data → equal") {
        StringView_t other = { nullptr, 3 };
        REQUIRE(sv_compare(null_view, other) == 0);
    }

    SECTION("null-data left < non-empty right") {
        REQUIRE(sv_compare(null_view, valid) < 0);
    }

    SECTION("non-empty left > null-data right") {
        REQUIRE(sv_compare(valid, null_view) > 0);
    }

    SECTION("null-data vs zero-length non-null → null side is less") {
        // empty has non-null data but length 0; null_view has null data
        REQUIRE(sv_compare(null_view, empty) < 0);
        REQUIRE(sv_compare(empty, null_view) > 0);
    }
}

TEST_CASE("cStringView – starts_with / ends_with / contains", "[cStringView]") {
    StringView_t full = SV("  RigidEngine  ");

    REQUIRE(sv_starts_with(full, SV("  ")));
    REQUIRE(sv_ends_with(full, SV("  ")));
    REQUIRE(sv_contains(full, SV("Engine")));
    REQUIRE_FALSE(sv_contains(full, SV("Missing")));

    SECTION("prefix longer than view returns false") {
        REQUIRE_FALSE(sv_starts_with(SV("Hi"), SV("Hello")));
    }

    SECTION("suffix longer than view returns false") {
        REQUIRE_FALSE(sv_ends_with(SV("Hi"), SV("Hello")));
    }
}

// ─────────────────────────────────────────────
//  Search
// ─────────────────────────────────────────────
TEST_CASE("cStringView – sv_find / sv_rfind", "[cStringView]") {
    // "  RigidEngine  "
    //  0123456789...
    // 'i' at index 3 (Rigid) and 10 (Engine)
    StringView_t full = SV("  RigidEngine  ");

    REQUIRE(sv_find(full, SV("i")) == 3);
    REQUIRE(sv_rfind(full, SV("i")) == 10);
    REQUIRE(sv_find(full, SV("Missing")) == INVALID_INDEX);
    REQUIRE(sv_rfind(full, SV("Missing")) == INVALID_INDEX);

    SECTION("empty search returns 0 / length") {
        REQUIRE(sv_find(full, SV("")) == 0);
        REQUIRE(sv_rfind(full, SV("")) == full.length);
    }

    SECTION("search longer than view returns INVALID_INDEX") {
        REQUIRE(sv_find(SV("ab"), SV("abcde")) == INVALID_INDEX);
    }
}

TEST_CASE("cStringView – sv_find_first_of / sv_find_last_of", "[cStringView]") {
    StringView_t full = SV("  RigidEngine  ");

    REQUIRE(sv_find_first_of(full, SV("aeiou")) == 3);   // 'i' in Rigid
    REQUIRE(sv_find_last_of(full, SV("aeiou")) == 12);   // 'e' in Engine (index 12)
    REQUIRE(sv_find_first_of(full, SV("xyz")) == INVALID_INDEX);
    REQUIRE(sv_find_last_of(full, SV("xyz")) == INVALID_INDEX);
}

// ─────────────────────────────────────────────
//  Trimming
// ─────────────────────────────────────────────
TEST_CASE("cStringView – sv_trim", "[cStringView]") {
    StringView_t full = SV("  RigidEngine  ");

    SECTION("sv_trim removes both sides") {
        StringView_t t = sv_trim(full);
        REQUIRE(t.length == 11);
        REQUIRE(sv_compare(t, SV("RigidEngine")) == 0);
    }

    SECTION("sv_ltrim removes leading whitespace only") {
        StringView_t t = sv_ltrim(full);
        REQUIRE(t.data[0] == 'R');
        REQUIRE(sv_ends_with(t, SV("  ")));
    }

    SECTION("sv_rtrim removes trailing whitespace only") {
        StringView_t t = sv_rtrim(full);
        REQUIRE(t.length == 13); // "  RigidEngine"
        REQUIRE(sv_starts_with(t, SV("  ")));
    }

    SECTION("all-whitespace view returns INVALID_SV") {
        REQUIRE(sv_is_empty(sv_trim(SV("   "))));
    }
}

// ─────────────────────────────────────────────
//  Slicing
// ─────────────────────────────────────────────
TEST_CASE("cStringView – sv_slice", "[cStringView]") {
    StringView_t s = SV("RigidEngine"); // length 11

    SECTION("valid slice") {
        StringView_t engine = sv_slice(s, 5, 11);
        REQUIRE(engine.length == 6);
        REQUIRE(sv_compare(engine, SV("Engine")) == 0);
    }

    SECTION("clamps end to view length") {
        StringView_t t = sv_slice(s, 0, 999);
        REQUIRE(t.length == s.length);
    }

    SECTION("end < start returns INVALID_SV") {
        StringView_t t = sv_slice(s, 5, 2);
        REQUIRE(sv_is_empty(t));
        REQUIRE(t.data == INVALID_SV.data);
    }

    SECTION("start >= length returns INVALID_SV") {
        StringView_t t = sv_slice(s, 11, 15);
        REQUIRE(sv_is_empty(t));
    }
}
