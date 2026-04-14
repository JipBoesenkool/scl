#ifndef SPAN_HPP
#define SPAN_HPP

#include <cLib/collection/cSpan.h>

// Generic Span (Pointer + Size)
template <typename T>
struct Span : public Span_t
{
  Span(T *ptr, size_t size)
  {
    this->pData = (void *)ptr;
    this->size = size;
  }

  T *begin() const { return static_cast<T *>(pData); }
  T *end() const { return static_cast<T *>(pData) + size; }
  
  T &operator[](size_t i) const
  {
    assert(i < size && "Span index out of bounds!");
    return begin()[i];
  }
};

#endif // SPAN_HPP
