#include <catch2/catch_test_macros.hpp>
#include <cLib/string/cString.h>
#include <string.h>

// ─────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────
static bool str_eq(const String_t* s, const char* lit) {
    return strcmp(string_to_c_str(s), lit) == 0;
}

// ─────────────────────────────────────────────
//  Lifecycle
// ─────────────────────────────────────────────
TEST_CASE("cString – INVALID_STRING is empty", "[cString]") {
    String_t s = INVALID_STRING;
    REQUIRE(string_is_empty(&s));
    REQUIRE(string_length(&s) == 0);
    string_destroy(&s); // must be a no-op
}

TEST_CASE("cString – string_create", "[cString]") {
    SECTION("non-empty string") {
        String_t s = string_create("Hello");
        REQUIRE_FALSE(string_is_empty(&s));
        REQUIRE(string_length(&s) == 5);
        REQUIRE(str_eq(&s, "Hello"));
        string_destroy(&s);
    }

    SECTION("null pointer returns INVALID_STRING") {
        String_t s = string_create(nullptr);
        REQUIRE(string_is_empty(&s));
    }

    SECTION("empty string literal") {
        String_t s = string_create("");
        REQUIRE(string_is_empty(&s));
        string_destroy(&s);
    }
}

TEST_CASE("cString – string_copy produces independent copy", "[cString]") {
    String_t orig = string_create("Original");
    String_t copy = string_copy(&orig);

    REQUIRE(string_compare(&orig, &copy) == 0);
    REQUIRE(string_to_c_str(&orig) != string_to_c_str(&copy)); // different buffers

    string_destroy(&orig);
    string_destroy(&copy);
}

TEST_CASE("cString – string_destroy resets to empty", "[cString]") {
    String_t s = string_create("Bye");
    string_destroy(&s);
    REQUIRE(string_is_empty(&s));
}

TEST_CASE("cString – create_from_sv", "[cString]") {
    SECTION("normal view") {
        StringView_t sv = SV("View");
        String_t s = string_create_from_sv(sv);
        REQUIRE(string_length(&s) == 4);
        REQUIRE(str_eq(&s, "View"));
        string_destroy(&s);
    }

    SECTION("empty view returns empty string") {
        String_t s = string_create_from_sv(SV(""));
        REQUIRE(string_is_empty(&s));
        string_destroy(&s);
    }

    SECTION("zero-length view (data non-null, length 0)") {
        StringView_t zv = { "ignored", 0 };
        String_t s = string_create_from_sv(zv);
        REQUIRE(string_is_empty(&s));
        string_destroy(&s);
    }
}

// ─────────────────────────────────────────────
//  SSO boundaries
// ─────────────────────────────────────────────
TEST_CASE("cString – SSO boundary (23 chars stays on stack)", "[cString][sso]") {
    const char* lit = "12345678901234567890123"; // 23 chars
    String_t s = string_create(lit);
    REQUIRE(string_length(&s) == 23);
    REQUIRE(s.capacity == 23);
    REQUIRE(str_eq(&s, lit));
    string_destroy(&s);
}

TEST_CASE("cString – heap allocation (24 chars)", "[cString][sso]") {
    const char* lit = "123456789012345678901234"; // 24 chars
    String_t s = string_create(lit);
    REQUIRE(string_length(&s) == 24);
    REQUIRE(s.capacity == 25); // length + 1
    REQUIRE(str_eq(&s, lit));
    string_destroy(&s);
}

// ─────────────────────────────────────────────
//  Inspection
// ─────────────────────────────────────────────
TEST_CASE("cString – string_at", "[cString]") {
    String_t s = string_create("Hello");
    REQUIRE(string_at(&s, 0) == 'H');
    REQUIRE(string_at(&s, 4) == 'o');
    REQUIRE(string_at(&s, 99) == '\0'); // out-of-bounds returns '\0'
    string_destroy(&s);
}

TEST_CASE("cString – string_compare", "[cString]") {
    String_t a = string_create("abc");
    String_t b = string_create("abc");
    String_t c = string_create("abd");
    String_t d = string_create("ab");

    REQUIRE(string_compare(&a, &b) == 0);
    REQUIRE(string_compare(&a, &c) < 0);
    REQUIRE(string_compare(&c, &a) > 0);
    REQUIRE(string_compare(&a, &d) > 0); // longer is greater when prefix matches

    string_destroy(&a); string_destroy(&b);
    string_destroy(&c); string_destroy(&d);
}

