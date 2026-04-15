#include <catch2/catch_test_macros.hpp>
#include <cLib/collection/Array.hpp>
#include <string>
#include <type_traits>

TEST_CASE("Array ABI Compatibility & Memory Layout", "[Array][abi]") {
  // Ensure Array<T> matches Array_t memory layout
  static_assert(std::is_standard_layout_v<Array<int>>, "Array<int> must be standard layout");
  static_assert(std::is_standard_layout_v<Array<float>>, "Array<float> must be standard layout");
  static_assert(std::is_trivially_copyable_v<Array<double>>, "Array<T> must be trivially copyable");

  // Size must match exactly
  REQUIRE(sizeof(Array<int>) == sizeof(Array_t));
  REQUIRE(sizeof(Array<float>) == sizeof(Array_t));
  REQUIRE(sizeof(Array<double>) == sizeof(Array_t));
}

TEST_CASE("Array Construction & Initialization", "[Array]") {
  // Default constructor
  Array<int> arr1;
  REQUIRE(arr1.impl.pData);
  REQUIRE(arr1.impl.size == 0);
  REQUIRE(arr1.impl.capacity == 32);

  // Capacity constructor
  Array<int> arr2(5);
  REQUIRE(arr2.capacity() == 5);
  arr2.destroy();

  // Static create helper
  Array<int> arr3 = Array<int>(10);
  REQUIRE(arr3.impl.pData != nullptr);
  REQUIRE(arr3.capacity() == 10);
  arr3.destroy();
}

TEST_CASE("Array Element Access & Bounds Checking", "[Array]") {
  Array<int> arr = Array<int>( 5 );
  arr.assign(5, 0); // Fill with 5 zeros

  // operator[]
  arr[0] = 10;
  arr[1] = 20;
  arr[2] = 30;
  REQUIRE(arr[0] == 10);
  REQUIRE(arr[1] == 20);
  REQUIRE(arr[2] == 30);

  // Const access
  const Array<int>& constRef = arr;
  REQUIRE(constRef[0] == 10);
  REQUIRE(constRef[1] == 20);

  // data()
  int* ptr = arr.data();
  REQUIRE(ptr != nullptr);
  REQUIRE(ptr[0] == 10);

  // at() - safe access returning nullptr
  int* pVal = arr.at(2);
  REQUIRE(pVal != nullptr);
  REQUIRE(*pVal == 30);

  // at() out of bounds
  REQUIRE(arr.at(10) == nullptr);

  arr.destroy();
}

TEST_CASE("Array Iterators & Ranges", "[Array]") {
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
  REQUIRE(begin == arr.data());
  REQUIRE(end == arr.data() + 5);

  // Range iteration
  int sum = 0;
  for (int* p = arr.begin(); p != arr.end(); ++p) {
    sum += *p;
  }
  REQUIRE(sum == 15); // 1+2+3+4+5

  // Const iterators
  const Array<int>& constArr = arr;
  const int* cbegin = constArr.begin();
  const int* cend = constArr.end();
  REQUIRE(cbegin == constArr.data());
  REQUIRE(cend == constArr.data() + 5);

  arr.destroy();
}

TEST_CASE("Array Inspection Methods", "[Array]") {
  Array<int> arr = Array<int>(10);

  // Empty array
  REQUIRE(arr.isEmpty());
  REQUIRE(arr.size() == 0);
  REQUIRE(arr.capacity() == 10);

  // Add elements
  int val = 42;
  arr.append(val);
  arr.append(val);

  REQUIRE_FALSE(arr.isEmpty());
  REQUIRE(arr.size() == 2);
  REQUIRE(arr.capacity() == 10);

  arr.destroy();
}

TEST_CASE("Array Capacity Management", "[Array]") {
  Array<float> arr = Array<float>(5);
  REQUIRE(arr.capacity() == 5);

  // Reserve larger capacity
  bool ok = arr.reserve(20);
  REQUIRE(ok);
  REQUIRE(arr.capacity() == 20);

  // Resize with default values
  arr.resize(15);
  REQUIRE(arr.size() == 15);

  // Shrink to fit
  ok = arr.shrink_to_fit();
  REQUIRE(ok);
  REQUIRE(arr.capacity() == 15);

  arr.destroy();
}

