#ifndef ARRAY_HPP
#define ARRAY_HPP

#include <cLib/collection/cArray.h>
#include <cassert>

/**
 * Generic C++ wrapper for dynamic Array_t.
 * Provides type-safe access and convenience methods.
 * Does NOT provide RAII — manual lifecycle required.
 */
template <typename T>
struct Array
{
  Array_t impl;
  // --- Lifecycle & Construction ---
  Array();
  Array(uint32_t capacity);
  void destroy();

  // Copy ownership transfer (move semantics simulation)
  Array(Array<T> *pOther);

  // --- Element Access ---
  T *data() const;
  T &operator[](uint32_t index);
  const T &operator[](uint32_t index) const;
  
  T *at(uint32_t index);
  const T *at(uint32_t index) const;

  // --- Iterators ---
  T *begin();
  const T *begin() const;
  T *end();
  const T *end() const;

  // --- Inspection ---
  uint32_t size() const;  // Returns size (don't shadow the member variable)
  uint32_t capacity() const;
  bool isEmpty() const;

  // --- Capacity Management ---
  bool reserve(uint32_t newCapacity);
  bool shrink_to_fit();
  bool resize(uint32_t newSize);

  // --- Modifiers ---
  T *append(const T &item);
  T *append(const T &item, const Strategy_t *pStrategy);
  T *insert(uint32_t index, const T &item);
  T *insert(uint32_t index, const T &item, const Strategy_t *pStrategy);
  void pop_back();
  void pop_back(uint32_t count);
  void erase(uint32_t index);
  void clear();
  void fill(const T &item);
  void assign(uint32_t count, const T &item);
  void swap(Array<T> *pOther);
};

// Type-safe assertion
template <typename T>
constexpr bool validate_array_layout() {
  static_assert(sizeof(Array<T>) == sizeof(Array_t),
    "Array<T>: wrapper must match memory layout of Array_t");
  return true;
}

// Include template implementations
#include <details/collection/Array.inl>

#endif // ARRAY_HPP
