#include "qemu/osdep.h"
#include "hw/boards.h"
#include "qom/object.h"
#include "qemu/log-for-trace.h"
#include "hw/arm/boot.h"
#include "ricky_board_32.h"

#define SYSCLK_FRQ 24000000ULL

static void ricky_board_init(MachineState *machine)
{
    Error *error_fatal;
    Clock *sysclk;

    qemu_log("ricky board_init\r\n");

    RickyBoardState *board = RICKY_BOARD(machine);
    sysclk = clock_new(OBJECT(machine), "SYSCLK");
    clock_set_hz(sysclk, SYSCLK_FRQ);
    object_initialize_child(OBJECT(machine), "ricky_board.soc", &board->soc,
                            TYPE_RICKY_SOC);
    qdev_connect_clock_in(DEVICE(&board->soc), "sysclk", sysclk);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(&board->soc), &error_fatal);


    armv7m_load_kernel(board->soc.armv7m.cpu, machine->kernel_filename, 0x00, FLASH_SIZE);

    qemu_log("After ricky board_init\r\n");
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
