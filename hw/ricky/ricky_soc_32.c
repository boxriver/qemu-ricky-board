#include <string.h>
// #include "hw/qdev-core.h"
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/module.h"
#include "qemu/log-for-trace.h"
#include "qom/object.h"
#include "ricky_soc_32.h"



/* stm32f100_soc implementation is derived from stm32f205_soc */
static const uint32_t usart_addr[RICKY_SOC_USART_NUM] = { 0x40013800, 0x40004400,
    0x40004800 };
static const int usart_irq[RICKY_SOC_USART_NUM] = {37, 38, 39};



static const uint32_t spi_addr[STM_NUM_SPIS] = { 0x40013000, 0x40003800 };
static const int spi_irq[STM_NUM_SPIS] = {35, 36};


static void ricky_soc_usart_create(RickySocUsartState *usart, int serial_id, qemu_irq irq, hwaddr base, Error **errp)
{
    DeviceState *dev;
    SysBusDevice *busdev;

    dev = DEVICE(usart);
    qdev_prop_set_chr(dev, "chardev", serial_hd(serial_id));
    if (!sysbus_realize(SYS_BUS_DEVICE(usart), errp)) {
        return;
    }
    busdev = SYS_BUS_DEVICE(dev);
    sysbus_mmio_map(busdev, 0, base);
    sysbus_connect_irq(busdev, 0, irq);
}


