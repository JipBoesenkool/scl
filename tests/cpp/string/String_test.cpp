#include <catch2/catch_test_macros.hpp>
#include <cLib/string/String.hpp>
#include <cLib/string/cStringBuilder.h>
#include <cstring>

// ─────────────────────────────────────────────
//  ABI / layout
// ─────────────────────────────────────────────
TEST_CASE("String – ABI: wrapper is same size as String_t", "[String][abi]") {
    static_assert(sizeof(String_t) == 32);
    static_assert(sizeof(String)   == 32);

    String s("Align");
    REQUIRE((void*)&s == (void*)&s.impl);
    s.destroy();
}

// ─────────────────────────────────────────────
//  Lifecycle
// ─────────────────────────────────────────────
TEST_CASE("String – default constructor is empty", "[String]") {
    String s;
    REQUIRE(s.isEmpty());
    REQUIRE(s.length() == 0);
    s.destroy();
}

TEST_CASE("String – construct from c-string", "[String]") {
    String s("Hello");
    REQUIRE_FALSE(s.isEmpty());
    REQUIRE(s.length() == 5);
    REQUIRE(strcmp(s.c_str(), "Hello") == 0);
    s.destroy();
}

TEST_CASE("String – construct from StringView_t", "[String]") {
    String s(SV("View"));
    REQUIRE(s.length() == 4);
    REQUIRE(s == SV("View"));
    s.destroy();
}

TEST_CASE("String – move constructor (String*) transfers ownership", "[String]") {
    String src("Move Me");
    String dst(&src);
    REQUIRE(src.isEmpty());
    REQUIRE(dst == SV("Move Me"));
    dst.destroy();
}

TEST_CASE("String – init() reinitialises from c-string and sv", "[String]") {
    String s;
    s.init("Manual");
    REQUIRE(s == SV("Manual"));
    s.destroy();

    String s2;
    s2.init(SV("ViewInit"));
    REQUIRE(s2 == SV("ViewInit"));
    s2.destroy();

    SECTION("init(nullptr) produces empty string") {
        String sn;
        sn.init(nullptr);
        REQUIRE(sn.isEmpty());
        sn.destroy();
    }

    SECTION("init(empty SV) produces empty string") {
        String se;
        se.init(SV(""));
        REQUIRE(se.isEmpty());
        se.destroy();
    }
}

TEST_CASE("String – copy() produces independent deep copy", "[String]") {
    String orig("Original");
    String copy = orig.copy();

    REQUIRE(orig == copy);
    REQUIRE(orig.c_str() != copy.c_str()); // different buffers

    orig.destroy();
    copy.destroy();
}

TEST_CASE("String – destroy() resets to empty", "[String]") {
    String s("Bye");
    s.destroy();
    REQUIRE(s.isEmpty());
}

// ─────────────────────────────────────────────
//  SSO boundaries
// ─────────────────────────────────────────────
TEST_CASE("String – SSO: 23-char string stays on stack", "[String][sso]") {
    const char* lit = "12345678901234567890123";
    String s(lit);
    REQUIRE(s.length()   == 23);
    REQUIRE(s.capacity() == 23);
    REQUIRE(s == StringView_t{ lit, 23 });
    s.destroy();
}

TEST_CASE("String – heap: 24-char string allocates", "[String][sso]") {
    const char* lit = "123456789012345678901234";
    String s(lit);
    REQUIRE(s.length()   == 24);
    REQUIRE(s.capacity() == 25);
    REQUIRE(s == StringView_t{ lit, 24 });
    s.destroy();
}

// ─────────────────────────────────────────────
//  Operators
// ─────────────────────────────────────────────
TEST_CASE("String – operator== and operator!=", "[String]") {
    String a("abc");
    String b("abc");
    String c("xyz");

    REQUIRE(a == b);
    REQUIRE_FALSE(a == c);      // operator==(const String&) inequality path
    REQUIRE(a == SV("abc"));
    REQUIRE(a != SV("xyz"));
    REQUIRE(a != c);            // operator!=(const String&)
    REQUIRE_FALSE(a != b);      // operator!=(const String&) — same strings

    a.destroy(); b.destroy(); c.destroy();
}

