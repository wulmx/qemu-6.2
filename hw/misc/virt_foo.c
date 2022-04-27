/*
 * Virtual Foo Device
 *
 * Copyright (c) 2017 Milo Kim <woogyom.kim@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */
 
#include "qemu/osdep.h"
#include "hw/hw.h"
#include "hw/sysbus.h"
#include "hw/irq.h"
#include "qemu/bitops.h"
#include "qemu/log.h"
 
#define TYPE_VIRT_FOO      "virt-foo"
#define VIRT_FOO(obj)      OBJECT_CHECK(VirtFooState, (obj), TYPE_VIRT_FOO)
 
/* Register Map */
#define REG_ID             0x0
#define CHIP_ID            0xf001
 
#define REG_INIT           0x4
#define CHIP_EN            BIT(0)
 
#define REG_CMD            0x8
 
#define REG_INT_STATUS     0xc
#define INT_ENABLED        BIT(0)
#define INT_BUFFER_DEQ     BIT(1)
 
typedef struct {
    SysBusDevice parent_obj;
    MemoryRegion iomem;
    qemu_irq     irq;
    uint32_t     id;
    uint32_t     init;
    uint32_t     cmd;
    uint32_t     status;
} VirtFooState;
 
static void virt_foo_set_irq(VirtFooState *s, int irq) {
    fprintf(stdout, "[QEMU_VIRT_FOO]Call virt_foo_set_irq!\n");
    s->status = irq;
    qemu_set_irq(s->irq, 1);
}
 
static void virt_foo_clr_irq(VirtFooState *s) {
    fprintf(stdout, "[QEMU_VIRT_FOO]Call virt_foo_clr_irq!\n");
    qemu_set_irq(s->irq, 0);
}
 
static uint64_t virt_foo_read(void *opaque, hwaddr offset, unsigned size) {
    VirtFooState *s = (VirtFooState *)opaque;
    bool is_enabled = s->init & CHIP_EN;
    fprintf(stdout, "[QEMU_VIRT_FOO]Call virt_foo_read!\n");
    if(!is_enabled) {
        fprintf(stderr, "Virt foo Device is disabled!\n");
        return 0;
    }
 
    switch(offset) {
        case REG_ID:
            return s->id;
        case REG_INIT:
            return s->init;
        case REG_CMD:
            return s->cmd;
        case REG_INT_STATUS:
            virt_foo_clr_irq(s);
            return s->status;
        default:
            break;
    }
    return 0;
}
 
static void virt_foo_write(void *opaque, hwaddr offset, uint64_t value, unsigned size) {
    VirtFooState *s = (VirtFooState *)opaque;
    fprintf(stdout, "[QEMU_VIRT_FOO]Call virt_foo_write!\n");
    switch(offset) {
        case REG_INIT:
            s->init = (int)value;
            if (value)
                virt_foo_set_irq(s, INT_ENABLED);
            break;
        case REG_CMD:
            s->cmd = (int)value;
            virt_foo_set_irq(s, INT_BUFFER_DEQ);
            break;
        default:
            break;
    }
}
 
static const MemoryRegionOps virt_foo_ops = {
    .read = virt_foo_read,
    .write = virt_foo_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};
 
static void virt_foo_realize(DeviceState *d, Error **errp) {
    VirtFooState *s = VIRT_FOO(d);
    SysBusDevice *sbd = SYS_BUS_DEVICE(d);
    fprintf(stdout, "[QEMU_VIRT_FOO]Call virt_foo_realize!\n");
    memory_region_init_io(&s->iomem, OBJECT(s), &virt_foo_ops, s, TYPE_VIRT_FOO, 0x200);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq);
 
    s->id = CHIP_ID;
    s->init = 0;
}
 
static void virt_foo_class_init(ObjectClass *klass, void *data) {
    fprintf(stdout, "[QEMU_VIRT_FOO]Call ObjectClass!\n");
    DeviceClass *dc = DEVICE_CLASS(klass);
    dc->realize = virt_foo_realize;
}
 
static const TypeInfo virt_foo_info = {
    .name          = TYPE_VIRT_FOO,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(VirtFooState),
    .class_init    = virt_foo_class_init,
};
 
static void virt_foo_register_types(void) {
    fprintf(stdout, "[QEMU_VIRT_FOO]Call virt_foo_register_types!\n");
    type_register_static(&virt_foo_info);
}
 
type_init(virt_foo_register_types)
