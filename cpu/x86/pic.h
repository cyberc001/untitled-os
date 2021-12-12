#ifndef X86_PIC_H
#define X86_PIC_H

#include <stdint.h>

void pic_send_eoi(uint8_t irq);

void pic_remap_irqs(uint8_t master_off, uint8_t slave_off);

void pic_set_mask(uint8_t irq);
void pic_clear_mask(uint8_t irq);

uint16_t pic_get_irr();
uint16_t pic_get_isr();

#endif
