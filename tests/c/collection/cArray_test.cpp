#include <iostream>
#include <cstdlib>
#include <cassert>

#include <cLib/collection/cArray.h>
#include <cLib/collection/cSpan.h>
#include <cLib/collection/cRange.h>

#define ASSERT_EQ(val1, val2) \
  do { if ((val1) != (val2)) { \
    std::cerr << "Assertion Failed: " << #val1 << " == " << #val2 \
              << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
    std::abort(); \
  } } while (0)

#define ASSERT_TRUE(cond) \
  do { if (!(cond)) { \
    std::cerr << "Assertion Failed: " << #cond \
              << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
    std::abort(); \
  } } while (0)

#define ASSERT_GE(val1, val2) \
  do { if ((val1) < (val2)) { \
    std::cerr << "Assertion Failed: " << #val1 << " >= " << #val2 \
              << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
    std::abort(); \
  } } while (0)

static int compare_ints(const void* pA, const void* pB) {
  int a = *(const int*)pA;
  int b = *(const int*)pB;
  return (a < b) ? -1 : (a > b);
}

// --- Lifecycle ---

void TestCreate() {
  std::cout << "[Test] da_create (default capacity)..." << std::endl;
  Array_t arr;
  ASSERT_TRUE(da_create(&arr, sizeof(int)));
  ASSERT_TRUE(arr.pData != nullptr);
  ASSERT_EQ(arr.size, 0);
  ASSERT_EQ(arr.capacity, 32); // default initial capacity
  da_destroy(&arr);
  std::cout << "  ✓" << std::endl;
}

void TestInit() {
  std::cout << "[Test] da_init (explicit capacity)..." << std::endl;
  Array_t arr;
  ASSERT_TRUE(da_init(&arr, sizeof(int), 8));
  ASSERT_TRUE(arr.pData != nullptr);
  ASSERT_EQ(arr.size, 0);
  ASSERT_EQ(arr.capacity, 8);
  da_destroy(&arr);
  std::cout << "  ✓" << std::endl;
}

void TestDestroy() {
  std::cout << "[Test] da_destroy..." << std::endl;
  Array_t arr;
  da_init(&arr, sizeof(int), 4);
  da_destroy(&arr);
  ASSERT_EQ(arr.pData, nullptr);
  ASSERT_EQ(arr.size, 0);
  ASSERT_EQ(arr.capacity, 0);
  std::cout << "  ✓" << std::endl;
}

// --- Iterators ---

void TestIterators() {
  std::cout << "[Test] da_begin, da_end..." << std::endl;
  Array_t arr;
  da_init(&arr, sizeof(int), 0);

  // Empty: begin == end == pData
  ASSERT_EQ(da_begin(&arr), arr.pData);
  ASSERT_EQ(da_end(&arr, sizeof(int)), arr.pData);

  int v = 7;
  da_assign(&arr, sizeof(int), 3, &v); // {7, 7, 7}

  int* pBegin = (int*)da_begin(&arr);
  int* pEnd   = (int*)da_end(&arr, sizeof(int));
  ASSERT_EQ((size_t)(pEnd - pBegin), 3);
  ASSERT_EQ(*pBegin, 7);

  da_destroy(&arr);
  std::cout << "  ✓" << std::endl;
}

// --- Element Access ---

void TestAt() {
  std::cout << "[Test] da_at..." << std::endl;
  Array_t arr;
  da_init(&arr, sizeof(int), 0);

  int vals[] = {10, 20, 30};
  da_assign_span(&arr, sizeof(int), Span_t{ .pData = vals, .size = 3 });

  ASSERT_EQ(*(int*)da_at(&arr, sizeof(int), 0), 10);
  ASSERT_EQ(*(int*)da_at(&arr, sizeof(int), 1), 20);
  ASSERT_EQ(*(int*)da_at(&arr, sizeof(int), 2), 30);

  da_destroy(&arr);
  std::cout << "  ✓" << std::endl;
}

// --- Capacity ---

void TestIsEmpty() {
  std::cout << "[Test] da_isEmpty..." << std::endl;
  Array_t arr;
  da_init(&arr, sizeof(int), 4);
  ASSERT_TRUE(da_isEmpty(&arr));

  int v = 1;
  da_append(&arr, sizeof(int), &v);
  ASSERT_TRUE(!da_isEmpty(&arr));

  da_destroy(&arr);
  std::cout << "  ✓" << std::endl;
}

