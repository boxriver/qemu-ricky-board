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
#include "ricky_soc_usart_32.h"



#define TYPE_RICKY_SOC "ricky-soc"
typedef struct RickySocState RickySocState;
#define RICKY_SOC(obj) \
OBJECT_CHECK(RickySocState, (obj), TYPE_RICKY_SOC)



#define STM_NUM_USARTS 3
#define STM_NUM_SPIS 2

#define RICKY_SOC_USART_NUM 3


struct RickySocState {
    SysBusDevice parent_obj;

    ARMv7MState armv7m;

    RickySocUsartState usart[STM_NUM_USARTS];
    STM32F2XXSPIState spi[STM_NUM_SPIS];

    struct MemoryRegion sram;           //SRAM
    struct MemoryRegion flash;          //FLASH
    struct MemoryRegion boot_alias;    //BOOT

    Clock *sysclk;
    Clock *refclk;
};

enum {
    RICKY_MEM_BOOT,
    RICKY_MEM_FLASH,
    RICKY_MEM_SYSTEM_MEM,
    RICKY_MEM_OPTION_BYTES,
    RICKY_MEM_SRAM,
    RICKY_MEM_TIM2,
    RICKY_MEM_TIM3,
    RICKY_MEM_TIM4,
    RICKY_MEM_TIM6,
    RICKY_MEM_TIM7,
    RICKY_MEM_RTC,
    RICKY_MEM_WWDG,
    RICKY_MEM_IWDG,
    RICKY_MEM_SPI2,
    RICKY_MEM_USART2,
    RICKY_MEM_USART3,
    RICKY_MEM_I2C1,
    RICKY_MEM_I2C2,
    RICKY_MEM_USB,
    RICKY_MEM_USB_CAN_SRAM,
    RICKY_MEM_BXCAN,
    RICKY_MEM_BKP,
    RICKY_MEM_PWR,
    RICKY_MEM_DAC,
    RICKY_MEM_CEC,
    RICKY_MEM_AFIO,
    RICKY_MEM_EXTI,
    RICKY_MEM_GPIOA,
    RICKY_MEM_GPIOB,
    RICKY_MEM_GPIOC,
    RICKY_MEM_GPIOD,
    RICKY_MEM_GPIOE,
    RICKY_MEM_ADC1,
    RICKY_MEM_ADC2,
    RICKY_MEM_TIM1,
    RICKY_MEM_SPI1,
    RICKY_MEM_USART1,
    RICKY_MEM_DMA,
    RICKY_MEM_RCC,
    RICKY_MEM_FLASH_INT,
    RICKY_MEM_CRC,
    RICKY_MEM_NUM
};


static const MemMapEntry base_memmap[] = {
    [RICKY_MEM_BOOT] = { 0x00000000, 0x8000000 },
    [RICKY_MEM_FLASH] = { 0x08000000, FLASH_SIZE },
    [RICKY_MEM_SYSTEM_MEM] = { 0x1FFFF000, 0x800 },
    [RICKY_MEM_OPTION_BYTES] = { 0x1FFFF800, 0xF },
    [RICKY_MEM_SRAM] = {0x20000000, 0x20000},
    [RICKY_MEM_TIM2] = { 0x40000000, 0x400 },
    [RICKY_MEM_TIM3] = { 0x40000400, 0x400 },
    [RICKY_MEM_TIM4] = { 0x40000800, 0x400 },
    [RICKY_MEM_RTC] = { 0x40002800, 0x400 },
    [RICKY_MEM_WWDG] = { 0x40002C00, 0x400 },
    [RICKY_MEM_IWDG] = { 0x40003000, 0x400 },
    [RICKY_MEM_SPI2] = { 0x40003800, 0x400 },
    [RICKY_MEM_USART2] = { 0x40004400, 0x400 },
    [RICKY_MEM_USART3] = { 0x40004800, 0x400 },
    [RICKY_MEM_I2C1] = { 0x40005400, 0x400 },
    [RICKY_MEM_I2C2] = { 0x40005800, 0x400 },
    [RICKY_MEM_USB] = { 0x40005C00, 0x400 },
    [RICKY_MEM_USB_CAN_SRAM] = { 0x40006000, 0x400 },
    [RICKY_MEM_BXCAN] = { 0x40006400, 0x400 },
    [RICKY_MEM_BKP] = { 0x40006C00, 0x400 },
    [RICKY_MEM_PWR] = { 0x40007000, 0x400 },
    [RICKY_MEM_AFIO] = { 0x40010000, 0x400 },
    [RICKY_MEM_EXTI] = { 0x40010400, 0x400 },
    [RICKY_MEM_GPIOA] = { 0x40010800, 0x400 },
    [RICKY_MEM_GPIOB] = { 0x40010C00, 0x400 },
    [RICKY_MEM_GPIOC] = { 0x40011000, 0x400 },
    [RICKY_MEM_GPIOD] = { 0x40011400, 0x400 },
    [RICKY_MEM_GPIOE] = { 0x40011800, 0x400 },
    [RICKY_MEM_ADC1] = { 0x40012400, 0x400 },
    [RICKY_MEM_ADC2] = { 0x40012800, 0x400 },
    [RICKY_MEM_TIM1] = { 0x40012C00, 0x400 },
    [RICKY_MEM_SPI1] = { 0x40013000, 0x400 },
    [RICKY_MEM_USART1] = { 0x40013800, 0x400 },
    [RICKY_MEM_DMA] = { 0x40020000, 0x400 },
    [RICKY_MEM_RCC] = { 0x40021000, 0x400 },
    [RICKY_MEM_FLASH_INT] = { 0x40022000, 0x400 },
    [RICKY_MEM_CRC] = { 0x40023000, 0x400 },
};



#endif //QEMU_RICKY_SOC_H
