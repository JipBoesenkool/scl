/**
 * Log_test.cpp — unit tests for memory/private/Log.c
 *
 * Covers: Log_Init, Log_Add, Log_GetCount, Log_GetLine, circular wraparound.
 */
#include <cstdio>
#include <cstring>
#include "cLib/Log.h"
#include "TestHelpers.h"

/* =========================================================================
 * Test_Log_Init
 * After Log_Init the count is 1 (the built-in "[INIT]" message) and
 * Log_GetLine(0) returns a non-null, non-empty string.
 * ========================================================================= */
void Test_Log_Init()
{
  printf("--- Test_Log_Init ---\n");
  Log_Init();
  ASSERT(Log_GetCount() == 1);
  const char* pLine = Log_GetLine(0);
  ASSERT_NOT_NULL(pLine);
  ASSERT(strlen(pLine) > 0);
  printf("[PASS] Test_Log_Init\n\n");
}

/* =========================================================================
 * Test_Log_Add
 * Each call to Log_Add increments the count and makes the message retrievable.
 * ========================================================================= */
void Test_Log_Add()
{
  printf("--- Test_Log_Add ---\n");
  Log_Init();
  uint32_t before = Log_GetCount();

  Log_Add("Hello %s", "World");
  ASSERT_EQ(Log_GetCount(), before + 1);

  const char* pLine = Log_GetLine(before);
  ASSERT_NOT_NULL(pLine);
  ASSERT(strstr(pLine, "Hello") != nullptr);

  Log_Add("Value: %d", 42);
  ASSERT_EQ(Log_GetCount(), before + 2);
  const char* pLine2 = Log_GetLine(before + 1);
  ASSERT_NOT_NULL(pLine2);
  ASSERT(strstr(pLine2, "42") != nullptr);

  printf("[PASS] Test_Log_Add\n\n");
}

/* =========================================================================
 * Test_Log_GetLine_OutOfRange
 * Log_GetLine returns NULL for indices that are out of range (>= count).
 * ========================================================================= */
void Test_Log_GetLine_OutOfRange()
{
  printf("--- Test_Log_GetLine_OutOfRange ---\n");
  Log_Init();
  uint32_t count = Log_GetCount();
  ASSERT_NULL(Log_GetLine(count));      // one past end
  ASSERT_NULL(Log_GetLine(count + 99)); // well past end
  printf("[PASS] Test_Log_GetLine_OutOfRange\n\n");
}

/* =========================================================================
 * Test_Log_NullFormat
 * Log_Add with a null format string must not crash.
 * ========================================================================= */
void Test_Log_NullFormat()
{
  printf("--- Test_Log_NullFormat ---\n");
  Log_Init();
  uint32_t before = Log_GetCount();
  Log_Add(nullptr);               // must return cleanly, count must not change
  ASSERT_EQ(Log_GetCount(), before);
  printf("[PASS] Test_Log_NullFormat\n\n");
}

/* =========================================================================
 * Test_Log_Wraparound
 * After more than 64 messages, the oldest entries are overwritten.
 * Log_GetLine for an overwritten index returns NULL.
 * Log_GetLine for the newest index returns the last message written.
 * ========================================================================= */
void Test_Log_Wraparound()
{
  printf("--- Test_Log_Wraparound ---\n");
  Log_Init();
  // Log_Init already added message at index 0. Add 64 more → total 65.
  for (int i = 0; i < 64; ++i)
    Log_Add("msg %d", i);

  uint32_t count = Log_GetCount(); // 65
  ASSERT_EQ(count, 65u);

  // Index 0 (the Init message) has been overwritten — should return NULL.
  ASSERT_NULL(Log_GetLine(0));

  // Index 1 (first user message) is still within the window.
  const char* pFirst = Log_GetLine(1);
  ASSERT_NOT_NULL(pFirst);
  ASSERT(strstr(pFirst, "msg 0") != nullptr);

  // Index 64 (the last message written) is the newest.
  const char* pLast = Log_GetLine(64);
  ASSERT_NOT_NULL(pLast);
  ASSERT(strstr(pLast, "msg 63") != nullptr);

  printf("[PASS] Test_Log_Wraparound\n\n");
}

/* =========================================================================
 * main
 * ========================================================================= */
int main()
{
  Test_Log_Init();
  Test_Log_Add();
  Test_Log_GetLine_OutOfRange();
  Test_Log_NullFormat();
  Test_Log_Wraparound();

  printf("--- ALL LOG TESTS PASSED ---\n");
  return 0;
}