static void ricky_soc_initfn(Object *obj)
{
    qemu_log("ricky soc_instance_init\r\n");

    int i;

    RickySocState *s = RICKY_SOC(obj);
    object_initialize_child(obj, "armv7m", &s->armv7m, TYPE_ARMV7M);
   

    for (i = 0; i < RICKY_SOC_USART_NUM; i++) {
        object_initialize_child(obj, "usart[*]", &s->usart[i], TYPE_RICKY_SOC_USART);
        // qdev_realize(DEVICE(&s->usart[i]), NULL, &error_fatal);
    }

    // for (i = 0; i < STM_NUM_SPIS; i++) {
    //     object_initialize_child(obj, "spi[*]", &s->spi[i], TYPE_STM32F2XX_SPI);
    //     // qdev_realize(DEVICE(&s->spi[i]), NULL, &error_fatal);
    // }

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

    qemu_log("before get_system_memory\n");

    struct MemoryRegion *system_memory = get_system_memory();

    /*
     * We use s->refclk internally and only define it with qdev_init_clock_in()
     * so it is correctly parented and not leaked on an init/deinit; it is not
     * intended as an externally exposed clock.
     */
    qemu_log("before clock_has_source(s->refclk)\n");
    if (clock_has_source(s->refclk)) {
        error_setg(errp, "refclk clock must not be wired up by the board code");
        return;
    }
    
    qemu_log("before clock_has_source(s->sysclk)\n");
    // //! 此处有问题，会导致断言realized为false(在Board板卡中设置clk后正常)
    if (!clock_has_source(s->sysclk)) {
        error_setg(errp, "sysclk clock must be wired up by the board code");
        return;
    }

    /*
     * TODO: ideally we should model the SoC RCC and its ability to
     * change the sysclk frequency and define different sysclk sources.
     */
    qemu_log("before clock_set_mul_div\n");
    /* The refclk always runs at frequency HCLK / 8 */
    clock_set_mul_div(s->refclk, 8, 1);
    clock_set_source(s->refclk, s->sysclk);


    /*
     * Init flash region
     * Flash starts at 0x08000000 and then is aliased to boot memory at 0x0
     */
    qemu_log("before memory_region_init_rom\n");
    memory_region_init_rom(&s->flash, OBJECT(dev_soc), "ricky-soc.flash", base_memmap[RICKY_MEM_FLASH].size, &error_fatal);
    memory_region_add_subregion(system_memory, base_memmap[RICKY_MEM_FLASH].base, &s->flash);

    qemu_log("before memory_region_init_ram\n");
    memory_region_init_ram(&s->sram, NULL, "ricky-soc.sram", base_memmap[RICKY_MEM_SRAM].size, &error_fatal);
    memory_region_add_subregion(system_memory, base_memmap[RICKY_MEM_SRAM].base, &s->sram);

    // 此处需要有Flash alias，是因为在Cortex-M SoC芯片中，需要满足以下要求
    // 1.CPU上电后必须从得治 0x00000000开始执行
    // 2.该地址必须包含中断向量表和reset handler
    // 3.Flash通常在0x80000000，因此必须在启动时将其映射到0x00000000
    qemu_log("before memory_region_init_alias\n");
    memory_region_init_alias(&s->boot_alias, OBJECT(dev_soc), "ricky-soc.boot", &s->flash, 0, base_memmap[RICKY_MEM_FLASH].size);
    memory_region_add_subregion(system_memory, base_memmap[RICKY_MEM_BOOT].base, &s->boot_alias);



    qemu_log("before Init ARMv7m\n");
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
    qemu_log("before armv7m realize\n");
    //QEMU 用来将设备“实例化”并连接到系统总线，注意，memoey等属性需要在实例化之前设置
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->armv7m), errp)) {
        return;
    }

    /* Attach UART (uses USART registers) and USART controllers */
    for (i = 0; i < RICKY_SOC_USART_NUM; i++) {
        ricky_soc_usart_create(&(s->usart[i]), i, qdev_get_gpio_in(armv7m, usart_irq[i]), usart_addr[i], errp);
    }


    qemu_log("before create_unimplemented_device\n");
    create_unimplemented_device("timer[2]", base_memmap[RICKY_MEM_TIM2].base, base_memmap[RICKY_MEM_TIM2].size);
    create_unimplemented_device("timer[3]", base_memmap[RICKY_MEM_TIM3].base, base_memmap[RICKY_MEM_TIM3].size);
    create_unimplemented_device("timer[4]", base_memmap[RICKY_MEM_TIM4].base, base_memmap[RICKY_MEM_TIM4].size);
    create_unimplemented_device("rtc", base_memmap[RICKY_MEM_RTC].base, base_memmap[RICKY_MEM_RTC].size);
    create_unimplemented_device("wwdg", base_memmap[RICKY_MEM_WWDG].base, base_memmap[RICKY_MEM_WWDG].size);
    create_unimplemented_device("iwdg", base_memmap[RICKY_MEM_IWDG].base, base_memmap[RICKY_MEM_IWDG].size);
    create_unimplemented_device("spi[2]", base_memmap[RICKY_MEM_SPI2].base, base_memmap[RICKY_MEM_SPI2].size);
    create_unimplemented_device("usart[2]", base_memmap[RICKY_MEM_USART2].base, base_memmap[RICKY_MEM_USART2].size);
    create_unimplemented_device("usart[3]", base_memmap[RICKY_MEM_USART3].base, base_memmap[RICKY_MEM_USART3].size);
    create_unimplemented_device("i2c[1]", base_memmap[RICKY_MEM_I2C1].base, base_memmap[RICKY_MEM_I2C1].size);
    create_unimplemented_device("i2c[2]", base_memmap[RICKY_MEM_I2C2].base, base_memmap[RICKY_MEM_I2C2].size);
    create_unimplemented_device("usb", base_memmap[RICKY_MEM_USB].base, base_memmap[RICKY_MEM_USB].size);
    create_unimplemented_device("usb-can-sram", base_memmap[RICKY_MEM_USB_CAN_SRAM].base, base_memmap[RICKY_MEM_USB_CAN_SRAM].size);
    create_unimplemented_device("bxcan", base_memmap[RICKY_MEM_BXCAN].base, base_memmap[RICKY_MEM_BXCAN].size);
    create_unimplemented_device("bkp", base_memmap[RICKY_MEM_BKP].base, base_memmap[RICKY_MEM_BKP].size);
    create_unimplemented_device("pwr", base_memmap[RICKY_MEM_PWR].base, base_memmap[RICKY_MEM_PWR].size);
    create_unimplemented_device("afio", base_memmap[RICKY_MEM_AFIO].base, base_memmap[RICKY_MEM_AFIO].size);
    create_unimplemented_device("exti", base_memmap[RICKY_MEM_EXTI].base, base_memmap[RICKY_MEM_EXTI].size);
    create_unimplemented_device("gpio[a]", base_memmap[RICKY_MEM_GPIOA].base, base_memmap[RICKY_MEM_GPIOA].size);
    create_unimplemented_device("gpio[b]", base_memmap[RICKY_MEM_GPIOB].base, base_memmap[RICKY_MEM_GPIOB].size);
    create_unimplemented_device("gpio[c]", base_memmap[RICKY_MEM_GPIOC].base, base_memmap[RICKY_MEM_GPIOC].size);
    create_unimplemented_device("gpio[d]", base_memmap[RICKY_MEM_GPIOD].base, base_memmap[RICKY_MEM_GPIOD].size);
    create_unimplemented_device("gpio[e]", base_memmap[RICKY_MEM_GPIOE].base, base_memmap[RICKY_MEM_GPIOE].size);
    create_unimplemented_device("adc[1]", base_memmap[RICKY_MEM_ADC1].base, base_memmap[RICKY_MEM_ADC1].size);
    create_unimplemented_device("adc[2]", base_memmap[RICKY_MEM_ADC2].base, base_memmap[RICKY_MEM_ADC2].size);
    create_unimplemented_device("timer[1]", base_memmap[RICKY_MEM_TIM1].base, base_memmap[RICKY_MEM_TIM1].size);
    create_unimplemented_device("spi[1]", base_memmap[RICKY_MEM_SPI1].base, base_memmap[RICKY_MEM_SPI1].size);
    create_unimplemented_device("usart[1]", base_memmap[RICKY_MEM_USART1].base, base_memmap[RICKY_MEM_USART1].size);
    create_unimplemented_device("dma", base_memmap[RICKY_MEM_DMA].base, base_memmap[RICKY_MEM_DMA].size);
    create_unimplemented_device("rcc", base_memmap[RICKY_MEM_RCC].base, base_memmap[RICKY_MEM_RCC].size);
    create_unimplemented_device("flash-int", base_memmap[RICKY_MEM_FLASH_INT].base, base_memmap[RICKY_MEM_FLASH_INT].size);
    create_unimplemented_device("crc", base_memmap[RICKY_MEM_CRC].base, base_memmap[RICKY_MEM_CRC].size);
    qemu_log("After create_unimplemented_device\n");


    // qemu_log("before Init ARMv7m\n");
    // /* Init ARMv7m */
    // armv7m = DEVICE(&s->armv7m);
    // //表示中断优先级的位数，用于控制可配置的中断优先级等级数量,可以设置4位，表示2⁴ = 16个中断优先等级
    // //ARM Cortex-M 系列处理器中（如 Cortex-M0/M3/M4/M7），中断优先级是可配置的
    // qdev_prop_set_uint32(armv7m, "num-irq", 61);
    // qdev_prop_set_uint8(armv7m, "num-prio-bits", 4);
    // //配置使用的CPU核心
    // qdev_prop_set_string(armv7m, "cpu-type", ARM_CPU_TYPE_NAME("cortex-m3"));
    // //启动Bit-Band特性，以将某些内存区域中的每一位映射到一个独立的地址，实现“位操作地址化”，常用与GPIO、位操作等优化场景
    // qdev_prop_set_bit(armv7m, "enable-bitband", true);
    // qdev_connect_clock_in(armv7m, "cpuclk", s->sysclk);
    // qdev_connect_clock_in(armv7m, "refclk", s->refclk);
    // //将系统内存连接到 armv7m 设备的 "memory" 属性上，表示这个 CPU 所访问的主存空间就是系统模拟的内存
    // object_property_set_link(OBJECT(&s->armv7m), "memory", OBJECT(get_system_memory()), &error_abort);
    // qemu_log("before armv7m realize\n");
    // //QEMU 用来将设备“实例化”并连接到系统总线
    // if (!sysbus_realize(SYS_BUS_DEVICE(&s->armv7m), errp)) {
    //     return;
    // }

    // /* Attach UART (uses USART registers) and USART controllers */
    // qemu_log("before usart realize\n");
    // for (i = 0; i < STM_NUM_USARTS; i++) {
    //     dev = DEVICE(&(s->usart[i]));
    //     qdev_prop_set_chr(dev, "chardev", serial_hd(i));
    //     if (!sysbus_realize(SYS_BUS_DEVICE(&s->usart[i]), errp)) {
    //         return;
    //     }
    //     busdev = SYS_BUS_DEVICE(dev);
    //     //将设备地址映射到模拟系统的内存地址空间
    //     sysbus_mmio_map(busdev, 0, usart_addr[i]);
    //     //将设备的中断号链接到处理器的中断控制器（NVIC）
    //     sysbus_connect_irq(busdev, 0, qdev_get_gpio_in(armv7m, usart_irq[i]));
    // }

    // /* SPI 1 and 2 */
    // qemu_log("before spi realize\n");
    // for (i = 0; i < STM_NUM_SPIS; i++) {
    //     dev = DEVICE(&(s->spi[i]));
    //     if (!sysbus_realize(SYS_BUS_DEVICE(&s->spi[i]), errp)) {
    //         return;
    //     }
    //     busdev = SYS_BUS_DEVICE(dev);
    //     sysbus_mmio_map(busdev, 0, spi_addr[i]);
    //     sysbus_connect_irq(busdev, 0, qdev_get_gpio_in(armv7m, spi_irq[i]));
    // }

    // create_unimplemented_device("timer[2]",  0x40000000, 0x400);
    // create_unimplemented_device("timer[3]",  0x40000400, 0x400);
    // create_unimplemented_device("timer[4]",  0x40000800, 0x400);
    // create_unimplemented_device("timer[6]",  0x40001000, 0x400);
    // create_unimplemented_device("timer[7]",  0x40001400, 0x400);
    // create_unimplemented_device("RTC",       0x40002800, 0x400);
    // create_unimplemented_device("WWDG",      0x40002C00, 0x400);
    // create_unimplemented_device("IWDG",      0x40003000, 0x400);
    // create_unimplemented_device("I2C1",      0x40005400, 0x400);
    // create_unimplemented_device("I2C2",      0x40005800, 0x400);
    // create_unimplemented_device("BKP",       0x40006C00, 0x400);
    // create_unimplemented_device("PWR",       0x40007000, 0x400);
    // create_unimplemented_device("DAC",       0x40007400, 0x400);
    // create_unimplemented_device("CEC",       0x40007800, 0x400);
    // create_unimplemented_device("AFIO",      0x40010000, 0x400);
    // create_unimplemented_device("EXTI",      0x40010400, 0x400);
    // create_unimplemented_device("GPIOA",     0x40010800, 0x400);
    // create_unimplemented_device("GPIOB",     0x40010C00, 0x400);
    // create_unimplemented_device("GPIOC",     0x40011000, 0x400);
    // create_unimplemented_device("GPIOD",     0x40011400, 0x400);
    // create_unimplemented_device("GPIOE",     0x40011800, 0x400);
    // create_unimplemented_device("ADC1",      0x40012400, 0x400);
    // create_unimplemented_device("timer[1]",  0x40012C00, 0x400);
    // create_unimplemented_device("timer[15]", 0x40014000, 0x400);
    // create_unimplemented_device("timer[16]", 0x40014400, 0x400);
    // create_unimplemented_device("timer[17]", 0x40014800, 0x400);
    // create_unimplemented_device("DMA",       0x40020000, 0x400);
    // create_unimplemented_device("RCC",       0x40021000, 0x400);
    // create_unimplemented_device("Flash Int", 0x40022000, 0x400);
    // create_unimplemented_device("CRC",       0x40023000, 0x400);
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