TEST_CASE("Array Modifiers (Append, Insert, Erase, Pop)", "[Array]") {
  Array<int> arr = Array<int>();

  // Append
  int v1 = 10, v2 = 20, v3 = 30;
  arr.append(v1);
  arr.append(v3);
  REQUIRE(arr.size() == 2);
  REQUIRE(arr[0] == 10);
  REQUIRE(arr[1] == 30);

  // Insert
  arr.insert(1, v2); // {10, 20, 30}
  REQUIRE(arr.size() == 3);
  REQUIRE(arr[1] == 20);
  REQUIRE(arr[2] == 30);

  // Pop back
  arr.pop_back();
  REQUIRE(arr.size() == 2);

  // Erase
  arr.erase(0); // {20}
  REQUIRE(arr.size() == 1);
  REQUIRE(arr[0] == 20);

  // Fill
  arr.fill(99);
  REQUIRE(arr[0] == 99);

  // Assign
  arr.assign(3, 77); // {77, 77, 77}
  REQUIRE(arr.size() == 3);
  REQUIRE(arr[0] == 77);
  REQUIRE(arr[1] == 77);
  REQUIRE(arr[2] == 77);

  // Clear
  arr.clear();
  REQUIRE(arr.size() == 0);
  REQUIRE(arr.isEmpty());

  arr.destroy();
}

TEST_CASE("Array pop_back(n)", "[Array]") {
  Array<int> arr = Array<int>();
  arr.assign(5, 1); // {1, 1, 1, 1, 1}
  REQUIRE(arr.size() == 5);

  arr.pop_back(2); // Remove last 2
  REQUIRE(arr.size() == 3);

  arr.destroy();
}

TEST_CASE("Array Swap", "[Array]") {
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
  REQUIRE(arr1.impl.pData == pData2Before);
  REQUIRE(arr2.impl.pData == pData1Before);
  REQUIRE(arr1.impl.size == size2Before); // arr1 now has arr2's former size
  REQUIRE(arr2.impl.size == size1Before); // arr2 now has arr1's former size

  arr1.destroy();
  arr2.destroy();
}

TEST_CASE("Array Extended Modifiers with Strategy", "[Array]") {
  Array<int> arr = Array<int>();

  Strategy_t strat = {.initialCapacity = 10, .growthFactor = 2.5f};

  int val = 5;
  int* pResult = arr.append(val, &strat);  // Overloaded append
  REQUIRE(pResult != nullptr);
  REQUIRE(arr.capacity() >= 10); // Should use initial capacity

  arr.insert(0, 3, &strat);  // Overloaded insert
  REQUIRE(arr.size() == 2);

  arr.destroy();
}

TEST_CASE("Array Move Semantics (Ownership Transfer)", "[Array]") {
  Array<int> arr1 = Array<int>(5);
  arr1.assign(5, 42);

  void* pData = arr1.impl.pData;
  uint32_t size = arr1.impl.size;
  uint32_t capacity = arr1.impl.capacity;

  // Move ownership to arr2
  Array<int> arr2(&arr1);

  // arr2 has arr1's data
  REQUIRE(arr2.impl.pData == pData);
  REQUIRE(arr2.impl.size == size);
  REQUIRE(arr2.impl.capacity == capacity);

  // arr1 is now empty (ownership transferred)
  REQUIRE(arr1.impl.pData == nullptr);
  REQUIRE(arr1.impl.size == 0);
  REQUIRE(arr1.impl.capacity == 0);

  arr2.destroy();
}

TEST_CASE("Array Type Safety", "[Array]") {
  // Array<float>
  Array<float> floatArr = Array<float>(3);
  floatArr.assign(3, 0.0f);
  floatArr[0] = 1.5f;
  floatArr[1] = 2.5f;
  REQUIRE(floatArr[0] == 1.5f);
  REQUIRE(floatArr[1] == 2.5f);
  floatArr.destroy();

  // Array<double>
  Array<double> doubleArr = Array<double>(3);
  doubleArr.assign(3, 0.0);
  doubleArr[0] = 3.14159;
  REQUIRE(doubleArr[0] == 3.14159);
  doubleArr.destroy();

  // Array<uint32_t>
  Array<uint32_t> uintArr = Array<uint32_t>(3);
  uintArr.assign(3, 0);
  uintArr[0] = 1000000;
  REQUIRE(uintArr[0] == 1000000);
  uintArr.destroy();
}