void TestReserve() {
  std::cout << "[Test] da_reserve..." << std::endl;
  Array_t arr;
  da_init(&arr, sizeof(int), 4);

  ASSERT_TRUE(da_reserve(&arr, sizeof(int), 50));
  ASSERT_GE(arr.capacity, 50);
  ASSERT_EQ(arr.size, 0); // reserve does not change size

  da_destroy(&arr);
  std::cout << "  ✓" << std::endl;
}

void TestShrinkToFit() {
  std::cout << "[Test] da_shrink_to_fit..." << std::endl;
  Array_t arr;
  da_init(&arr, sizeof(int), 0);

  int v = 0;
  da_assign(&arr, sizeof(int), 5, &v);
  da_reserve(&arr, sizeof(int), 100);
  ASSERT_GE(arr.capacity, 100);

  ASSERT_TRUE(da_shrink_to_fit(&arr, sizeof(int)));
  ASSERT_EQ(arr.capacity, 5);

  da_destroy(&arr);
  std::cout << "  ✓" << std::endl;
}

void TestResize() {
  std::cout << "[Test] da_resize..." << std::endl;
  Array_t arr;
  da_init(&arr, sizeof(int), 0);

  ASSERT_TRUE(da_resize(&arr, sizeof(int), 10));
  ASSERT_EQ(arr.size, 10);
  ASSERT_GE(arr.capacity, 10);

  // Shrink via resize
  ASSERT_TRUE(da_resize(&arr, sizeof(int), 3));
  ASSERT_EQ(arr.size, 3);

  da_destroy(&arr);
  std::cout << "  ✓" << std::endl;
}

// --- Modifiers ---

void TestAppend() {
  std::cout << "[Test] da_append..." << std::endl;
  Array_t arr;
  da_init(&arr, sizeof(int), 0);

  int v1 = 10, v2 = 20;
  int* p1 = (int*)da_append(&arr, sizeof(int), &v1);
  int* p2 = (int*)da_append(&arr, sizeof(int), &v2);

  ASSERT_TRUE(p1 != nullptr);
  ASSERT_TRUE(p2 != nullptr);
  ASSERT_EQ(arr.size, 2);
  ASSERT_EQ(*(int*)da_at(&arr, sizeof(int), 0), 10);
  ASSERT_EQ(*(int*)da_at(&arr, sizeof(int), 1), 20);

  da_destroy(&arr);
  std::cout << "  ✓" << std::endl;
}

void TestAppendSpanRange() {
  std::cout << "[Test] da_append_span, da_append_range..." << std::endl;
  Array_t arr;
  da_init(&arr, sizeof(int), 0);

  int data[] = {1, 2, 3, 4, 5};
  Span_t  s = { .pData = data,     .size = 2 };          // {1, 2}
  Range_t r = { .pBegin = &data[2], .pSentinel = &data[5] }; // {3, 4, 5}

  ASSERT_TRUE(da_append_span(&arr, sizeof(int), s));
  ASSERT_TRUE(da_append_range(&arr, sizeof(int), r));
  ASSERT_EQ(arr.size, 5);
  ASSERT_EQ(*(int*)da_at(&arr, sizeof(int), 0), 1);
  ASSERT_EQ(*(int*)da_at(&arr, sizeof(int), 4), 5);

  da_destroy(&arr);
  std::cout << "  ✓" << std::endl;
}

void TestInsert() {
  std::cout << "[Test] da_insert..." << std::endl;
  Array_t arr;
  da_init(&arr, sizeof(int), 0);

  int v1 = 10, v2 = 30, v3 = 20;
  da_append(&arr, sizeof(int), &v1);
  da_append(&arr, sizeof(int), &v2);
  da_insert(&arr, sizeof(int), 1, &v3); // {10, 20, 30}

  ASSERT_EQ(arr.size, 3);
  ASSERT_EQ(*(int*)da_at(&arr, sizeof(int), 0), 10);
  ASSERT_EQ(*(int*)da_at(&arr, sizeof(int), 1), 20);
  ASSERT_EQ(*(int*)da_at(&arr, sizeof(int), 2), 30);

  da_destroy(&arr);
  std::cout << "  ✓" << std::endl;
}

