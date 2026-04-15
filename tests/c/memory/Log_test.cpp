#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <cstring>
#include "cLib/Log.h"

/* =========================================================================
 * Test_Log_Init
 * After Log_Init the count is 1 (the built-in "[INIT]" message) and
 * Log_GetLine(0) returns a non-null, non-empty string.
 * ========================================================================= */
TEST_CASE("Log – Init", "[Log]") {
  Log_Init();
  REQUIRE(Log_GetCount() == 1);
  const char* pLine = Log_GetLine(0);
  REQUIRE(pLine != nullptr);
  REQUIRE(strlen(pLine) > 0);
}

/* =========================================================================
 * Test_Log_Add
 * Each call to Log_Add increments the count and makes the message retrievable.
 * ========================================================================= */
TEST_CASE("Log – Add", "[Log]") {
  Log_Init();
  uint32_t before = Log_GetCount();

  Log_Add("Hello %s", "World");
  REQUIRE(Log_GetCount() == before + 1);

  const char* pLine = Log_GetLine(before);
  REQUIRE(pLine != nullptr);
  REQUIRE(strstr(pLine, "Hello") != nullptr);

  Log_Add("Value: %d", 42);
  REQUIRE(Log_GetCount() == before + 2);
  const char* pLine2 = Log_GetLine(before + 1);
  REQUIRE(pLine2 != nullptr);
  REQUIRE(strstr(pLine2, "42") != nullptr);
}

/* =========================================================================
 * Test_Log_GetLine_OutOfRange
 * Log_GetLine returns NULL for indices that are out of range (>= count).
 * ========================================================================= */
TEST_CASE("Log – GetLine Out of Range", "[Log]") {
  Log_Init();
  uint32_t count = Log_GetCount();
  REQUIRE(Log_GetLine(count) == nullptr);      // one past end
  REQUIRE(Log_GetLine(count + 99) == nullptr); // well past end
}

/* =========================================================================
 * Test_Log_NullFormat
 * Log_Add with a null format string must not crash.
 * ========================================================================= */
TEST_CASE("Log – Null Format", "[Log]") {
  Log_Init();
  uint32_t before = Log_GetCount();
  Log_Add(nullptr);               // must return cleanly, count must not change
  REQUIRE(Log_GetCount() == before);
}

/* =========================================================================
 * Test_Log_Wraparound
 * After more than 64 messages, the oldest entries are overwritten.
 * Log_GetLine for an overwritten index returns NULL.
 * Log_GetLine for the newest index returns the last message written.
 * ========================================================================= */
TEST_CASE("Log – Wraparound", "[Log]") {
  Log_Init();
  // Log_Init already added message at index 0. Add 64 more → total 65.
  for (int i = 0; i < 64; ++i)
    Log_Add("msg %d", i);

  uint32_t count = Log_GetCount(); // 65
  REQUIRE(count == 65u);

  // Index 0 (the Init message) has been overwritten — should return NULL.
  REQUIRE(Log_GetLine(0) == nullptr);

  // Index 1 (first user message) is still within the window.
  const char* pFirst = Log_GetLine(1);
  REQUIRE(pFirst != nullptr);
  REQUIRE(strstr(pFirst, "msg 0") != nullptr);

  // Index 64 (the last message written) is the newest.
  const char* pLast = Log_GetLine(64);
  REQUIRE(pLast != nullptr);
  REQUIRE(strstr(pLast, "msg 63") != nullptr);
}
