#ifndef APIC_H
#define APIC_H

#include "qemu-common.h"

/* apic.c */
void apic_deliver_irq(uint8_t dest, uint8_t dest_mode, uint8_t delivery_mode,
                      uint8_t vector_num, uint8_t trigger_mode);
int apic_accept_pic_intr(DeviceState *s);
void apic_deliver_pic_intr(DeviceState *s, int level);
void apic_reset_irq_delivered(void);
int apic_get_irq_delivered(void);
void cpu_set_apic_base(DeviceState *s, uint64_t val);
uint64_t cpu_get_apic_base(DeviceState *s);
void cpu_set_apic_tpr(DeviceState *s, uint8_t val);
uint8_t cpu_get_apic_tpr(DeviceState *s);
void apic_init_reset(DeviceState *s);
void apic_sipi(DeviceState *s);

int ski_apic_get_interrupt(DeviceState *s);
int original_apic_get_interrupt(DeviceState *s);

/* PF: SKI */
int ski_apic_irq_pending(void *s); // Just slightly changed the arguments
static int apic_irq_pending(void *s); // Just slightly changed the arguments
int ski_apic_get_highest_priority_int(uint32_t *tab, void *s);
int ski_apic_check_interrupt_set(int int_no, void *opaque_apic);
int ski_apic_process_incoming_interrupts(CPUState * env);

/* pc.c */
int cpu_is_bsp(CPUState *env);
DeviceState *cpu_get_current_apic(void);

#endif