void TestInsertSpanRange() {
  std::cout << "[Test] da_insert_span, da_insert_range..." << std::endl;
  Array_t arr;
  da_init(&arr, sizeof(int), 0);

  int data[] = {1, 2, 3, 4, 5};
  Span_t  s = { .pData = data,     .size = 2 };          // {1, 2}
  Range_t r = { .pBegin = &data[2], .pSentinel = &data[5] }; // {3, 4, 5}

  da_insert_span(&arr, sizeof(int), 0, s);          // {1, 2}
  da_insert_range(&arr, sizeof(int), 1, r);         // {1, 3, 4, 5, 2}

  ASSERT_EQ(arr.size, 5);
  ASSERT_EQ(*(int*)da_at(&arr, sizeof(int), 0), 1);
  ASSERT_EQ(*(int*)da_at(&arr, sizeof(int), 1), 3);
  ASSERT_EQ(*(int*)da_at(&arr, sizeof(int), 4), 2);

  da_destroy(&arr);
  std::cout << "  ✓" << std::endl;
}

void TestPopBack() {
  std::cout << "[Test] da_pop_back, da_pop_back_n..." << std::endl;
  Array_t arr;
  da_init(&arr, sizeof(int), 0);

  int v = 1;
  da_assign(&arr, sizeof(int), 5, &v); // {1,1,1,1,1}

  da_pop_back(&arr);
  ASSERT_EQ(arr.size, 4);

  da_pop_back_n(&arr, 2);
  ASSERT_EQ(arr.size, 2);

  da_destroy(&arr);
  std::cout << "  ✓" << std::endl;
}

void TestErase() {
  std::cout << "[Test] da_erase..." << std::endl;
  Array_t arr;
  da_init(&arr, sizeof(int), 0);

  int data[] = {10, 20, 30};
  da_assign_span(&arr, sizeof(int), Span_t{ .pData = data, .size = 3 });

  da_erase(&arr, sizeof(int), 1); // Remove 20 → {10, 30}
  ASSERT_EQ(arr.size, 2);
  ASSERT_EQ(*(int*)da_at(&arr, sizeof(int), 0), 10);
  ASSERT_EQ(*(int*)da_at(&arr, sizeof(int), 1), 30);

  da_destroy(&arr);
  std::cout << "  ✓" << std::endl;
}

void TestClear() {
  std::cout << "[Test] da_clear..." << std::endl;
  Array_t arr;
  da_init(&arr, sizeof(int), 0);

  int v = 5;
  da_assign(&arr, sizeof(int), 3, &v);
  ASSERT_EQ(arr.size, 3);

  da_clear(&arr);
  ASSERT_EQ(arr.size, 0);
  ASSERT_TRUE(arr.capacity > 0); // memory retained

  da_destroy(&arr);
  std::cout << "  ✓" << std::endl;
}

void TestFill() {
  std::cout << "[Test] da_fill..." << std::endl;
  Array_t arr;
  da_init(&arr, sizeof(int), 0);

  int v = 0;
  da_assign(&arr, sizeof(int), 3, &v); // {0, 0, 0}

  int fill = 99;
  da_fill(&arr, sizeof(int), &fill); // {99, 99, 99}

  ASSERT_EQ(*(int*)da_at(&arr, sizeof(int), 0), 99);
  ASSERT_EQ(*(int*)da_at(&arr, sizeof(int), 1), 99);
  ASSERT_EQ(*(int*)da_at(&arr, sizeof(int), 2), 99);

  da_destroy(&arr);
  std::cout << "  ✓" << std::endl;
}

void TestAssign() {
  std::cout << "[Test] da_assign, da_assign_span, da_assign_range..." << std::endl;
  Array_t arr;
  da_init(&arr, sizeof(int), 0);

  int v = 7;
  da_assign(&arr, sizeof(int), 4, &v); // {7,7,7,7}
  ASSERT_EQ(arr.size, 4);
  ASSERT_EQ(*(int*)da_at(&arr, sizeof(int), 0), 7);

  int data[] = {1, 2, 3};
  da_assign_span(&arr, sizeof(int), Span_t{ .pData = data, .size = 3 }); // {1,2,3}
  ASSERT_EQ(arr.size, 3);
  ASSERT_EQ(*(int*)da_at(&arr, sizeof(int), 1), 2);

  Range_t r = { .pBegin = data, .pSentinel = &data[2] }; // {1, 2}
  da_assign_range(&arr, sizeof(int), r);
  ASSERT_EQ(arr.size, 2);
  ASSERT_EQ(*(int*)da_at(&arr, sizeof(int), 0), 1);
  ASSERT_EQ(*(int*)da_at(&arr, sizeof(int), 1), 2);

  da_destroy(&arr);
  std::cout << "  ✓" << std::endl;
}

