#include <cLib/string/String.hpp>

// --- Lifecycle & Ownership ---

String::String() {
  this->impl = INVALID_STRING;
}

String::String(const char* str) {
  this->impl = string_create(str);
}

String::String(StringView_t sv) {
  this->impl = string_create_from_sv(sv);
}

String::String(String* pOther) {
  // TODO(allocator): when allocator-managed strings exist, this constructor must
  // update the zone block's ppUser to point at this->impl.heap.pData, so the
  // allocator can patch the correct location during defrag/compact.
  if (pOther) {
    this->impl = pOther->impl;
    pOther->impl = INVALID_STRING;
  }
}

void String::init(const char* str) {
  this->impl = string_create(str);
}

void String::init(StringView_t sv) {
  this->impl = string_create_from_sv(sv);
}

String String::copy() const {
  return String(string_copy(&this->impl));
}

void String::destroy() {
  string_destroy(&this->impl);
}

// --- Conversion ---

const char* String::c_str() const {
  return string_to_c_str(&this->impl);
}

StringView_t String::sv() const {
  return string_to_sv(&this->impl);
}

// --- Operators ---

char String::operator[](size_t index) const {
  return string_at(&this->impl, index);
}

bool String::operator==(StringView_t other) const {
  return sv_compare(this->sv(), other) == 0;
}

bool String::operator!=(StringView_t other) const {
  return sv_compare(this->sv(), other) != 0;
}

bool String::operator==(const String& other) const {
  return string_compare(&this->impl, &other.impl) == 0;
}

bool String::operator!=(const String& other) const {
  return string_compare(&this->impl, &other.impl) != 0;
}

const char* String::begin() const {
  return this->sv().data;
}

const char* String::end() const {
  return this->sv().data + this->sv().length;
}

// --- Inspections ---

size_t String::length() const {
  return string_length(&this->impl);
}

size_t String::size() const {
  return string_length(&this->impl);
}

size_t String::capacity() const {
  return this->impl.capacity;
}

bool String::isEmpty() const {
  return string_is_empty(&this->impl);
}

// --- StringView API ---

bool String::contains(StringView_t other) const {
  return sv_contains(this->sv(), other);
}

int String::compare(StringView_t other) const {
  return sv_compare(this->sv(), other);
}

bool String::starts_with(StringView_t sv) const {
  return sv_starts_with(this->sv(), sv);
}

bool String::ends_with(StringView_t sv) const {
  return sv_ends_with(this->sv(), sv);
}

StringView_t String::find(StringView_t search) const {
  size_t idx = sv_find(this->sv(), search);
  if (idx == INVALID_INDEX) return INVALID_SV;
  return { this->sv().data + idx, search.length };
}

size_t String::find_first_of(char ch) const {
  const char buf[1] = { ch };
  return sv_find_first_of(this->sv(), { buf, 1 });
}

size_t String::find_first_of(StringView_t search) const {
  return sv_find_first_of(this->sv(), search);
}

StringView_t String::rfind(StringView_t search) const {
  size_t idx = sv_rfind(this->sv(), search);
  if (idx == INVALID_INDEX) return INVALID_SV;
  return { this->sv().data + idx, search.length };
}

size_t String::find_last_of(char ch) const {
  return sv_find_last_of(this->sv(), { &ch, 1 });
}

size_t String::find_last_of(StringView_t search) const {
  return sv_find_last_of(this->sv(), search);
}

// --- Views & Slicing ---

StringView_t String::trim() const {
  return sv_trim(this->sv());
}

StringView_t String::ltrim() const {
  return sv_ltrim(this->sv());
}

StringView_t String::rtrim() const {
  return sv_rtrim(this->sv());
}

StringView_t String::slice(size_t start, size_t end) const {
  return sv_slice(this->sv(), start, end);
}

// --- Transformations (Returns NEW String) ---
String String::substring(size_t start, size_t end) const {
  return String(string_substring(&this->impl, start, end));
}

String String::erase(size_t start, size_t count) const {
  return String(string_erase(&this->impl, start, count));
}

String String::erase(const StringView_t view) const {
  return String(string_erase_sv(&this->impl, view));
}

String String::toUpper() const {
  return String(string_to_upper(&this->impl));
}

String String::toLower() const {
  return String(string_to_lower(&this->impl));
}

String String::repeat(uint32_t count) const {
  return String(string_repeat(&this->impl, (size_t)count));
}

String String::replace(const StringView_t oldView, const StringView_t newView) const {
  return String(string_replace(&this->impl, oldView, newView));
}

String String::replace(const char* pOld, const char* pNew) const {
  return String(string_replace(&this->impl, create_sv(pOld), create_sv(pNew)));
}

String String::lpad(char pad, int count) const {
  return String(string_lpad(&this->impl, pad, (size_t)count));
}

String String::rpad(char pad, int count) const {
  return String(string_rpad(&this->impl, pad, (size_t)count));
}

// --- Private ---
String::String(String_t string) : impl(string) {}
