#include <catch2/catch_test_macros.hpp>
#include <cLib/collection/cArray.h>
#include <cLib/collection/cSpan.h>
#include <cLib/collection/cRange.h>

static int compare_ints(const void* pA, const void* pB) {
  int a = *(const int*)pA;
  int b = *(const int*)pB;
  return (a < b) ? -1 : (a > b);
}

// --- Lifecycle ---

TEST_CASE("cArray – da_create (default capacity)", "[cArray]") {
  Array_t arr;
  REQUIRE(da_create(&arr, sizeof(int)));
  REQUIRE(arr.pData != nullptr);
  REQUIRE(arr.size == 0);
  REQUIRE(arr.capacity == 32); // default initial capacity
  da_destroy(&arr);
}

TEST_CASE("cArray – da_init (explicit capacity)", "[cArray]") {
  Array_t arr;
  REQUIRE(da_init(&arr, sizeof(int), 8));
  REQUIRE(arr.pData != nullptr);
  REQUIRE(arr.size == 0);
  REQUIRE(arr.capacity == 8);
  da_destroy(&arr);
}

TEST_CASE("cArray – da_destroy", "[cArray]") {
  Array_t arr;
  da_init(&arr, sizeof(int), 4);
  da_destroy(&arr);
  REQUIRE(arr.pData == nullptr);
  REQUIRE(arr.size == 0);
  REQUIRE(arr.capacity == 0);
}

// --- Iterators ---

TEST_CASE("cArray – da_begin, da_end", "[cArray]") {
  Array_t arr;
  da_init(&arr, sizeof(int), 0);

  // Empty: begin == end == pData
  REQUIRE(da_begin(&arr) == arr.pData);
  REQUIRE(da_end(&arr, sizeof(int)) == arr.pData);

  int v = 7;
  da_assign(&arr, sizeof(int), 3, &v); // {7, 7, 7}

  int* pBegin = (int*)da_begin(&arr);
  int* pEnd   = (int*)da_end(&arr, sizeof(int));
  REQUIRE((size_t)(pEnd - pBegin) == 3);
  REQUIRE(*pBegin == 7);

  da_destroy(&arr);
}

// --- Element Access ---

TEST_CASE("cArray – da_at", "[cArray]") {
  Array_t arr;
  da_init(&arr, sizeof(int), 0);

  int vals[] = {10, 20, 30};
  da_assign_span(&arr, sizeof(int), Span_t{ .pData = vals, .size = 3 });

  REQUIRE(*(int*)da_at(&arr, sizeof(int), 0) == 10);
  REQUIRE(*(int*)da_at(&arr, sizeof(int), 1) == 20);
  REQUIRE(*(int*)da_at(&arr, sizeof(int), 2) == 30);

  da_destroy(&arr);
}

// --- Capacity ---

TEST_CASE("cArray – da_isEmpty", "[cArray]") {
  Array_t arr;
  da_init(&arr, sizeof(int), 4);
  REQUIRE(da_isEmpty(&arr));

  int v = 1;
  da_append(&arr, sizeof(int), &v);
  REQUIRE(!da_isEmpty(&arr));

  da_destroy(&arr);
}

TEST_CASE("cArray – da_reserve", "[cArray]") {
  Array_t arr;
  da_init(&arr, sizeof(int), 4);

  REQUIRE(da_reserve(&arr, sizeof(int), 50));
  REQUIRE(arr.capacity >= 50);
  REQUIRE(arr.size == 0); // reserve does not change size

  da_destroy(&arr);
}

TEST_CASE("cArray – da_shrink_to_fit", "[cArray]") {
  Array_t arr;
  da_init(&arr, sizeof(int), 0);

  int v = 0;
  da_assign(&arr, sizeof(int), 5, &v);
  da_reserve(&arr, sizeof(int), 100);
  REQUIRE(arr.capacity >= 100);

  REQUIRE(da_shrink_to_fit(&arr, sizeof(int)));
  REQUIRE(arr.capacity == 5);

  da_destroy(&arr);
}

TEST_CASE("cArray – da_resize", "[cArray]") {
  Array_t arr;
  da_init(&arr, sizeof(int), 0);

  REQUIRE(da_resize(&arr, sizeof(int), 10));
  REQUIRE(arr.size == 10);
  REQUIRE(arr.capacity >= 10);

  // Shrink via resize
  REQUIRE(da_resize(&arr, sizeof(int), 3));
  REQUIRE(arr.size == 3);

  da_destroy(&arr);
}

// --- Modifiers ---

