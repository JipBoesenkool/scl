#ifndef STRING_HPP
#define STRING_HPP

#include <cLib/string/cString.h>
#include <cLib/string/cStringView.h>

struct String
{
  String_t impl;

  // --- Lifecycle & Ownership ---
  String();
  String(const char *str);
  String(StringView_t sv);
  String(String *pOther);

  void init(const char *str);
  void init(StringView_t sv);
  String copy() const;
  void destroy();

  // --- Conversion ---
  const char *c_str() const;
  StringView_t sv() const;

  // --- Operators ---
  char operator[](size_t index) const;
  bool operator==(StringView_t other) const;
  bool operator!=(StringView_t other) const;
  bool operator==(const String &other) const;
  bool operator!=(const String &other) const;
  const char *begin() const;
  const char *end() const;

  // --- Inspections ---
  size_t length() const;
  size_t size() const;
  size_t capacity() const;
  bool isEmpty() const;

  // --- StringView API ---
  bool contains(StringView_t other) const;
  int compare(StringView_t other) const;
  bool starts_with(StringView_t sv) const;
  bool ends_with(StringView_t sv) const;
  StringView_t find(StringView_t search) const;
  size_t find_first_of(char ch) const;
  size_t find_first_of(StringView_t search) const;
  StringView_t rfind(StringView_t search) const;
  size_t find_last_of(char ch) const;
  size_t find_last_of(StringView_t search) const;

  // --- Views & Slicing ---
  StringView_t trim() const;
  StringView_t ltrim() const;
  StringView_t rtrim() const;
  StringView_t slice(size_t start, size_t end) const;

  // --- Transformations (Returns NEW String) ---
  String substring(size_t start, size_t end) const;
  String erase(size_t start, size_t count) const;
  String erase(const StringView_t view) const;
  String toUpper() const;
  String toLower() const;
  String repeat(uint32_t count) const;
  String replace(const StringView_t oldView, const StringView_t newView) const;
  String replace(const char *pOld, const char *pNew) const;
  String lpad(char pad, int count) const;
  String rpad(char pad, int count) const;

private:
  explicit String(String_t string);
  String(const String &) = delete;
  String &operator=(const String &) = delete;
};

static_assert(sizeof(String) == sizeof(String_t),
              "String.hpp: String wrapper must match memory layout of String_t");

#endif
