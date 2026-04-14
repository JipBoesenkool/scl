#ifndef HANDLE_H
#define HANDLE_H

#include "cLib/cTypes.h"

#define kINVALID_POINTER 0XDEADBEEFDEADBEEF
#define kCLOSED_POINTER  0xDECEA5EDDECEA5ED
#define kINVALID_HANDLE  0xDEADBEEFu
#define kCLOSED_HANDLE   0xDECEA5EDu

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cHandle {
  uint32_t  id;
  uint32_t  version;
  void*     pData;
} Handle_t; // 16 bytes

#ifdef __cplusplus
} // extern "C"
#endif

/* Aggregate initializers work in both C99 and C++11 without designated fields. */
static const Handle_t kInvalidHandle = { kINVALID_HANDLE, 0u, NULL };
static const Handle_t kClosedHandle  = { kCLOSED_HANDLE,  0u, NULL };

#endif // HANDLE_H
