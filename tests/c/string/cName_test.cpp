#include <catch2/catch_test_macros.hpp>
#include <cLib/string/cName.h>
#include <string.h>

// Tests for cName.h only: NAME macro, fnv_hash, name_from_cstr, name_equals.
// NameTable tests live in cNameTable_test.cpp.

// ─────────────────────────────────────────────
//  NAME macro
// ─────────────────────────────────────────────
TEST_CASE("cName – NAME macro produces valid Name_t at compile time", "[cName]") {
    Name_t n = NAME("Hello");
    REQUIRE(n.name != nullptr);
    REQUIRE(n.hash != INVALID_HASH);
    REQUIRE(strcmp(n.name, "Hello") == 0);
}

TEST_CASE("cName – NAME macro hash matches runtime fnv_hash", "[cName]") {
    Name_t n = NAME("Player");
    uint32_t runtime = fnv_hash("Player", 6);
    REQUIRE(n.hash == runtime);
}

// ─────────────────────────────────────────────
//  fnv_hash
// ─────────────────────────────────────────────
TEST_CASE("cName – fnv_hash is deterministic", "[cName]") {
    REQUIRE(fnv_hash("Player", 6) == fnv_hash("Player", 6));
}

TEST_CASE("cName – fnv_hash distinguishes different strings", "[cName]") {
    REQUIRE(fnv_hash("Player", 6) != fnv_hash("Enemy", 5));
}

TEST_CASE("cName – fnv_hash null or zero-length returns INVALID_HASH", "[cName]") {
    REQUIRE(fnv_hash(nullptr, 0) == INVALID_HASH);
    REQUIRE(fnv_hash("x",    0) == INVALID_HASH);
}

// ─────────────────────────────────────────────
//  fnv_hash_string_view
// ─────────────────────────────────────────────
TEST_CASE("cName – fnv_hash_string_view matches fnv_hash for same content", "[cName]") {
    StringView_t sv = SV("Transform");
    uint32_t h1 = fnv_hash_string_view(sv);
    uint32_t h2 = fnv_hash("Transform", 9);
    REQUIRE(h1 == h2);
}

TEST_CASE("cName – fnv_hash_string_view empty view returns INVALID_HASH", "[cName]") {
    REQUIRE(fnv_hash_string_view(SV(""))              == INVALID_HASH);
    REQUIRE(fnv_hash_string_view(StringView_t{nullptr, 0}) == INVALID_HASH);
}

// ─────────────────────────────────────────────
//  name_from_cstr
// ─────────────────────────────────────────────
TEST_CASE("cName – name_from_cstr valid literal", "[cName]") {
    Name_t n = name_from_cstr("Transform");
    REQUIRE(n.hash != INVALID_HASH);
    REQUIRE(strcmp(n.name, "Transform") == 0);
    REQUIRE(n.hash == fnv_hash("Transform", 9));
}

TEST_CASE("cName – name_from_cstr null returns INVALID_NAME", "[cName]") {
    Name_t n = name_from_cstr(nullptr);
    REQUIRE(n.hash == INVALID_HASH);
    REQUIRE(strcmp(n.name, INVALID_CSTR) == 0);
}

TEST_CASE("cName – name_from_cstr empty string returns INVALID_NAME", "[cName]") {
    Name_t n = name_from_cstr("");
    REQUIRE(n.hash == INVALID_HASH);
}

// ─────────────────────────────────────────────
//  name_equals
// ─────────────────────────────────────────────
TEST_CASE("cName – name_equals same content", "[cName]") {
    Name_t a = NAME("Entity");
    Name_t b = NAME("Entity");
    REQUIRE(name_equals(a, b));
}

TEST_CASE("cName – name_equals different content", "[cName]") {
    Name_t a = NAME("Entity");
    Name_t b = NAME("Component");
    REQUIRE_FALSE(name_equals(a, b));
}

TEST_CASE("cName – name_equals against INVALID_NAME", "[cName]") {
    Name_t a = NAME("Entity");
    REQUIRE_FALSE(name_equals(a, INVALID_NAME));
}

TEST_CASE("cName – name_equals two INVALID_NAMEs", "[cName]") {
    // Both hash == INVALID_HASH == 0, same pointer → equals
    REQUIRE(name_equals(INVALID_NAME, INVALID_NAME));
}
