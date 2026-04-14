#include <iostream>
#include <type_traits>
#include <cstdlib>
#include <cassert>
#include <vector>
#include <string>

#include <cLib/Types.hpp>

// Helper to check if two values are equal (for assertions)
#define ASSERT_EQ(val1, val2) \
    do { if ((val1) != (val2)) { \
        std::cerr << "Assertion Failed: " << #val1 << " == " << #val2 \
                  << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::abort(); \
    } } while (0)

void TestSpan() {
    std::cout << "[Test] Span Logic..." << std::endl;
    int raw[] = {1, 2, 3, 4, 5};
    Span<int> s(raw, 5);

    ASSERT_EQ(s.size, 5);
    ASSERT_EQ(s[0], 1);
    ASSERT_EQ(s[4], 5);

    int sum = 0;
    for (int n : s) sum += n;
    ASSERT_EQ(sum, 15);
}

void TestRangeUniversal() {
    std::cout << "[Test] Range Universal Sentinel Logic..." << std::endl;

    // 1. Value/Pointer Sentinel
    const char* names[] = {"Alpha", "Beta", "Gamma", nullptr};
    Range<const char*> r1(names, nullptr);
    int count1 = 0;
    for (auto n : r1) { (void)n; count1++; }
    ASSERT_EQ(count1, 3);

    // 2. Value/Char Sentinel
    const char* str = "Rigid\nTest";
    Range<const char> r2(str, (void*)(uintptr_t)'\n');
    std::string result2;
    for (char c : r2) result2 += c;
    ASSERT_EQ(result2, "Rigid");

    // 3. Value/Int Sentinel
    int stream[] = {10, 20, 30, -1, 99};
    Range<int> r3(stream, (void*)(uintptr_t)-1);
    int sum3 = 0;
    for (int i : r3) sum3 += i;
    ASSERT_EQ(sum3, 60);

    // 4. Address Sentinel (STL style)
    int block[] = {100, 200, 300, 400, 500};
    Range<int> r4(block, &block[3]); // Should stop at index 3 (400)
    int count4 = 0;
    for (int i : r4) { (void)i; count4++; }
    ASSERT_EQ(count4, 3);
    ASSERT_EQ(*r4.begin().ptr, 100);
}

void TestABICompatibility() {
    std::cout << "[Test] ABI & POD Layout..." << std::endl;

    // Standard Layout ensures C compatibility
    static_assert(std::is_standard_layout_v<Span<int>>);
    static_assert(std::is_standard_layout_v<Range<int>>);
    static_assert(std::is_standard_layout_v<Array<int>>);
    
    // Size check ensures no hidden vtables or padding in the wrappers
    ASSERT_EQ(sizeof(Span<int>), sizeof(Span_t));
    ASSERT_EQ(sizeof(Range<int>), sizeof(Range_t));
    ASSERT_EQ(sizeof(Array<int>), sizeof(Array_t));

    // Trivially copyable ensures safety for memcpy/malloc
    static_assert(std::is_trivially_copyable_v<Span<double>>);
    static_assert(std::is_trivially_copyable_v<Range<float>>);
}

void TestBoundaries() {
    std::cout << "[Test] Boundary Assertions (Logic only)..." << std::endl;
    int raw[] = {10, 20};
    Span<int> s(raw, 2);

    // Valid access
    int x = s[1]; 
    ASSERT_EQ(x, 20);

    // Note: To actually test that an assert FAILS, you'd need 
    // a death-test framework. For now, we trust the logic:
    // s[2] would trigger the assert here.
}

int main() {
    try {
        TestABICompatibility();
        TestSpan();
        TestRangeUniversal();
        TestBoundaries();

        std::cout << "\n======================================" << std::endl;
        std::cout << "  ALL RIGID TESTS PASSED SUCCESSFULLY" << std::endl;
        std::cout << "======================================" << std::endl;
    } catch (...) {
        std::cerr << "Unknown error during testing!" << std::endl;
        return 1;
    }
    return 0;
}
