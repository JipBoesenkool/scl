#ifndef C_COMPONENT_INFO_H
#define C_COMPONENT_INFO_H

#include <cLib/cTypes.h>
#include <cLib/string/cName.h>

#define INVALID_COMP_ID   0x00000000u
#define INVALID_COMP_SIZE 0x00000000u

typedef struct cComponentID {
    uint32_t value = INVALID_COMP_ID;
} ComponentID_t;

typedef struct cComponentInfo {
    ComponentID_t id;
    Name_t        name = INVALID_NAME;
    size_t        size = INVALID_COMP_SIZE;
} ComponentInfo_t;

// Static library null object
static const ComponentInfo_t INVALID_COMPONENT_INFO = {
    { INVALID_COMP_ID },
    INVALID_NAME,
    INVALID_COMP_SIZE
};

#endif
