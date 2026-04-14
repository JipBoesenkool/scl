#include <cLib/string/cName.h>
#include <string.h>

bool name_equals(Name_t a, Name_t b) {
  // Fast path: Different hashes cannot be the same string
  if (a.hash != b.hash) return false;

  // Fast path: Same pointer means same string (interning win)
  if (a.name == b.name) return true;

  // Slow path: Different tables/memory, but same hash. 
  // We must check for collisions to be 100% Rigid.
  return strcmp(a.name, b.name) == 0;
}
