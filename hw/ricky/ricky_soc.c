#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/module.h"
#include "qemu/log-for-trace.h"
#include "qom/object.h"
#include "ricky_soc.h"

static void ricky_soc_realize(DeviceState *dev_soc, Error **errp)
{
    qemu_log("ricky soc_realize\r\n");
}

static void ricky_soc_initfn(Object *obj)
{
    qemu_log("ricky soc_instance_init\r\n");

    RickySocState *s = RICKY_SOC(obj);
    object_initialize_child(obj, "armv7m", &s->armv7m, TYPE_ARMV7M);



}


static void ricky_soc_class_init(ObjectClass *klass, void *data)
{
    qemu_log("ricky soc_class_init\r\n");
    DeviceClass *dc = DEVICE_CLASS(klass);
    dc->realize = ricky_soc_realize;
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
