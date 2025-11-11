#include <string.h>
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/module.h"
#include "qemu/log-for-trace.h"
#include "qom/object.h"
#include "ricky_soc.h"


/* stm32f100_soc implementation is derived from stm32f205_soc */

static const uint32_t usart_addr[STM_NUM_USARTS] = { 0x40013800, 0x40004400,
    0x40004800 };
static const uint32_t spi_addr[STM_NUM_SPIS] = { 0x40013000, 0x40003800 };

static const int usart_irq[STM_NUM_USARTS] = {37, 38, 39};
static const int spi_irq[STM_NUM_SPIS] = {35, 36};


static void ricky_soc_initfn(Object *obj)
{
    qemu_log("ricky soc_instance_init\r\n");

    int i;

    RickySocState *s = RICKY_SOC(obj);
    object_initialize_child(obj, "armv7m", &s->armv7m, TYPE_ARMV7M);

    for (i = 0; i < STM_NUM_USARTS; i++) {
        object_initialize_child(obj, "usart[*]", &s->usart[i], TYPE_STM32F2XX_USART);
    }

    for (i = 0; i < STM_NUM_SPIS; i++) {
        object_initialize_child(obj, "spi[*]", &s->spi[i], TYPE_STM32F2XX_SPI);
    }

    s->sysclk = qdev_init_clock_in(DEVICE(s), "sysclk", NULL, NULL, 0);
    s->refclk = qdev_init_clock_in(DEVICE(s), "refclk", NULL, NULL, 0);

}

