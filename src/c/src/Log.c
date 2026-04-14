#include "cLib/Log.h"
#include <stdio.h>
#include <string.h>

#define MAX_LOG_MESSAGES 64
#define LOG_LINE_LEN 256

typedef struct cLog {
  char lines[MAX_LOG_MESSAGES][LOG_LINE_LEN];
  uint32_t count;
} Log_t;

/* Internal state hidden from other files */
static Log_t s_log;

void Log_Init(void) {
  memset(&s_log, 0, sizeof(Log_t));
  Log_Add("[INIT] Logging System Online.");
}

void Log_Add(const char* pFormat, ...) {
  if (!pFormat) return;

  va_list args;
  
  /* 1. Log to internal buffer */
  va_start(args, pFormat);
  vsnprintf(s_log.lines[s_log.count % MAX_LOG_MESSAGES], LOG_LINE_LEN, pFormat, args);
  va_end(args);

  /* 2. Log to stdout - MUST use vprintf for va_list */
  va_start(args, pFormat);
  vprintf(pFormat, args); 
  printf("\n"); // Add a newline since your formats don't always have them
  va_end(args);

  s_log.count++;
}

uint32_t Log_GetCount()
{
  return s_log.count;
}

const char* Log_GetLine(uint32_t index) {
  if (index >= s_log.count || (s_log.count > MAX_LOG_MESSAGES && index < s_log.count - MAX_LOG_MESSAGES)) {
    return NULL;
  }
  return s_log.lines[index % MAX_LOG_MESSAGES];
}
