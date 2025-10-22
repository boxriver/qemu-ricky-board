#ifndef QEMU_RICKY_BOARD_H
#define QEMU_RICKY_BOARD_H

#include "hw/boards.h"
#include "hw/sysbus.h"
#include "qom/object.h"
#include "ricky_soc.h"

#define TYPE_RICKY_BOARD MACHINE_TYPE_NAME("ricky")
typedef struct RickyBoardState RickyBoardState;
#define RICKY_BOARD(obj) \
OBJECT_CHECK(RickyBoardState, (obj), TYPE_RICKY_BOARD)

struct RickyBoardState {
    /*< private >*/
    MachineState parent;

    /*< public >*/
    RickySocState soc;

};

#endif //QEMU_RICKY_BOARD_H
