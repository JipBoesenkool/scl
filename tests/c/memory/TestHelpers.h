#pragma once
#include <iostream>
#include <cstdlib>
#include <cstring>

/* -------------------------------------------------------------------------
 * Assertion helpers shared across all unit tests.
 * ------------------------------------------------------------------------- */
#define ASSERT(condition) \
  do { if (!(condition)) { \
    std::cerr << "\033[31m[FAIL] " << #condition \
              << " [" << __FILE__ << ":" << __LINE__ << "]\033[0m\n"; \
    exit(0); \
  } } while (0)

#define ASSERT_EQ(a, b) \
  do { if ((a) != (b)) { \
    std::cerr << "\033[31m[FAIL] " << #a << " == " << #b \
              << " [" << __FILE__ << ":" << __LINE__ << "]\033[0m\n"; \
    exit(0); \
  } } while (0)

#define ASSERT_NULL(p) \
  do { if ((p) != nullptr) { \
    std::cerr << "\033[31m[FAIL] expected nullptr: " << #p \
              << " [" << __FILE__ << ":" << __LINE__ << "]\033[0m\n"; \
    exit(0); \
  } } while (0)

#define ASSERT_NOT_NULL(p) \
  do { if ((p) == nullptr) { \
    std::cerr << "\033[31m[FAIL] unexpected nullptr: " << #p \
              << " [" << __FILE__ << ":" << __LINE__ << "]\033[0m\n"; \
    exit(0); \
  } } while (0)
