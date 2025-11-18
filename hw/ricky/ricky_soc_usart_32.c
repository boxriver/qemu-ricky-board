#include "qemu/osdep.h"

#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "sysemu/sysemu.h"
#include "ricky_soc_usart_32.h"

static const Property ricky_soc_usart_properties[] = {
    DEFINE_PROP_CHR("chardev", RickySocUsartState, chr),
    DEFINE_PROP_END_OF_LIST(),
};


static void ricky_soc_usart_update_irq(RickySocUsartState *s)
{
    uint32_t mask = s->usart_sr & s->usart_cr1;

    if (mask & (USART_SR_TXE | USART_SR_TC | USART_SR_RXNE)) {
        qemu_set_irq(s->irq, 1);    //触发中断
    } else {
        qemu_set_irq(s->irq, 0);    //清除中断
    }
}


static void ricky_soc_usart_reset(DeviceState *dev)
{
    RickySocUsartState *s = RICKY_SOC_USART(dev);

    s->usart_sr = USART_SR_RESET;
    s->usart_dr = 0x00000000;
    s->usart_brr = 0x00000000;
    s->usart_cr1 = 0x00000000;
    s->usart_cr2 = 0x00000000;
    s->usart_cr3 = 0x00000000;
    s->usart_gtpr = 0x00000000;

    ricky_soc_usart_update_irq(s);
}


static int ricky_soc_usart_can_receive(void *opaque)
{
    RickySocUsartState *s = RICKY_SOC_USART(opaque);

    if (!(s->usart_sr & USART_SR_RXNE)) {
        return 1;
    }

    return 0;
}

static void ricky_soc_usart_receive(void *opaque, const uint8_t *buf, int size)
{
    RickySocUsartState *s = opaque;

    if (!(s->usart_cr1 & USART_CR1_UE && s->usart_cr1 & USART_CR1_RE)) {
        /* USART not enabled - drop the chars */
        return;
    }

    s->usart_dr = *buf;
    s->usart_sr |= USART_SR_RXNE;

    ricky_soc_usart_update_irq(s);
}


static void ricky_soc_usart_realize(DeviceState *dev, Error **errp)
{
    RickySocUsartState *s = RICKY_SOC_USART(dev);

    qemu_chr_fe_set_handlers(&s->chr, ricky_soc_usart_can_receive, ricky_soc_usart_receive, NULL, NULL, s, NULL, true);
}

static void ricky_soc_usart_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    device_class_set_legacy_reset(dc, ricky_soc_usart_reset);
    device_class_set_props(dc, ricky_soc_usart_properties);
    dc->realize = ricky_soc_usart_realize;
}


static void ricky_soc_usart_write(void *opaque, hwaddr addr, uint64_t val64, unsigned int size)
{
    RickySocUsartState *s = RICKY_SOC_USART(opaque);
    uint32_t value = val64;
    unsigned char ch;

    switch (addr) {
        case USART_SR:
            if (value <= 0x3FFF) {
                // I/O being synchronous, TXE is always set.
                // In addition, it may only be set by hardware, so keep it set here
                s->usart_sr = value | USART_SR_TXE;
            } else {
                s->usart_sr &= value;
            }
            ricky_soc_usart_update_irq(s);
            return;
        case USART_DR:
            if (value < 0xF000) {
                ch = value;
                // XXX this block entire thread. Rewrite to use 
                // qemu_chr_fe_write and background I/O callbacks
                qemu_chr_fe_write_all(&s->chr, &ch, 1);
                // IO are currently synchronous, making it impossible for software
                // to observe transient states where TXE or TC aren't set. 
                // Unlike TXE however, which is read-only, software may clear TC
                // by writing 0 to the SR register, so set it again on each write
                s->usart_sr |= USART_SR_TC;
                ricky_soc_usart_update_irq(s);
            }
            return;
        case USART_BRR:
            s->usart_brr = value;
            return;
        case USART_CR1:
            s->usart_cr1 = value;
            return;
        case USART_CR2:
            s->usart_cr2 = value;
            return;
        case USART_CR3:
            s->usart_cr3 = value;
            return;
        case USART_GTPR:
            s->usart_gtpr = value;
            return;
        default:
            qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset 0x%"HWADDR_PRIx"\n", __func__, addr);
    }
}

static uint64_t ricky_soc_usart_read(void *opaque, hwaddr addr, unsigned int size)
{
    RickySocUsartState *s = RICKY_SOC_USART(opaque);

    uint64_t retvalue = 0;
    switch (addr) {
        case USART_SR:
            retvalue = s->usart_sr;
            qemu_chr_fe_accept_input(&s->chr);
            break;
        case USART_DR:
            retvalue = s->usart_dr & 0x3FF;
            s->usart_sr &= ~USART_SR_RXNE;
            qemu_chr_fe_accept_input(&s->chr);
            ricky_soc_usart_update_irq(s);
            break;
        case USART_BRR:
            retvalue = s->usart_brr;
            break;
        case USART_CR1:
            retvalue = s->usart_cr1;
            break;
        case USART_CR2:
            retvalue = s->usart_cr2;
            break;
        case USART_CR3:
            retvalue = s->usart_cr3;
            break;
        case USART_GTPR:
            retvalue = s->usart_gtpr;
            break;
        default:
            qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset 0x%"HWADDR_PRIx"\n", __func__, addr);
            return 0;
    }
    return retvalue;
}


static const MemoryRegionOps ricky_soc_usart_ops = {
    .read = ricky_soc_usart_read,
    .write = ricky_soc_usart_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};


static void ricky_soc_usart_init(Object *obj)
{
    RickySocUsartState *s = RICKY_SOC_USART(obj);
    sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->irq);
    memory_region_init_io(&s->mmio, obj, &ricky_soc_usart_ops, s, TYPE_RICKY_SOC_USART, 0x400);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->mmio);
}


static const TypeInfo ricky_soc_usart_info = {
    .name = TYPE_RICKY_SOC_USART,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(RickySocUsartState),
    .instance_init = ricky_soc_usart_init,
    .class_init = ricky_soc_usart_class_init,
};

static void ricky_soc_usart_register_types(void)
{
    type_register_static(&ricky_soc_usart_info);
}

type_init(ricky_soc_usart_register_types)

