TEST_CASE("cArray – da_append", "[cArray]") {
  Array_t arr;
  da_init(&arr, sizeof(int), 0);

  int v1 = 10, v2 = 20;
  int* p1 = (int*)da_append(&arr, sizeof(int), &v1);
  int* p2 = (int*)da_append(&arr, sizeof(int), &v2);

  REQUIRE(p1 != nullptr);
  REQUIRE(p2 != nullptr);
  REQUIRE(arr.size == 2);
  REQUIRE(*(int*)da_at(&arr, sizeof(int), 0) == 10);
  REQUIRE(*(int*)da_at(&arr, sizeof(int), 1) == 20);

  da_destroy(&arr);
}

TEST_CASE("cArray – da_append_span, da_append_range", "[cArray]") {
  Array_t arr;
  da_init(&arr, sizeof(int), 0);

  int data[] = {1, 2, 3, 4, 5};
  Span_t  s = { .pData = data,     .size = 2 };          // {1, 2}
  Range_t r = { .pBegin = &data[2], .pSentinel = &data[5] }; // {3, 4, 5}

  REQUIRE(da_append_span(&arr, sizeof(int), s));
  REQUIRE(da_append_range(&arr, sizeof(int), r));
  REQUIRE(arr.size == 5);
  REQUIRE(*(int*)da_at(&arr, sizeof(int), 0) == 1);
  REQUIRE(*(int*)da_at(&arr, sizeof(int), 4) == 5);

  da_destroy(&arr);
}

TEST_CASE("cArray – da_insert", "[cArray]") {
  Array_t arr;
  da_init(&arr, sizeof(int), 0);

  int v1 = 10, v2 = 30, v3 = 20;
  da_append(&arr, sizeof(int), &v1);
  da_append(&arr, sizeof(int), &v2);
  da_insert(&arr, sizeof(int), 1, &v3); // {10, 20, 30}

  REQUIRE(arr.size == 3);
  REQUIRE(*(int*)da_at(&arr, sizeof(int), 0) == 10);
  REQUIRE(*(int*)da_at(&arr, sizeof(int), 1) == 20);
  REQUIRE(*(int*)da_at(&arr, sizeof(int), 2) == 30);

  da_destroy(&arr);
}

TEST_CASE("cArray – da_insert_span, da_insert_range", "[cArray]") {
  Array_t arr;
  da_init(&arr, sizeof(int), 0);

  int data[] = {1, 2, 3, 4, 5};
  Span_t  s = { .pData = data,     .size = 2 };          // {1, 2}
  Range_t r = { .pBegin = &data[2], .pSentinel = &data[5] }; // {3, 4, 5}

  da_insert_span(&arr, sizeof(int), 0, s);          // {1, 2}
  da_insert_range(&arr, sizeof(int), 1, r);         // {1, 3, 4, 5, 2}

  REQUIRE(arr.size == 5);
  REQUIRE(*(int*)da_at(&arr, sizeof(int), 0) == 1);
  REQUIRE(*(int*)da_at(&arr, sizeof(int), 1) == 3);
  REQUIRE(*(int*)da_at(&arr, sizeof(int), 4) == 2);

  da_destroy(&arr);
}

TEST_CASE("cArray – da_pop_back, da_pop_back_n", "[cArray]") {
  Array_t arr;
  da_init(&arr, sizeof(int), 0);

  int v = 1;
  da_assign(&arr, sizeof(int), 5, &v); // {1,1,1,1,1}

  da_pop_back(&arr);
  REQUIRE(arr.size == 4);

  da_pop_back_n(&arr, 2);
  REQUIRE(arr.size == 2);

  da_destroy(&arr);
}

TEST_CASE("cArray – da_erase", "[cArray]") {
  Array_t arr;
  da_init(&arr, sizeof(int), 0);

  int data[] = {10, 20, 30};
  da_assign_span(&arr, sizeof(int), Span_t{ .pData = data, .size = 3 });

  da_erase(&arr, sizeof(int), 1); // Remove 20 → {10, 30}
  REQUIRE(arr.size == 2);
  REQUIRE(*(int*)da_at(&arr, sizeof(int), 0) == 10);
  REQUIRE(*(int*)da_at(&arr, sizeof(int), 1) == 30);

  da_destroy(&arr);
}

TEST_CASE("cArray – da_clear", "[cArray]") {
  Array_t arr;
  da_init(&arr, sizeof(int), 0);

  int v = 5;
  da_assign(&arr, sizeof(int), 3, &v);
  REQUIRE(arr.size == 3);

  da_clear(&arr);
  REQUIRE(arr.size == 0);
  REQUIRE(arr.capacity > 0); // memory retained

  da_destroy(&arr);
}

TEST_CASE("cArray – da_fill", "[cArray]") {
  Array_t arr;
  da_init(&arr, sizeof(int), 0);

  int v = 0;
  da_assign(&arr, sizeof(int), 3, &v); // {0, 0, 0}

  int fill = 99;
  da_fill(&arr, sizeof(int), &fill); // {99, 99, 99}

  REQUIRE(*(int*)da_at(&arr, sizeof(int), 0) == 99);
  REQUIRE(*(int*)da_at(&arr, sizeof(int), 1) == 99);
  REQUIRE(*(int*)da_at(&arr, sizeof(int), 2) == 99);

  da_destroy(&arr);
}

