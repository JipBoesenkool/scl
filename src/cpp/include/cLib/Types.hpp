#ifndef TYPES_HPP
#define TYPES_HPP

// 1. Core ABI Layer (PODs and C-compatible structs)
#include <cLib/cTypes.h>
#include <cLib/string/cString.h>
#include <cLib/string/cName.h>
#include <cLib/collection/cRange.h>
#include <cLib/collection/cSpan.h>
#include <cLib/collection/cArray.h>
#include <cLib/ecs/cComponentInfo.h>

// 2. C++ Ergonomic Layer (Wrappers and Helpers)
#include <cLib/string/String.hpp>
#include <cLib/collection/Range.hpp>
#include <cLib/collection/Span.hpp>
#include <cLib/collection/Array.hpp>

// Future wrappers like Name.hpp or Component.hpp go here

#endif // TYPES_HPP
