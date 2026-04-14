#include <catch2/catch_test_macros.hpp>
#include <cLib/string/cName.h>
#include <cLib/string/cNameTable.h>
#include <string.h>

// ─────────────────────────────────────────────
//  Fixture — each TEST_CASE gets an isolated table.
//  Slot count MUST be a power of two.
// ─────────────────────────────────────────────
struct TableFixture {
    static constexpr uint32_t kSlots = 64;
    static constexpr size_t   kArena = 4096;

    Name_t      slots[kSlots];
    char        arena[kArena];
    NameTable_t table;

    TableFixture() {
        NameTableStorage_t s;
        s.slotBuffer   = { (uint8_t*)slots, sizeof(slots) };
        s.stringBuffer = { (uint8_t*)arena,  sizeof(arena) };
        name_table_init(&table, s);
    }
};

// ─────────────────────────────────────────────
//  name_table_init
// ─────────────────────────────────────────────
TEST_CASE("cNameTable – init zeroes bookkeeping fields", "[cNameTable]") {
    TableFixture f;
    REQUIRE(f.table.activeSlots    == 0);
    REQUIRE(f.table.bufferOffset   == 0);
    REQUIRE(f.table.bIsInitialized == true);
    REQUIRE(f.table.nameSlotBuffer.count == TableFixture::kSlots);
    REQUIRE(f.table.stringBuffer.size    == TableFixture::kArena);
}

TEST_CASE("cNameTable – init zeroes all slot hashes", "[cNameTable]") {
    TableFixture f;
    for (uint32_t i = 0; i < TableFixture::kSlots; ++i) {
        REQUIRE(f.slots[i].hash == INVALID_HASH);
    }
}

// ─────────────────────────────────────────────
//  name_create (StringView_t overload)
// ─────────────────────────────────────────────
TEST_CASE("cNameTable – name_create interns a new string", "[cNameTable]") {
    TableFixture f;
    Name_t n = name_create(&f.table, SV("Player"));
    REQUIRE(n.hash != INVALID_HASH);
    REQUIRE(strncmp(n.name, "Player", 6) == 0);
    REQUIRE(f.table.activeSlots  == 1);
    REQUIRE(f.table.bufferOffset == 7); // "Player\0"
}

TEST_CASE("cNameTable – name_create deduplicates same string", "[cNameTable]") {
    TableFixture f;
    Name_t n1 = name_create(&f.table, SV("Entity"));
    Name_t n2 = name_create(&f.table, SV("Entity"));
    REQUIRE(n1.name == n2.name);   // pointer identity
    REQUIRE(n1.hash == n2.hash);
    REQUIRE(f.table.activeSlots == 1); // only one slot consumed
}

TEST_CASE("cNameTable – name_create stores multiple distinct strings", "[cNameTable]") {
    TableFixture f;
    Name_t a = name_create(&f.table, SV("Alpha"));
    Name_t b = name_create(&f.table, SV("Beta"));
    Name_t c = name_create(&f.table, SV("Gamma"));
    REQUIRE(f.table.activeSlots == 3);
    REQUIRE(a.hash != b.hash);
    REQUIRE(b.hash != c.hash);
}

TEST_CASE("cNameTable – name_create empty view returns INVALID_NAME", "[cNameTable]") {
    TableFixture f;
    Name_t n = name_create(&f.table, SV(""));
    REQUIRE(n.hash == INVALID_HASH);
    REQUIRE(f.table.activeSlots == 0);
}

TEST_CASE("cNameTable – name_create bufferOffset tracks string bytes", "[cNameTable]") {
    TableFixture f;
    name_create(&f.table, SV("AB"));   // 2 + 1 = 3
    name_create(&f.table, SV("CDE"));  // 3 + 1 = 4  → total 7
    REQUIRE(f.table.bufferOffset == 7);
}

// ─────────────────────────────────────────────
//  name_create_str (const char* overload)
// ─────────────────────────────────────────────
TEST_CASE("cNameTable – name_create_str interns a new string", "[cNameTable]") {
    TableFixture f;
    Name_t n = name_create_str(&f.table, "Transform");
    REQUIRE(n.hash != INVALID_HASH);
    REQUIRE(strcmp(n.name, "Transform") == 0);
    REQUIRE(f.table.activeSlots  == 1);
    REQUIRE(f.table.bufferOffset == 10); // "Transform\0"
}

