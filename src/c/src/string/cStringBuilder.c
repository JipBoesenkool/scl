#include <cLib/string/cStringBuilder.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>

// --- Internal Helper ---

static void sb_internal_append(StringBuilder_t* sb, const char* src, size_t len) {
  if (!sb || !sb->buffer.pData || !src || len == 0) return;

  // Space left before the mandatory null terminator
  size_t remaining = (sb->capacity > sb->length) ? (sb->capacity - sb->length - 1) : 0;
  
  // Rigid safety: Ensure we don't truncate silently in debug
  assert(len <= remaining && "StringBuilder buffer overflow!");

  size_t toCopy = (len < remaining) ? len : remaining;
  if (toCopy > 0) {
    memcpy(sb->buffer.pData + sb->length, src, toCopy);
    sb->length += toCopy;
    sb->buffer.pData[sb->length] = '\0';
  }
}

// --- Lifecycle ---
void sb_init(StringBuilder_t* sb, Buffer_t rawBuffer) {
  assert(rawBuffer.pData != nullptr && "StringBuilder initialized with NULL buffer");
  assert(rawBuffer.size > 0 && "StringBuilder capacity must be > 0");

  sb->buffer.pData = (char*)rawBuffer.pData;
  sb->buffer.size  = rawBuffer.size;
  sb->capacity     = rawBuffer.size;
  sb->length       = 0;

  if (sb->capacity > 0) {
    sb->buffer.pData[0] = '\0';
  }
}

void sb_clear(StringBuilder_t* sb) {
  if (!sb) return;
  sb->length = 0;
  if (sb->buffer.pData) {
    sb->buffer.pData[0] = '\0';
  }
}

// --- Appending ---
void sb_append_sv(StringBuilder_t* sb, StringView_t view) {
  sb_internal_append(sb, view.data, view.length);
}

void sb_append_char(StringBuilder_t* sb, char ch) {
  sb_internal_append(sb, &ch, 1);
}

void sb_append_cstr(StringBuilder_t* sb, const char* str) {
  if (!str) return;
  sb_internal_append(sb, str, strlen(str));
}

void sb_append_string(StringBuilder_t* sb, const String_t* s) {
  if (!s) return;
  sb_append_sv(sb, string_to_sv(s));
}

void sb_append_i32(StringBuilder_t* sb, int32_t value) {
  char tmp[16];
  int n = snprintf(tmp, sizeof(tmp), "%d", value);
  if (n > 0) sb_internal_append(sb, tmp, (size_t)n);
}

void sb_append_u32(StringBuilder_t* sb, uint32_t value) {
  char tmp[16];
  int n = snprintf(tmp, sizeof(tmp), "%u", value);
  if (n > 0) sb_internal_append(sb, tmp, (size_t)n);
}

void sb_append_f32(StringBuilder_t* sb, float value) {
  char tmp[32];
  int n = snprintf(tmp, sizeof(tmp), "%g", (double)value);
  if (n > 0) sb_internal_append(sb, tmp, (size_t)n);
}

void sb_append_bool(StringBuilder_t* sb, bool value) {
  if (value) sb_internal_append(sb, "true",  4);
  else       sb_internal_append(sb, "false", 5);
}

void sb_append_format(StringBuilder_t* sb, const char* format, ...) {
  if (!sb || !sb->buffer.pData || !format) return;

  size_t remaining = (sb->capacity > sb->length) ? (sb->capacity - sb->length - 1) : 0;
  if (remaining == 0) {
    assert(false && "StringBuilder buffer overflow on format start!");
    return;
  }

  va_list args;
  va_start(args, format);
  // vsnprintf takes the full remaining buffer including null terminator
  int written = vsnprintf(sb->buffer.pData + sb->length, remaining + 1, format, args);
  va_end(args);

  if (written > 0) {
    size_t actual = (size_t)written;
    assert(actual <= remaining && "StringBuilder buffer overflow during format!");
    
    size_t added = (actual < remaining) ? actual : remaining;
    sb->length += added;
    // vsnprintf handles null termination, but we keep it explicit for safety
    sb->buffer.pData[sb->length] = '\0';
  }
}

// --- Conversion ---

StringView_t sb_to_sv(const StringBuilder_t* sb) {
  if (!sb || sb->length == 0) return INVALID_SV;
  return StringView_t{ .data = sb->buffer.pData, .length = sb->length };
}

String_t sb_to_string(const StringBuilder_t* sb) {
  if (!sb || sb->length == 0) return INVALID_STRING;
  // Optimization: Use the known length from the view to avoid strlen
  return string_create_from_sv(sb_to_sv(sb));
}

String_t sb_to_string_alloc(const StringBuilder_t* sb, Allocator_t* pAlloc) {
  if (!sb || sb->length == 0) return INVALID_STRING;
  if (!pAlloc) return string_create_from_sv(sb_to_sv(sb));
  return string_create_from_sv_alloc(sb_to_sv(sb), pAlloc);
}
