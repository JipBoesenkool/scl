#ifndef C_BUFFER_H
#define C_BUFFER_H

#include <cLib/cTypes.h>

// Universal memory interface
typedef struct cBuffer {
  uint8_t* pData = nullptr;
  size_t   size  = 0;
} Buffer_t;

// Internal interpretation for text-heavy modules
typedef struct cTextBuffer {
  char* pData = nullptr;
  size_t size  = 0;
} TextBuffer_t;

#endif // C_BUFFER_H