TEST_CASE("cNameTable – name_create_str deduplicates same string", "[cNameTable]") {
    TableFixture f;
    Name_t n1 = name_create_str(&f.table, "Velocity");
    Name_t n2 = name_create_str(&f.table, "Velocity");
    REQUIRE(n1.name == n2.name);
    REQUIRE(n1.hash == n2.hash);
    REQUIRE(f.table.activeSlots == 1);
}

TEST_CASE("cNameTable – name_create_str null returns INVALID_NAME", "[cNameTable]") {
    TableFixture f;
    Name_t n = name_create_str(&f.table, nullptr);
    REQUIRE(n.hash == INVALID_HASH);
    REQUIRE(f.table.activeSlots == 0);
}

TEST_CASE("cNameTable – name_create_str empty string returns INVALID_NAME", "[cNameTable]") {
    TableFixture f;
    Name_t n = name_create_str(&f.table, "");
    REQUIRE(n.hash == INVALID_HASH);
    REQUIRE(f.table.activeSlots == 0);
}

TEST_CASE("cNameTable – name_create_str and name_create agree on same string", "[cNameTable]") {
    // The two overloads must produce identical hashes for the same content.
    TableFixture f1, f2;
    Name_t bySV  = name_create    (&f1.table, SV("Health"));
    Name_t byCStr = name_create_str(&f2.table, "Health");
    REQUIRE(bySV.hash == byCStr.hash);
    REQUIRE(strcmp(bySV.name, byCStr.name) == 0);
}

// ─────────────────────────────────────────────
//  Cross-table equality
// ─────────────────────────────────────────────
TEST_CASE("cNameTable – same string in two tables: equal hash, different pointer", "[cNameTable]") {
    TableFixture t1, t2;
    Name_t n1 = name_create(&t1.table, SV("Entity"));
    Name_t n2 = name_create(&t2.table, SV("Entity"));
    REQUIRE(n1.hash == n2.hash);
    REQUIRE(n1.name != n2.name); // different arenas
    REQUIRE(name_equals(n1, n2));
}

// ─────────────────────────────────────────────
//  Global table (GLOBAL_TABLE sentinel)
// ─────────────────────────────────────────────
TEST_CASE("cNameTable – global table interns and deduplicates", "[cNameTable][global]") {
    static Name_t gSlots[1024];
    static char   gArena[1024 * 16];
    NameTableStorage_t gs;
    gs.slotBuffer   = { (uint8_t*)gSlots, sizeof(gSlots) };
    gs.stringBuffer = { (uint8_t*)gArena,  sizeof(gArena) };
    name_table_init_global(gs);

    Name_t g1 = name_create(GLOBAL_TABLE, SV("GlobalUnique_NT"));
    Name_t g2 = name_create(GLOBAL_TABLE, SV("GlobalUnique_NT"));
    REQUIRE(g1.name == g2.name);
    REQUIRE(g1.hash == g2.hash);
}

TEST_CASE("cNameTable – global table via name_create_str", "[cNameTable][global]") {
    // Re-uses the global table initialised in the previous test (same process).
    Name_t g1 = name_create_str(GLOBAL_TABLE, "GlobalUnique_NTS");
    Name_t g2 = name_create_str(GLOBAL_TABLE, "GlobalUnique_NTS");
    REQUIRE(g1.name == g2.name);
    REQUIRE(g1.hash == g2.hash);
}

// ─────────────────────────────────────────────
//  name_table_log_stats — smoke: must not crash
// ─────────────────────────────────────────────
TEST_CASE("cNameTable – log_stats does not crash on valid table", "[cNameTable]") {
    TableFixture f;
    name_create(&f.table, SV("Smoke"));
    REQUIRE_NOTHROW(name_table_log_stats(&f.table));
}

TEST_CASE("cNameTable – log_stats does not crash on empty table", "[cNameTable]") {
    TableFixture f;
    REQUIRE_NOTHROW(name_table_log_stats(&f.table));
}

TEST_CASE("cNameTable – log_stats does not crash on global table", "[cNameTable][global]") {
    REQUIRE_NOTHROW(name_table_log_stats(GLOBAL_TABLE));
}
