#include "cLib/memory/strategy/MemStrategy.h"
#include "cLib/memory/strategy/NullStrategy.h"
#include "cLib/memory/strategy/DataStrategy.h"
#include "cLib/memory/strategy/ZoneStrategy.h"
#include "cLib/memory/strategy/StackStrategy.h"

/* ============================================================
 * Global dispatch table — indexed by eMemType_t.
 * TYPE_NONE=0, TYPE_DATA=1, TYPE_ZONE=2, TYPE_STACK=3
 * ============================================================ */
const MemStrategy_t kMemStrategies[4] = {
  /* [TYPE_NONE]  = */ kNullStrategy,
  /* [TYPE_DATA]  = */ kDataStrategy,
  /* [TYPE_ZONE]  = */ kZoneStrategy,
  /* [TYPE_STACK] = */ kStackStrategy,
};
