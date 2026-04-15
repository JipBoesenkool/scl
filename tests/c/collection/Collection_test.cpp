#include <catch2/catch_test_macros.hpp>
#include <cLib/Types.hpp>
#include <string>

TEST_CASE("Span Logic", "[span]") {
    int raw[] = {1, 2, 3, 4, 5};
    Span<int> s(raw, 5);

    REQUIRE(s.size == 5);
    REQUIRE(s[0] == 1);
    REQUIRE(s[4] == 5);

    int sum = 0;
    for (int n : s) sum += n;
    REQUIRE(sum == 15);
}

TEST_CASE("Range Universal Sentinel Logic", "[range]") {
    // 1. Value/Pointer Sentinel
    const char* names[] = {"Alpha", "Beta", "Gamma", nullptr};
    Range<const char*> r1(names, nullptr);
    int count1 = 0;
    for (auto n : r1) { (void)n; count1++; }
    REQUIRE(count1 == 3);

    // 2. Value/Char Sentinel
    const char* str = "Rigid\nTest";
    Range<const char> r2(str, (void*)(uintptr_t)'\n');
    std::string result2;
    for (char c : r2) result2 += c;
    REQUIRE(result2 == "Rigid");

    // 3. Value/Int Sentinel
    int stream[] = {10, 20, 30, -1, 99};
    Range<int> r3(stream, (void*)(uintptr_t)-1);
    int sum3 = 0;
    for (int i : r3) sum3 += i;
    REQUIRE(sum3 == 60);

    // 4. Address Sentinel (STL style)
    int block[] = {100, 200, 300, 400, 500};
    Range<int> r4(block, &block[3]); // Should stop at index 3 (400)
    int count4 = 0;
    for (int i : r4) { (void)i; count4++; }
    REQUIRE(count4 == 3);
    REQUIRE(*r4.begin().ptr == 100);
}

TEST_CASE("ABI & POD Layout", "[abi]") {
    // Standard Layout ensures C compatibility
    static_assert(std::is_standard_layout_v<Span<int>>);
    static_assert(std::is_standard_layout_v<Range<int>>);
    static_assert(std::is_standard_layout_v<Array<int>>);
    
    // Size check ensures no hidden vtables or padding in the wrappers
    REQUIRE(sizeof(Span<int>) == sizeof(Span_t));
    REQUIRE(sizeof(Range<int>) == sizeof(Range_t));
    REQUIRE(sizeof(Array<int>) == sizeof(Array_t));

    // Trivially copyable ensures safety for memcpy/malloc
    static_assert(std::is_trivially_copyable_v<Span<double>>);
    static_assert(std::is_trivially_copyable_v<Range<float>>);
}

TEST_CASE("Boundary Assertions (Logic only)", "[boundary]") {
    int raw[] = {10, 20};
    Span<int> s(raw, 2);

    // Valid access
    int x = s[1]; 
    REQUIRE(x == 20);

    // Note: To actually test that an assert FAILS, you'd need 
    // a death-test framework. For now, we trust the logic:
    // s[2] would trigger the assert here.
}