TEST_CASE("String – operator!=(const String&) SSO vs heap", "[String]") {
    // SSO vs SSO — unequal
    String sso_a("abc");
    String sso_b("xyz");
    REQUIRE(sso_a != sso_b);
    REQUIRE_FALSE(sso_a != String("abc")); // same content
    sso_a.destroy(); sso_b.destroy();

    // SSO vs heap — unequal
    String sso("short");
    String heap("this string is long enough to live on the heap!!");
    REQUIRE(sso != heap);
    sso.destroy(); heap.destroy();

    // Heap vs heap — equal content, should NOT be !=
    String heap_a("this string is long enough to live on the heap!!");
    String heap_b("this string is long enough to live on the heap!!");
    REQUIRE_FALSE(heap_a != heap_b);
    heap_a.destroy(); heap_b.destroy();
}

TEST_CASE("String – operator[] returns char at index", "[String]") {
    String s("Hello");
    REQUIRE(s[0] == 'H');
    REQUIRE(s[4] == 'o');
    REQUIRE(s[99] == '\0'); // out-of-bounds returns '\0'
    s.destroy();
}

TEST_CASE("String – concatenation via StringBuilder", "[String]") {
    // operator+ is intentionally removed; concatenation goes through StringBuilder
    char buf[64];
    Buffer_t mem = { .pData = (uint8_t*)buf, .size = sizeof(buf) };
    StringBuilder_t sb;
    sb_init(&sb, mem);

    String a("Hello");
    String b(" World");
    sb_append_sv(&sb, a.sv());
    sb_append_sv(&sb, b.sv());
    String c(sb_to_sv(&sb));
    REQUIRE(c == SV("Hello World"));

    a.destroy(); b.destroy(); c.destroy();
}

TEST_CASE("String – begin/end iterators span content", "[String]") {
    String s("Hi");
    REQUIRE(s.end() - s.begin() == 2);
    REQUIRE(*s.begin() == 'H');
    s.destroy();
}

// ─────────────────────────────────────────────
//  Inspection
// ─────────────────────────────────────────────
TEST_CASE("String – size() equals length()", "[String]") {
    String s("Test");
    REQUIRE(s.size() == s.length());
    s.destroy();
}

TEST_CASE("String – contains / starts_with / ends_with", "[String]") {
    String s("  Mississippi  ");
    REQUIRE(s.contains(SV("ssi")));
    REQUIRE(s.starts_with(SV("  ")));
    REQUIRE(s.ends_with(SV("  ")));
    REQUIRE_FALSE(s.contains(SV("xyz")));

    SECTION("starts_with: prefix longer than string returns false") {
        String short_s("Hi");
        REQUIRE_FALSE(short_s.starts_with(SV("Hello World")));
        short_s.destroy();
    }

    SECTION("ends_with: suffix longer than string returns false") {
        String short_s("Hi");
        REQUIRE_FALSE(short_s.ends_with(SV("Hello World")));
        short_s.destroy();
    }

    s.destroy();
}

TEST_CASE("String – compare delegates to sv_compare", "[String]") {
    String s("abc");
    REQUIRE(s.compare(SV("abc")) == 0);
    REQUIRE(s.compare(SV("abd")) < 0);
    REQUIRE(s.compare(SV("abb")) > 0);
    s.destroy();
}

// ─────────────────────────────────────────────
//  Search
// ─────────────────────────────────────────────
TEST_CASE("String – find / rfind return StringView_t into string", "[String]") {
    String s("  Mississippi  ");

    SECTION("find first occurrence") {
        StringView_t r = s.find(SV("i"));
        REQUIRE(r.data == s.begin() + 3);
        REQUIRE(r.length == 1);
    }

    SECTION("rfind last occurrence") {
        StringView_t r = s.rfind(SV("i"));
        REQUIRE(r.data == s.begin() + 12);
    }

    SECTION("not found returns INVALID_SV") {
        StringView_t r = s.find(SV("xyz"));
        REQUIRE(sv_is_empty(r));
    }

    s.destroy();
}

TEST_CASE("String – find_first_of / find_last_of", "[String]") {
    String s("  Mississippi  ");
    REQUIRE(s.find_first_of(SV("aeiou")) == 3);
    REQUIRE(s.find_last_of(SV("aeiou"))  == 12);
    REQUIRE(s.find_first_of('M')         == 2);
    REQUIRE(s.find_last_of('p')          == 11);
    REQUIRE(s.find_first_of(SV("xyz"))   == INVALID_INDEX);

    SECTION("find_first_of(char) not found returns INVALID_INDEX") {
        REQUIRE(s.find_first_of('z') == INVALID_INDEX);
    }

    SECTION("find_last_of(char) not found returns INVALID_INDEX") {
        REQUIRE(s.find_last_of('z') == INVALID_INDEX);
    }

    s.destroy();
}