TEST_CASE("cString – string_to_sv", "[cString]") {
    String_t s = string_create("Hi");
    StringView_t sv = string_to_sv(&s);
    REQUIRE(sv.length == 2);
    REQUIRE(memcmp(sv.data, "Hi", 2) == 0);
    string_destroy(&s);

    SECTION("empty string returns INVALID_SV") {
        String_t empty = INVALID_STRING;
        StringView_t esv = string_to_sv(&empty);
        REQUIRE(sv_is_empty(esv));
    }
}

// ─────────────────────────────────────────────
//  Transformation – case
// ─────────────────────────────────────────────
TEST_CASE("cString – string_to_upper / string_to_lower", "[cString]") {
    String_t s = string_create("Hello World");
    String_t upper = string_to_upper(&s);
    String_t lower = string_to_lower(&s);

    REQUIRE(str_eq(&upper, "HELLO WORLD"));
    REQUIRE(str_eq(&lower, "hello world"));

    string_destroy(&s); string_destroy(&upper); string_destroy(&lower);
}

TEST_CASE("cString – case on empty string returns empty", "[cString]") {
    String_t s = INVALID_STRING;
    String_t upper = string_to_upper(&s);
    String_t lower = string_to_lower(&s);
    REQUIRE(string_is_empty(&upper));
    REQUIRE(string_is_empty(&lower));
    string_destroy(&upper); string_destroy(&lower);
}

// ─────────────────────────────────────────────
//  Transformation – substring
// ─────────────────────────────────────────────
TEST_CASE("cString – string_substring", "[cString]") {
    String_t s = string_create("Hello");

    SECTION("valid range") {
        String_t sub = string_substring(&s, 0, 4);
        REQUIRE(str_eq(&sub, "Hell"));
        string_destroy(&sub);
    }

    SECTION("end > length clamps to length") {
        String_t sub = string_substring(&s, 1, 999);
        REQUIRE(str_eq(&sub, "ello"));
        string_destroy(&sub);
    }

    SECTION("end < start returns empty") {
        String_t sub = string_substring(&s, 3, 1);
        REQUIRE(string_is_empty(&sub));
        string_destroy(&sub);
    }

    string_destroy(&s);
}

// ─────────────────────────────────────────────
//  Transformation – concat
// ─────────────────────────────────────────────
TEST_CASE("cString – string_concat", "[cString]") {
    String_t result = string_concat(SV("Hello"), SV(" World"));
    REQUIRE(str_eq(&result, "Hello World"));
    string_destroy(&result);

    SECTION("concat with empty") {
        String_t r = string_concat(SV("Hello"), SV(""));
        REQUIRE(str_eq(&r, "Hello"));
        string_destroy(&r);
    }
}

// ─────────────────────────────────────────────
//  Transformation – upper / lower
// ─────────────────────────────────────────────
TEST_CASE("cString – string_to_upper / string_to_lower edge cases", "[cString]") {
    SECTION("empty string returns INVALID_STRING") {
        String_t empty = INVALID_STRING;
        String_t up   = string_to_upper(&empty);
        String_t down = string_to_lower(&empty);
        REQUIRE(string_is_empty(&up));
        REQUIRE(string_is_empty(&down));
        string_destroy(&up);
        string_destroy(&down);
    }

    SECTION("already upper / already lower roundtrip") {
        String_t s = string_create("Hello World");
        String_t up   = string_to_upper(&s);
        String_t down = string_to_lower(&s);
        REQUIRE(str_eq(&up,   "HELLO WORLD"));
        REQUIRE(str_eq(&down, "hello world"));
        string_destroy(&s); string_destroy(&up); string_destroy(&down);
    }
}

// ─────────────────────────────────────────────
//  Transformation – repeat
// ─────────────────────────────────────────────
TEST_CASE("cString – string_repeat", "[cString]") {
    String_t dot = string_create(".");
    String_t line = string_repeat(&dot, 3);
    REQUIRE(str_eq(&line, "..."));
    string_destroy(&dot); string_destroy(&line);

    SECTION("repeat 0 times returns empty") {
        String_t base = string_create("abc");
        String_t r = string_repeat(&base, 0);
        REQUIRE(string_is_empty(&r));
        string_destroy(&base); string_destroy(&r);
    }

    SECTION("repeat empty string returns empty") {
        String_t empty = INVALID_STRING;
        String_t r = string_repeat(&empty, 5);
        REQUIRE(string_is_empty(&r));
        string_destroy(&r);
    }
}