static void ricky_soc_realize(DeviceState *dev_soc, Error **errp)
{
    qemu_log("ricky soc_realize\r\n");

    RickySocState *s = RICKY_SOC(dev_soc);
    DeviceState *dev, *armv7m;
    SysBusDevice *busdev;
    int i;

    struct MemoryRegion *system_memory = get_system_memory();

    /*
     * We use s->refclk internally and only define it with qdev_init_clock_in()
     * so it is correctly parented and not leaked on an init/deinit; it is not
     * intended as an externally exposed clock.
     */
     if (clock_has_source(s->refclk)) {
        error_setg(errp, "refclk clock must not be wired up by the board code");
        return;
     }

    if (!clock_has_source(s->sysclk)) {
        error_setg(errp, "sysclk clock must be wired up by the board code");
        return;
    }

    /*
     * TODO: ideally we should model the SoC RCC and its ability to
     * change the sysclk frequency and define different sysclk sources.
     */

    /* The refclk always runs at frequency HCLK / 8 */
    clock_set_mul_div(s->refclk, 8, 1);
    clock_set_source(s->refclk, s->sysclk);

    /*
     * Init flash region
     * Flash starts at 0x08000000 and then is aliased to boot memory at 0x0
     */
    memory_region_init_rom(&s->flash, OBJECT(dev_soc), "STM32F100.flash", FLASH_SIZE, &error_fatal);
    // 此处需要有Flash alias，是因为在Cortex-M SoC芯片中，需要满足以下要求
    // 1.CPU上电后必须从得治 0x00000000开始执行
    // 2.该地址必须包含中断向量表和reset handler
    // 3.Flash通常在0x80000000，因此必须在启动时将其映射到0x00000000
    memory_region_init_alias(&s->flash_alias, OBJECT(dev_soc), "STM32F100.flash.alias", &s->flash, 0, FLASH_SIZE);
    memory_region_add_subregion(system_memory, FLASH_BASE_ADDRESS, &s->flash);
    memory_region_add_subregion(system_memory, 0, &s->flash_alias);

    /* Init SRAM region*/
    memory_region_init_ram(&s->sram, NULL, "STM32F100.sram", SRAM_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, SRAM_BASE_ADDRESS, &s->sram);

    /* Init ARMv7m */
    armv7m = DEVICE(&s->armv7m);
    //表示中断优先级的位数，用于控制可配置的中断优先级等级数量,可以设置4位，表示2⁴ = 16个中断优先等级
    //ARM Cortex-M 系列处理器中（如 Cortex-M0/M3/M4/M7），中断优先级是可配置的
    qdev_prop_set_uint32(armv7m, "num-irq", 61);
    qdev_prop_set_uint8(armv7m, "num-prio-bits", 4);
    //配置使用的CPU核心
    qdev_prop_set_string(armv7m, "cpu-type", ARM_CPU_TYPE_NAME("cortex-m3"));
    //启动Bit-Band特性，以将某些内存区域中的每一位映射到一个独立的地址，实现“位操作地址化”，常用与GPIO、位操作等优化场景
    qdev_prop_set_bit(armv7m, "enable-bitband", true);
    qdev_connect_clock_in(armv7m, "cpuclk", s->sysclk);
    qdev_connect_clock_in(armv7m, "refclk", s->refclk);
    //将系统内存连接到 armv7m 设备的 "memory" 属性上，表示这个 CPU 所访问的主存空间就是系统模拟的内存
    object_property_set_link(OBJECT(&s->armv7m), "memory", OBJECT(get_system_memory()), &error_abort);
    //QEMU 用来将设备“实例化”并连接到系统总线
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->armv7m), errp)) {
        return;
    }

    /* Attach UART (uses USART registers) and USART controllers */
    for (i = 0; i < STM_NUM_USARTS; i++) {
        dev = DEVICE(&(s->usart[i]));
        qdev_prop_set_chr(dev, "chardev", serial_hd(i));
        if (!sysbus_realize(SYS_BUS_DEVICE(&s->usart[i]), errp)) {
            return;
        }
        busdev = SYS_BUS_DEVICE(dev);
        //将设备地址映射到模拟系统的内存地址空间
        sysbus_mmio_map(busdev, 0, usart_addr[i]);
        //将设备的中断号链接到处理器的中断控制器（NVIC）
        sysbus_connect_irq(busdev, 0, qdev_get_gpio_in(armv7m, usart_irq[i]));
    }

    /* SPI 1 and 2 */
    for (i = 0; i < STM_NUM_SPIS; i++) {
        dev = DEVICE(&(s->spi[i]));
        if (!sysbus_realize(SYS_BUS_DEVICE(&s->spi[i]), errp)) {
            return;
        }
        busdev = SYS_BUS_DEVICE(dev);
        sysbus_mmio_map(busdev, 0, spi_addr[i]);
        sysbus_connect_irq(busdev, 0, qdev_get_gpio_in(armv7m, spi_irq[i]));
    }

    create_unimplemented_device("timer[2]",  0x40000000, 0x400);
    create_unimplemented_device("timer[3]",  0x40000400, 0x400);
    create_unimplemented_device("timer[4]",  0x40000800, 0x400);
    create_unimplemented_device("timer[6]",  0x40001000, 0x400);
    create_unimplemented_device("timer[7]",  0x40001400, 0x400);
    create_unimplemented_device("RTC",       0x40002800, 0x400);
    create_unimplemented_device("WWDG",      0x40002C00, 0x400);
    create_unimplemented_device("IWDG",      0x40003000, 0x400);
    create_unimplemented_device("I2C1",      0x40005400, 0x400);
    create_unimplemented_device("I2C2",      0x40005800, 0x400);
    create_unimplemented_device("BKP",       0x40006C00, 0x400);
    create_unimplemented_device("PWR",       0x40007000, 0x400);
    create_unimplemented_device("DAC",       0x40007400, 0x400);
    create_unimplemented_device("CEC",       0x40007800, 0x400);
    create_unimplemented_device("AFIO",      0x40010000, 0x400);
    create_unimplemented_device("EXTI",      0x40010400, 0x400);
    create_unimplemented_device("GPIOA",     0x40010800, 0x400);
    create_unimplemented_device("GPIOB",     0x40010C00, 0x400);
    create_unimplemented_device("GPIOC",     0x40011000, 0x400);
    create_unimplemented_device("GPIOD",     0x40011400, 0x400);
    create_unimplemented_device("GPIOE",     0x40011800, 0x400);
    create_unimplemented_device("ADC1",      0x40012400, 0x400);
    create_unimplemented_device("timer[1]",  0x40012C00, 0x400);
    create_unimplemented_device("timer[15]", 0x40014000, 0x400);
    create_unimplemented_device("timer[16]", 0x40014400, 0x400);
    create_unimplemented_device("timer[17]", 0x40014800, 0x400);
    create_unimplemented_device("DMA",       0x40020000, 0x400);
    create_unimplemented_device("RCC",       0x40021000, 0x400);
    create_unimplemented_device("Flash Int", 0x40022000, 0x400);
    create_unimplemented_device("CRC",       0x40023000, 0x400);
}


static void ricky_soc_class_init(ObjectClass *klass, void *data)
{
    qemu_log("ricky soc_class_init\r\n");
    DeviceClass *dc = DEVICE_CLASS(klass);
    dc->realize = ricky_soc_realize;
    /* No vmstate or reset required: device has no internal state */
}

static const TypeInfo ricky_soc_info = {
        .name          = TYPE_RICKY_SOC,
        .parent        = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(RickySocState),
        .instance_init = ricky_soc_initfn,
        .class_init    = ricky_soc_class_init,
};

static void ricky_soc_types(void)
{
    type_register_static(&ricky_soc_info);
}

type_init(ricky_soc_types)
