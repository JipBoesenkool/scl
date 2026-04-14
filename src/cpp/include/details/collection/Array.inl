// Array.inl — Template implementations for Array<T>
// Included at the end of Array.hpp — do NOT include this file directly.

// --- Lifecycle & Construction ---
template <typename T>
Array<T>::Array() {
  da_create(&this->impl, sizeof(T));
}

template <typename T>
Array<T>::Array(uint32_t capacity) {
  da_init(&this->impl, sizeof(T), capacity);
}

template <typename T>
void Array<T>::destroy() {
  da_destroy(&this->impl);
}

template <typename T>
Array<T>::Array(Array<T> *pOther) {
  if (pOther)
  {
    this->impl.pData    = pOther->impl.pData;
    this->impl.size     = pOther->impl.size;
    this->impl.capacity = pOther->impl.capacity;
    pOther->impl.pData    = nullptr;
    pOther->impl.size     = 0;
    pOther->impl.capacity = 0;
  } else {
    this->impl.pData    = nullptr;
    this->impl.size     = 0;
    this->impl.capacity = 0;
  }
}

// --- Element Access ---
template <typename T>
T *Array<T>::data() const {
  return static_cast<T *>(this->impl.pData);
}

template <typename T>
T& Array<T>::operator[](uint32_t index) {
  assert(index < this->impl.size && "Array index out of bounds!");
  return data()[index];
}

template <typename T>
const T& Array<T>::operator[](uint32_t index) const {
  assert(index < this->impl.size && "Array index out of bounds!");
  return data()[index];
}

template <typename T>
T *Array<T>::at(uint32_t index) {
  if (index >= this->impl.size) return nullptr;
  return data() + index;
}

template <typename T>
const T *Array<T>::at(uint32_t index) const {
  if (index >= this->impl.size) return nullptr;
  return data() + index;
}

// --- Iterators ---
template <typename T>
T *Array<T>::begin() {
  return data();
}

template <typename T>
const T *Array<T>::begin() const {
  return data();
}

template <typename T>
T *Array<T>::end() {
  return data() + this->impl.size;
}

template <typename T>
const T *Array<T>::end() const {
  return data() + this->impl.size;
}

// --- Inspection ---
template <typename T>
uint32_t Array<T>::size() const {
  return this->impl.size;
}

template <typename T>
uint32_t Array<T>::capacity() const {
  return this->impl.capacity;
}

template <typename T>
bool Array<T>::isEmpty() const {
  return this->impl.size == 0;
}

// --- Capacity Management ---
template <typename T>
bool Array<T>::reserve(uint32_t newCapacity) {
  return da_reserve(&this->impl, sizeof(T), newCapacity);
}

template <typename T>
bool Array<T>::shrink_to_fit() {
  return da_shrink_to_fit(&this->impl, sizeof(T));
}

template <typename T>
bool Array<T>::resize(uint32_t newSize) {
  return da_resize(&this->impl, sizeof(T), newSize);
}

// --- Modifiers ---
template <typename T>
T *Array<T>::append(const T &item) {
  return static_cast<T *>(da_append(&this->impl, sizeof(T), &item));
}

template <typename T>
T *Array<T>::append(const T &item, const Strategy_t *pStrategy) {
  return static_cast<T *>(da_append_ex(&this->impl, sizeof(T), &item, pStrategy));
}

template <typename T>
T *Array<T>::insert(uint32_t index, const T &item) {
  return static_cast<T *>(da_insert(&this->impl, sizeof(T), index, &item));
}

template <typename T>
T *Array<T>::insert(uint32_t index, const T &item, const Strategy_t *pStrategy) {
  return static_cast<T *>(da_insert_ex(&this->impl, sizeof(T), index, &item, pStrategy));
}

template <typename T>
void Array<T>::pop_back() {
  da_pop_back(&this->impl);
}

template <typename T>
void Array<T>::pop_back(uint32_t count) {
  da_pop_back_n(&this->impl, count);
}

template <typename T>
void Array<T>::erase(uint32_t index) {
  da_erase(&this->impl, sizeof(T), index);
}

template <typename T>
void Array<T>::clear() {
  da_clear(&this->impl);
}

template <typename T>
void Array<T>::fill(const T &item) {
  da_fill(&this->impl, sizeof(T), &item);
}

template <typename T>
void Array<T>::assign(uint32_t count, const T &item) {
  da_assign(&this->impl, sizeof(T), count, &item);
}

template <typename T>
void Array<T>::swap(Array<T> *pOther) {
  if (!pOther) return;
  da_swap(&this->impl, &pOther->impl);
}