// ─────────────────────────────────────────────
//  Trimming (returns StringView_t)
// ─────────────────────────────────────────────
TEST_CASE("String – trim / ltrim / rtrim", "[String]") {
    String s("  Hello  ");

    StringView_t t  = s.trim();
    StringView_t lt = s.ltrim();
    StringView_t rt = s.rtrim();

    REQUIRE(sv_compare(t,  SV("Hello")) == 0);
    REQUIRE(lt.data[0] == 'H');
    REQUIRE(sv_ends_with(lt, SV("  ")));
    REQUIRE(sv_starts_with(rt, SV("  ")));
    REQUIRE(rt.length == 7); // "  Hello"

    s.destroy();
}

// ─────────────────────────────────────────────
//  Slicing
// ─────────────────────────────────────────────
TEST_CASE("String – slice returns StringView_t", "[String]") {
    String s("C:/project/file.txt");

    StringView_t proj = s.slice(3, 10);
    REQUIRE(proj.length == 7);
    REQUIRE(memcmp(proj.data, "project", 7) == 0);

    s.destroy();
}

// ─────────────────────────────────────────────
//  Transformations (return new String)
// ─────────────────────────────────────────────
TEST_CASE("String – toUpper / toLower", "[String]") {
    String s("Hello World");
    String upper = s.toUpper();
    String lower = s.toLower();

    REQUIRE(upper == SV("HELLO WORLD"));
    REQUIRE(lower == SV("hello world"));

    s.destroy(); upper.destroy(); lower.destroy();
}

TEST_CASE("String – repeat", "[String]") {
    String s("!");
    String rep = s.repeat(3);
    REQUIRE(rep == SV("!!!"));
    s.destroy(); rep.destroy();

    SECTION("repeat 0 times") {
        String base("abc");
        String r = base.repeat(0);
        REQUIRE(r.isEmpty());
        base.destroy(); r.destroy();
    }
}

TEST_CASE("String – lpad / rpad", "[String]") {
    String s("42");
    String lp = s.lpad('0', 5);
    String rp = s.rpad(' ', 5);

    REQUIRE(lp == SV("00042"));
    REQUIRE(rp == SV("42   "));

    SECTION("count <= current length returns copy") {
        String noop = s.lpad('0', 1);
        REQUIRE(noop == SV("42"));
        noop.destroy();
    }

    s.destroy(); lp.destroy(); rp.destroy();
}

TEST_CASE("String – substring", "[String]") {
    String s("Hello");

    SECTION("valid range") {
        String sub = s.substring(0, 4);
        REQUIRE(sub == SV("Hell"));
        sub.destroy();
    }

    SECTION("end < start returns empty") {
        String sub = s.substring(3, 1);
        REQUIRE(sub.isEmpty());
        sub.destroy();
    }

    s.destroy();
}

TEST_CASE("String – erase by index", "[String]") {
    String s("C:/project/file.txt");

    SECTION("valid range") {
        String r = s.erase(2, 8); // remove "/project"
        REQUIRE(r == SV("C:/file.txt"));
        r.destroy();
    }

    SECTION("start >= length returns copy") {
        String r = s.erase(99, 1);
        REQUIRE(r == s);
        r.destroy();
    }

    SECTION("count 0 returns copy") {
        String r = s.erase(0, 0);
        REQUIRE(r == s);
        r.destroy();
    }

    SECTION("count exceeds remaining clamps to end") {
        String r = s.erase(2, 9999);
        REQUIRE(r == SV("C:"));
        r.destroy();
    }

    s.destroy();
}

TEST_CASE("String – erase by StringView_t pattern", "[String]") {
    String s("C:/project/file.txt");
    String r = s.erase(SV(".txt"));
    REQUIRE(r == SV("C:/project/file"));
    s.destroy(); r.destroy();
}

TEST_CASE("String – replace (StringView_t overload)", "[String]") {
    String s("C:/project/file.txt");
    String r = s.replace(SV(".txt"), SV(".cpp"));
    REQUIRE(r == SV("C:/project/file.cpp"));
    s.destroy(); r.destroy();
}

TEST_CASE("String – replace (const char* overload)", "[String]") {
    String s("C:/project/file.txt");
    String r = s.replace(".txt", ".cpp");
    REQUIRE(r == SV("C:/project/file.cpp"));
    s.destroy(); r.destroy();
}
