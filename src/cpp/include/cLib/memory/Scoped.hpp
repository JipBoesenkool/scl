#pragma once

/**
 * Scoped: A simple RAII wrapper that "takes" ownership of a resource.
 * It calls .destroy() on the value when the scope ends.
 */
template <typename T>
struct Scoped {
  T value;

  // Take Constructor: Explicitly take ownership from a pointer
  // Usage: Scoped<String> scoped(&tempString);
  explicit Scoped(T* pToTake) : value(pToTake) {}

  // Destructor: The only place "magic" happens
  ~Scoped() {
    value.destroy();
  }

  // Ergonomics
  T* operator->() { return &value; }
  const T* operator->() const { return &value; }
  T& operator*() { return value; }
  const T& operator*() const { return value; }

  // Rigid Safety: Scopes cannot be copied
  Scoped(const Scoped&) = delete;
  Scoped& operator=(const Scoped&) = delete;
};
