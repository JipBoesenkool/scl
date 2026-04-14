#ifndef LOG_H
#define LOG_H

#include <stdint.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

  void Log_Init();
  void Log_Add(const char* pFormat, ...);
  uint32_t Log_GetCount();
  const char* Log_GetLine(uint32_t index);

#ifdef __cplusplus
}
#endif

#endif // LOG_H
