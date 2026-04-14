#include <iostream>
#include <cstdlib>
#include <cassert>
#include <string>
#include <type_traits>

#include <cLib/collection/Array.hpp>

/**
 * Helper to check if two values are equal.
 * Uses std::cerr and std::abort for clear error reporting.
 */
#define ASSERT_EQ(val1, val2) \
  do { if ((val1) != (val2)) { \
    std::cerr << "Assertion Failed: " << #val1 << " == " << #val2 \
              << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
    std::abort(); \
  } } while (0)

#define ASSERT_NE(val1, val2) \
  do { if ((val1) == (val2)) { \
    std::cerr << "Assertion Failed: " << #val1 << " != " << #val2 \
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

void TestABICompatibility() {
  std::cout << "[Test] ABI Compatibility & Memory Layout..." << std::endl;

  // Ensure Array<T> matches Array_t memory layout
  static_assert(std::is_standard_layout_v<Array<int>>, "Array<int> must be standard layout");
  static_assert(std::is_standard_layout_v<Array<float>>, "Array<float> must be standard layout");
  static_assert(std::is_trivially_copyable_v<Array<double>>, "Array<T> must be trivially copyable");

  // Size must match exactly
  ASSERT_EQ(sizeof(Array<int>), sizeof(Array_t));
  ASSERT_EQ(sizeof(Array<float>), sizeof(Array_t));
  ASSERT_EQ(sizeof(Array<double>), sizeof(Array_t));

  std::cout << "  ✓ Layout matches Array_t structure" << std::endl;
}

void TestConstruction() {
  std::cout << "[Test] Construction & Initialization..." << std::endl;

  // Default constructor
  // Should initialize 
  // cArray.c L12
  Array<int> arr1;
  ASSERT_TRUE(arr1.impl.pData);
  ASSERT_EQ(arr1.impl.size, 0);
  ASSERT_EQ(arr1.impl.capacity, 32);

  // Capacity constructor
  Array<int> arr2(5);
  ASSERT_EQ(arr2.capacity(), 5);
  arr2.destroy();

  // Static create helper
  Array<int> arr3 = Array<int>(10);
  ASSERT_NE(arr3.impl.pData, nullptr);
  ASSERT_EQ(arr3.capacity(), 10);
  arr3.destroy();

  std::cout << "  ✓ Construction works correctly" << std::endl;
}

void TestElementAccess() {
  std::cout << "[Test] Element Access & Bounds Checking..." << std::endl;

  Array<int> arr = Array<int>( 5 );
  arr.assign(5, 0); // Fill with 5 zeros

  // operator[]
  arr[0] = 10;
  arr[1] = 20;
  arr[2] = 30;
  ASSERT_EQ(arr[0], 10);
  ASSERT_EQ(arr[1], 20);
  ASSERT_EQ(arr[2], 30);

  // Const access
  const Array<int>& constRef = arr;
  ASSERT_EQ(constRef[0], 10);
  ASSERT_EQ(constRef[1], 20);

  // data()
  int* ptr = arr.data();
  ASSERT_NE(ptr, nullptr);
  ASSERT_EQ(ptr[0], 10);

  // at() - safe access returning nullptr
  int* pVal = arr.at(2);
  ASSERT_NE(pVal, nullptr);
  ASSERT_EQ(*pVal, 30);

  // at() out of bounds
  ASSERT_EQ(arr.at(10), nullptr);

  arr.destroy();
  std::cout << "  ✓ Element access works correctly" << std::endl;
}

void TestIterators() {
  std::cout << "[Test] Iterators & Ranges..." << std::endl;

  Array<int> arr = Array<int>(5);
  arr.assign(5, 0);
  arr[0] = 1;
  arr[1] = 2;
  arr[2] = 3;
  arr[3] = 4;
  arr[4] = 5;

  // begin/end
  int* begin = arr.begin();
  int* end = arr.end();
  ASSERT_EQ(begin, arr.data());
  ASSERT_EQ(end, arr.data() + 5);

  // Range iteration
  int sum = 0;
  for (int* p = arr.begin(); p != arr.end(); ++p) {
    sum += *p;
  }
  ASSERT_EQ(sum, 15); // 1+2+3+4+5

  // Const iterators
  const Array<int>& constArr = arr;
  const int* cbegin = constArr.begin();
  const int* cend = constArr.end();
  ASSERT_EQ(cbegin, constArr.data());
  ASSERT_EQ(cend, constArr.data() + 5);

  arr.destroy();
  std::cout << "  ✓ Iterators work correctly" << std::endl;
}

void TestInspection() {
  std::cout << "[Test] Inspection Methods..." << std::endl;

  Array<int> arr = Array<int>(10);

  // Empty array
  ASSERT_TRUE(arr.isEmpty());
  ASSERT_EQ(arr.size(), 0);
  ASSERT_EQ(arr.capacity(), 10);

  // Add elements
  int val = 42;
  arr.append(val);
  arr.append(val);

  ASSERT_TRUE(!arr.isEmpty());
  ASSERT_EQ(arr.size(), 2);
  ASSERT_EQ(arr.capacity(), 10);

  arr.destroy();
  std::cout << "  ✓ Inspection methods work correctly" << std::endl;
}

void TestCapacityManagement() {
  std::cout << "[Test] Capacity Management..." << std::endl;

  Array<float> arr = Array<float>(5);
  ASSERT_EQ(arr.capacity(), 5);

  // Reserve larger capacity
  bool ok = arr.reserve(20);
  ASSERT_TRUE(ok);
  ASSERT_EQ(arr.capacity(), 20);

  // Resize with default values
  arr.resize(15);
  ASSERT_EQ(arr.size(), 15);

  // Shrink to fit
  ok = arr.shrink_to_fit();
  ASSERT_TRUE(ok);
  ASSERT_EQ(arr.capacity(), 15);

  arr.destroy();
  std::cout << "  ✓ Capacity management works correctly" << std::endl;
}

void TestModifiers() {
  std::cout << "[Test] Append, Insert, Erase, Pop..." << std::endl;

  Array<int> arr = Array<int>();

  // Append
  int v1 = 10, v2 = 20, v3 = 30;
  arr.append(v1);
  arr.append(v3);
  ASSERT_EQ(arr.size(), 2);
  ASSERT_EQ(arr[0], 10);
  ASSERT_EQ(arr[1], 30);

  // Insert
  arr.insert(1, v2); // {10, 20, 30}
  ASSERT_EQ(arr.size(), 3);
  ASSERT_EQ(arr[1], 20);
  ASSERT_EQ(arr[2], 30);

  // Pop back
  arr.pop_back();
  ASSERT_EQ(arr.size(), 2);

  // Erase
  arr.erase(0); // {20}
  ASSERT_EQ(arr.size(), 1);
  ASSERT_EQ(arr[0], 20);

  // Fill
  arr.fill(99);
  ASSERT_EQ(arr[0], 99);

  // Assign
  arr.assign(3, 77); // {77, 77, 77}
  ASSERT_EQ(arr.size(), 3);
  ASSERT_EQ(arr[0], 77);
  ASSERT_EQ(arr[1], 77);
  ASSERT_EQ(arr[2], 77);

  // Clear
  arr.clear();
  ASSERT_EQ(arr.size(), 0);
  ASSERT_TRUE(arr.isEmpty());

  arr.destroy();
  std::cout << "  ✓ Modifiers work correctly" << std::endl;
}

void TestPopBackN() {
  std::cout << "[Test] pop_back(n)..." << std::endl;

  Array<int> arr = Array<int>();
  arr.assign(5, 1); // {1, 1, 1, 1, 1}
  ASSERT_EQ(arr.size(), 5);

  arr.pop_back(2); // Remove last 2
  ASSERT_EQ(arr.size(), 3);

  arr.destroy();
  std::cout << "  ✓ pop_back(n) works correctly" << std::endl;
}

void TestSwap() {
  std::cout << "[Test] Swap..." << std::endl;

  Array<int> arr1 = Array<int>();
  Array<int> arr2 = Array<int>();

  arr1.assign(2, 10); // {10, 10}
  arr2.assign(3, 20); // {20, 20, 20}

  uint32_t size1Before = arr1.impl.size;
  uint32_t size2Before = arr2.impl.size;
  void* pData1Before = arr1.impl.pData;
  void* pData2Before = arr2.impl.pData;

  arr1.swap(&arr2);

  // After swap, arr1 has arr2's data and arr2 has arr1's data
  ASSERT_EQ(arr1.impl.pData, pData2Before);
  ASSERT_EQ(arr2.impl.pData, pData1Before);
  ASSERT_EQ(arr1.impl.size, size2Before); // arr1 now has arr2's former size
  ASSERT_EQ(arr2.impl.size, size1Before); // arr2 now has arr1's former size

  arr1.destroy();
  arr2.destroy();
  std::cout << "  ✓ Swap works correctly" << std::endl;
}

void TestStrategyModifiers() {
  std::cout << "[Test] Extended Modifiers with Strategy..." << std::endl;

  Array<int> arr = Array<int>();

  Strategy_t strat = {.initialCapacity = 10, .growthFactor = 2.5f};

  int val = 5;
  int* pResult = arr.append(val, &strat);  // Overloaded append
  ASSERT_NE(pResult, nullptr);
  ASSERT_GE(arr.capacity(), 10); // Should use initial capacity

  arr.insert(0, 3, &strat);  // Overloaded insert
  ASSERT_EQ(arr.size(), 2);

  arr.destroy();
  std::cout << "  ✓ Strategy modifiers work correctly" << std::endl;
}

void TestMoveSemantics() {
  std::cout << "[Test] Move Semantics (Ownership Transfer)..." << std::endl;

  Array<int> arr1 = Array<int>(5);
  arr1.assign(5, 42);

  void* pData = arr1.impl.pData;
  uint32_t size = arr1.impl.size;
  uint32_t capacity = arr1.impl.capacity;

  // Move ownership to arr2
  Array<int> arr2(&arr1);

  // arr2 has arr1's data
  ASSERT_EQ(arr2.impl.pData, pData);
  ASSERT_EQ(arr2.impl.size, size);
  ASSERT_EQ(arr2.impl.capacity, capacity);

  // arr1 is now empty (ownership transferred)
  ASSERT_EQ(arr1.impl.pData, nullptr);
  ASSERT_EQ(arr1.impl.size, 0);
  ASSERT_EQ(arr1.impl.capacity, 0);

  arr2.destroy();
  std::cout << "  ✓ Move semantics work correctly" << std::endl;
}

void TestTypeSafety() {
  std::cout << "[Test] Type Safety with Different Element Types..." << std::endl;

  // Array<float>
  Array<float> floatArr = Array<float>(3);
  floatArr.assign(3, 0.0f);
  floatArr[0] = 1.5f;
  floatArr[1] = 2.5f;
  ASSERT_EQ(floatArr[0], 1.5f);
  ASSERT_EQ(floatArr[1], 2.5f);
  floatArr.destroy();

  // Array<double>
  Array<double> doubleArr = Array<double>(3);
  doubleArr.assign(3, 0.0);
  doubleArr[0] = 3.14159;
  ASSERT_EQ(doubleArr[0], 3.14159);
  doubleArr.destroy();

  // Array<uint32_t>
  Array<uint32_t> uintArr = Array<uint32_t>(3);
  uintArr.assign(3, 0);
  uintArr[0] = 1000000;
  ASSERT_EQ(uintArr[0], 1000000);
  uintArr.destroy();

  std::cout << "  ✓ Type safety preserved across different types" << std::endl;
}

int main() {
  try {
    TestABICompatibility();
    TestConstruction();
    TestElementAccess();
    TestIterators();
    TestInspection();
    TestCapacityManagement();
    TestModifiers();
    TestPopBackN();
    TestSwap();
    TestStrategyModifiers();
    TestMoveSemantics();
    TestTypeSafety();

    std::cout << "\n======================================" << std::endl;
    std::cout << "  ALL ARRAY<T> TESTS PASSED (100%)"       << std::endl;
    std::cout << "======================================"   << std::endl;
  } catch (...) {
    std::cerr << "Test suite crashed with unknown exception!" << std::endl;
    return 1;
  }
  return 0;
}