TEST_CASE("cArray – da_assign, da_assign_span, da_assign_range", "[cArray]") {
  Array_t arr;
  da_init(&arr, sizeof(int), 0);

  int v = 7;
  da_assign(&arr, sizeof(int), 4, &v); // {7,7,7,7}
  REQUIRE(arr.size == 4);
  REQUIRE(*(int*)da_at(&arr, sizeof(int), 0) == 7);

  int data[] = {1, 2, 3};
  da_assign_span(&arr, sizeof(int), Span_t{ .pData = data, .size = 3 }); // {1,2,3}
  REQUIRE(arr.size == 3);
  REQUIRE(*(int*)da_at(&arr, sizeof(int), 1) == 2);

  Range_t r = { .pBegin = data, .pSentinel = &data[2] }; // {1, 2}
  da_assign_range(&arr, sizeof(int), r);
  REQUIRE(arr.size == 2);
  REQUIRE(*(int*)da_at(&arr, sizeof(int), 0) == 1);
  REQUIRE(*(int*)da_at(&arr, sizeof(int), 1) == 2);

  da_destroy(&arr);
}

TEST_CASE("cArray – da_swap", "[cArray]") {
  Array_t a, b;
  da_init(&a, sizeof(int), 0);
  da_init(&b, sizeof(int), 0);

  int va = 1, vb = 2;
  da_assign(&a, sizeof(int), 2, &va); // {1, 1}
  da_assign(&b, sizeof(int), 3, &vb); // {2, 2, 2}

  void* pDataA = a.pData;
  void* pDataB = b.pData;

  da_swap(&a, &b);

  REQUIRE(a.pData == pDataB);
  REQUIRE(b.pData == pDataA);
  REQUIRE(a.size == 3);
  REQUIRE(b.size == 2);

  da_destroy(&a);
  da_destroy(&b);
}

// --- Extended (Strategy) ---

TEST_CASE("cArray – da_append_ex (strategy)", "[cArray]") {
  Array_t arr;
  da_init(&arr, sizeof(int), 0);

  Strategy_t s = { .initialCapacity = 16, .growthFactor = 3.0f };
  int v = 5;
  int* p = (int*)da_append_ex(&arr, sizeof(int), &v, &s);

  REQUIRE(p != nullptr);
  REQUIRE(arr.capacity >= 16);
  REQUIRE(arr.size == 1);

  da_destroy(&arr);
}

TEST_CASE("cArray – da_append_span_ex, da_append_range_ex (strategy)", "[cArray]") {
  Array_t arr;
  da_init(&arr, sizeof(int), 0);

  Strategy_t s = { .initialCapacity = 20, .growthFactor = 2.0f };
  int data[] = {1, 2, 3, 4, 5};
  Span_t  sp = { .pData = data,     .size = 2 };
  Range_t r  = { .pBegin = &data[2], .pSentinel = &data[5] };

  REQUIRE(da_append_span_ex(&arr, sizeof(int), sp, &s));
  REQUIRE(da_append_range_ex(&arr, sizeof(int), r, &s));
  REQUIRE(arr.size == 5);

  da_destroy(&arr);
}

TEST_CASE("cArray – da_insert_ex (strategy)", "[cArray]") {
  Array_t arr;
  da_init(&arr, sizeof(int), 0);

  Strategy_t s = { .initialCapacity = 10, .growthFactor = 2.0f };
  int v1 = 10, v2 = 20;
  da_append_ex(&arr, sizeof(int), &v1, &s);
  da_insert_ex(&arr, sizeof(int), 0, &v2, &s); // {20, 10}

  REQUIRE(arr.size == 2);
  REQUIRE(*(int*)da_at(&arr, sizeof(int), 0) == 20);
  REQUIRE(*(int*)da_at(&arr, sizeof(int), 1) == 10);

  da_destroy(&arr);
}

TEST_CASE("cArray – da_insert_span_ex, da_insert_range_ex (strategy)", "[cArray]") {
  Array_t arr;
  da_init(&arr, sizeof(int), 0);

  Strategy_t s = { .initialCapacity = 10, .growthFactor = 2.0f };
  int data[] = {1, 2, 3, 4, 5};
  Span_t  sp = { .pData = data,     .size = 2 };
  Range_t r  = { .pBegin = &data[2], .pSentinel = &data[5] };

  REQUIRE(da_insert_span_ex(&arr, sizeof(int), 0, sp, &s));
  REQUIRE(da_insert_range_ex(&arr, sizeof(int), 0, r, &s));
  REQUIRE(arr.size == 5);

  da_destroy(&arr);
}
