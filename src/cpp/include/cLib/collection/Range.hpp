#ifndef RANGE_HPP
#define RANGE_HPP

#include <cLib/collection/cRange.h>

// Generic Range (Begin + Sentinel)
template <typename T>
struct Range : public Range_t
{
  Range(T *b, void *s = nullptr)
  {
    this->pBegin = (void *)b;
    this->pSentinel = s;
  }

  struct Iterator
  {
    T *ptr;
    void *sValue;

    bool operator!=(const Iterator &) const
    {
      // 1. Check if the current pointer ADDRESS matches the sentinel
      if ((void *)ptr == sValue)
        return false;

      // 2. Check if the current bit-pattern VALUE matches the sentinel
      T current = *ptr;
      if constexpr (std::is_pointer_v<T>)
      {
        return (void *)current != sValue;
      }
      else
      {
        return (void *)(uintptr_t)current != sValue;
      }
    }
    void operator++() { ptr++; }
    T &operator*() const { return *ptr; }
  };

  Iterator begin() const { return {static_cast<T *>(pBegin), pSentinel}; }
  Iterator end() const { return {nullptr, pSentinel}; }
};

#endif // RANGE_HPP