// ─────────────────────────────────────────────
//  Transformation – padding
// ─────────────────────────────────────────────
TEST_CASE("cString – string_lpad / string_rpad", "[cString]") {
    String_t base = string_create("42");

    String_t lpadded = string_lpad(&base, '0', 5);
    String_t rpadded = string_rpad(&base, ' ', 5);
    REQUIRE(str_eq(&lpadded, "00042"));
    REQUIRE(str_eq(&rpadded, "42   "));

    SECTION("pad length <= current length returns copy") {
        String_t noop = string_lpad(&base, '0', 1);
        REQUIRE(str_eq(&noop, "42"));
        string_destroy(&noop);
    }

    string_destroy(&base); string_destroy(&lpadded); string_destroy(&rpadded);
}

// ─────────────────────────────────────────────
//  Transformation – replace
// ─────────────────────────────────────────────
TEST_CASE("cString – string_replace", "[cString]") {
    String_t s = string_create("C:/dev/file.txt");

    SECTION("single occurrence") {
        String_t r = string_replace(&s, SV("file.txt"), SV("main.cpp"));
        REQUIRE(str_eq(&r, "C:/dev/main.cpp"));
        string_destroy(&r);
    }

    SECTION("no match returns copy") {
        String_t r = string_replace(&s, SV("missing"), SV("x"));
        REQUIRE(string_compare(&s, &r) == 0);
        string_destroy(&r);
    }

    SECTION("replace with shorter string") {
        String_t r = string_replace(&s, SV("/dev/"), SV("/"));
        REQUIRE(str_eq(&r, "C:/file.txt"));
        string_destroy(&r);
    }

    string_destroy(&s);
}

TEST_CASE("cString – string_replace_char", "[cString]") {
    SECTION("replaces all occurrences") {
        String_t s = string_create("aabbcc");
        String_t r = string_replace_char(&s, 'b', 'X');
        REQUIRE(str_eq(&r, "aaXXcc"));
        string_destroy(&s); string_destroy(&r);
    }

    SECTION("char not present returns copy") {
        String_t s = string_create("Hello");
        String_t r = string_replace_char(&s, 'z', 'X');
        REQUIRE(string_compare(&s, &r) == 0);
        string_destroy(&s); string_destroy(&r);
    }

    SECTION("empty string returns empty") {
        String_t s = INVALID_STRING;
        String_t r = string_replace_char(&s, 'a', 'b');
        REQUIRE(string_is_empty(&r));
        string_destroy(&r);
    }
}

// ─────────────────────────────────────────────
//  Transformation – erase
// ─────────────────────────────────────────────
TEST_CASE("cString – string_erase (by index)", "[cString]") {
    String_t s = string_create("C:/project/main.cpp");

    SECTION("erase middle range") {
        String_t r = string_erase(&s, 2, 8); // remove "/project"
        REQUIRE(str_eq(&r, "C:/main.cpp"));
        string_destroy(&r);
    }

    SECTION("start >= length returns copy") {
        String_t r = string_erase(&s, 99, 1);
        REQUIRE(string_compare(&s, &r) == 0);
        string_destroy(&r);
    }

    SECTION("count exceeds remaining chars clamps to end") {
        String_t r = string_erase(&s, 2, 9999);
        REQUIRE(str_eq(&r, "C:"));
        string_destroy(&r);
    }

    SECTION("count 0 returns copy") {
        String_t r = string_erase(&s, 0, 0);
        REQUIRE(string_compare(&s, &r) == 0);
        string_destroy(&r);
    }

    string_destroy(&s);
}

TEST_CASE("cString – string_erase_sv (by pattern)", "[cString]") {
    SECTION("removes all occurrences") {
        String_t s = string_create("Mississippi");
        String_t noS = string_erase_sv(&s, SV("s"));
        REQUIRE(str_eq(&noS, "Miiippi"));

        String_t noI = string_erase_sv(&noS, SV("i"));
        REQUIRE(str_eq(&noI, "Mpp"));

        string_destroy(&s); string_destroy(&noS); string_destroy(&noI);
    }

    SECTION("no match returns copy") {
        String_t s = string_create("Hello");
        String_t r = string_erase_sv(&s, SV("z"));
        REQUIRE(string_compare(&s, &r) == 0);
        string_destroy(&s); string_destroy(&r);
    }
}
