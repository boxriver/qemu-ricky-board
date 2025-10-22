#include "qemu/osdep.h"
#include "hw/boards.h"
#include "qom/object.h"
#include "qemu/log-for-trace.h"
#include "ricky_board.h"


static void ricky_board_init(MachineState *machine)
{
    Error *error_fatal;

    qemu_log("ricky board_init\r\n");

    RickyBoardState *board = RICKY_BOARD(machine);
    object_initialize_child(OBJECT(machine), "ricky_board.soc", &board->soc,
                            TYPE_RICKY_SOC);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(&board->soc), &error_fatal);
}


static void ricky_board_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    mc->init = ricky_board_init;
    mc->desc = "Ricky Star Board";
}

static const TypeInfo ricky_board_typeinfo = {
    .name       = TYPE_RICKY_BOARD,
    .parent     = TYPE_MACHINE,
    .class_init = ricky_board_class_init,
    .instance_size = sizeof(RickyBoardState),
};

static void ricky_board_init_register_types(void)
{
    type_register_static(&ricky_board_typeinfo);
}

type_init(ricky_board_init_register_types)
