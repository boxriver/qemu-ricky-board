#ifndef QEMU_RICKY_SOC_H
#define QEMU_RICKY_SOC_H

#include "hw/boards.h"
#include "hw/sysbus.h"
#include "qom/object.h"

#define TYPE_RICKY_SOC "ricky-soc"
typedef struct RickySocState RickySocState;
#define RICKY_SOC(obj) \
OBJECT_CHECK(RickySocState, (obj), TYPE_RICKY_SOC)

struct RickySocState {
    SysBusDevice parent_obj;
};

#endif //QEMU_RICKY_SOC_H