void TestSwap() {
  std::cout << "[Test] da_swap..." << std::endl;
  Array_t a, b;
  da_init(&a, sizeof(int), 0);
  da_init(&b, sizeof(int), 0);

  int va = 1, vb = 2;
  da_assign(&a, sizeof(int), 2, &va); // {1, 1}
  da_assign(&b, sizeof(int), 3, &vb); // {2, 2, 2}

  void* pDataA = a.pData;
  void* pDataB = b.pData;

  da_swap(&a, &b);

  ASSERT_EQ(a.pData, pDataB);
  ASSERT_EQ(b.pData, pDataA);
  ASSERT_EQ(a.size, 3);
  ASSERT_EQ(b.size, 2);

  da_destroy(&a);
  da_destroy(&b);
  std::cout << "  ✓" << std::endl;
}

// --- Extended (Strategy) ---

void TestAppendEx() {
  std::cout << "[Test] da_append_ex (strategy)..." << std::endl;
  Array_t arr;
  da_init(&arr, sizeof(int), 0);

  Strategy_t s = { .initialCapacity = 16, .growthFactor = 3.0f };
  int v = 5;
  int* p = (int*)da_append_ex(&arr, sizeof(int), &v, &s);

  ASSERT_TRUE(p != nullptr);
  ASSERT_GE(arr.capacity, 16);
  ASSERT_EQ(arr.size, 1);

  da_destroy(&arr);
  std::cout << "  ✓" << std::endl;
}

void TestAppendSpanRangeEx() {
  std::cout << "[Test] da_append_span_ex, da_append_range_ex (strategy)..." << std::endl;
  Array_t arr;
  da_init(&arr, sizeof(int), 0);

  Strategy_t s = { .initialCapacity = 20, .growthFactor = 2.0f };
  int data[] = {1, 2, 3, 4, 5};
  Span_t  sp = { .pData = data,     .size = 2 };
  Range_t r  = { .pBegin = &data[2], .pSentinel = &data[5] };

  ASSERT_TRUE(da_append_span_ex(&arr, sizeof(int), sp, &s));
  ASSERT_TRUE(da_append_range_ex(&arr, sizeof(int), r, &s));
  ASSERT_EQ(arr.size, 5);

  da_destroy(&arr);
  std::cout << "  ✓" << std::endl;
}

void TestInsertEx() {
  std::cout << "[Test] da_insert_ex (strategy)..." << std::endl;
  Array_t arr;
  da_init(&arr, sizeof(int), 0);

  Strategy_t s = { .initialCapacity = 10, .growthFactor = 2.0f };
  int v1 = 10, v2 = 20;
  da_append_ex(&arr, sizeof(int), &v1, &s);
  da_insert_ex(&arr, sizeof(int), 0, &v2, &s); // {20, 10}

  ASSERT_EQ(arr.size, 2);
  ASSERT_EQ(*(int*)da_at(&arr, sizeof(int), 0), 20);
  ASSERT_EQ(*(int*)da_at(&arr, sizeof(int), 1), 10);

  da_destroy(&arr);
  std::cout << "  ✓" << std::endl;
}

void TestInsertSpanRangeEx() {
  std::cout << "[Test] da_insert_span_ex, da_insert_range_ex (strategy)..." << std::endl;
  Array_t arr;
  da_init(&arr, sizeof(int), 0);

  Strategy_t s = { .initialCapacity = 10, .growthFactor = 2.0f };
  int data[] = {1, 2, 3, 4, 5};
  Span_t  sp = { .pData = data,     .size = 2 };
  Range_t r  = { .pBegin = &data[2], .pSentinel = &data[5] };

  ASSERT_TRUE(da_insert_span_ex(&arr, sizeof(int), 0, sp, &s));
  ASSERT_TRUE(da_insert_range_ex(&arr, sizeof(int), 0, r, &s));
  ASSERT_EQ(arr.size, 5);

  da_destroy(&arr);
  std::cout << "  ✓" << std::endl;
}

int main() {
  try {
    TestCreate();
    TestInit();
    TestDestroy();
    TestIterators();
    TestAt();
    TestIsEmpty();
    TestReserve();
    TestShrinkToFit();
    TestResize();
    TestAppend();
    TestAppendSpanRange();
    TestInsert();
    TestInsertSpanRange();
    TestPopBack();
    TestErase();
    TestClear();
    TestFill();
    TestAssign();
    TestSwap();
    TestAppendEx();
    TestAppendSpanRangeEx();
    TestInsertEx();
    TestInsertSpanRangeEx();

    std::cout << "\n======================================" << std::endl;
    std::cout << "  ALL C ARRAY API TESTS PASSED (100%)" << std::endl;
    std::cout << "======================================" << std::endl;
  } catch (...) {
    std::cerr << "Test suite crashed with unknown exception!" << std::endl;
    return 1;
  }
  return 0;
}
