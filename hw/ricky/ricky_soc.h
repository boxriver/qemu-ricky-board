#ifndef QEMU_RICKY_SOC_H
#define QEMU_RICKY_SOC_H


#include "hw/boards.h"
#include "hw/sysbus.h"
#include "qom/object.h"

#include "exec/memory.h"
#include "hw/arm/armv7m.h"
#include "hw/boards.h"
#include "hw/char/stm32f2xx_usart.h"
#include "hw/ssi/stm32f2xx_spi.h"
#include "hw/sysbus.h"
#include "qemu/typedefs.h"
#include "qom/object.h"


#include "cpu-qom.h"
#include "exec/address-spaces.h"
#include "exec/memory.h"
#include "hw/arm/stm32f100_soc.h"
#include "hw/char/stm32f2xx_usart.h"
#include "hw/clock.h"
#include "hw/qdev-clock.h"
#include "hw/qdev-properties.h"

#include "hw/qdev-core.h"
#include "hw/sysbus.h"


#include "hw/misc/unimp.h"
#include "sysemu/sysemu.h"

#define TYPE_RICKY_SOC "ricky-soc"
typedef struct RickySocState RickySocState;
#define RICKY_SOC(obj) \
OBJECT_CHECK(RickySocState, (obj), TYPE_RICKY_SOC)



#define STM_NUM_USARTS 3
#define STM_NUM_SPIS 2



struct RickySocState {
    SysBusDevice parent_obj;

    ARMv7MState armv7m;

    STM32F2XXUsartState usart[STM_NUM_USARTS];
    STM32F2XXSPIState spi[STM_NUM_SPIS];

    struct MemoryRegion sram;
    struct MemoryRegion flash;
    struct MemoryRegion flash_alias;

    Clock *sysclk;
    Clock *refclk;
};

#endif //QEMU_RICKY_SOC_H
