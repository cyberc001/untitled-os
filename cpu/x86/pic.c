#include "pic.h"

#include "../cpu_io.h"

// PIC port addresses
#define PIC1_PORT_COMMAND	0x20
#define PIC1_PORT_DATA		((PIC1_PORT_COMMAND) + 1)
#define PIC2_PORT_COMMAND 	0xA0
#define PIC2_PORT_DATA		((PIC2_PORT_COMMAND) + 1)

// PIC commands
#define PIC_COMMAND_EOI		0x20

// Initialization words
#define PIC_ICW1_ICW4		0x01	// ICW4 needed/not needed
#define PIC_ICW1_SINGLE		0x02	// single/cascade mode (1/2 PIC chips)
#define PIC_ICW1_INTERVAL4	0x04	// call address interval 4/8
#define PIC_ICW1_LEVEL		0x08	// level-triggered/edge mode
#define PIC_ICW1_INIT		0x10	// Required initialization bit

#define PIC_ICW4_8086		0x01	// 8086/88 / MCS-80/85 mode
#define PIC_ICW4_AUTO		0x02	// auto/normal EOI
#define PIC_ICW4_BUF_SLAVE	0x08	// buffered mode/slave
#define PIC_ICW4_BUF_MASTER	0x0C	// buffered mode/master
#define PIC_ICW4_SFNM		0X10	// special fully nested/not fully nested

void pic_send_eoi(uint8_t irq)
{
	if(irq >= 8) // for slave PIC, notify both PICs
		cpu_out8(PIC2_PORT_COMMAND, PIC_COMMAND_EOI);
	cpu_out8(PIC1_PORT_COMMAND, PIC_COMMAND_EOI);
}

void pic_remap_irqs(uint8_t master_off, uint8_t slave_off)
{
	uint8_t mask1, mask2;
	mask1 = cpu_in8(PIC1_PORT_DATA);
	mask2 = cpu_in8(PIC2_PORT_DATA);

	// start the initialization proccess
	cpu_out8(PIC1_PORT_COMMAND, PIC_ICW1_INIT | PIC_ICW1_ICW4);	cpu_io_wait();
	cpu_out8(PIC2_PORT_COMMAND, PIC_ICW1_INIT | PIC_ICW1_ICW4);	cpu_io_wait();
	cpu_out8(PIC1_PORT_DATA, master_off);		cpu_io_wait();
	cpu_out8(PIC2_PORT_DATA, slave_off);		cpu_io_wait();
	cpu_out8(PIC1_PORT_DATA, 4);		cpu_io_wait();	// tell master PIC of presence of slave PIC at IRQ #2
	cpu_out8(PIC2_PORT_DATA, 2);		cpu_io_wait();	// tell slave PIC that it is a part of a cascade

	cpu_out8(PIC1_PORT_DATA, PIC_ICW4_8086);		cpu_io_wait();
	cpu_out8(PIC2_PORT_DATA, PIC_ICW4_8086);		cpu_io_wait();

	cpu_out8(PIC1_PORT_DATA, mask1);
	cpu_out8(PIC2_PORT_DATA, mask2);
}

void pic_set_mask(uint8_t irq)
{
	uint16_t port;
	if(irq < 8) // master PIC
		port = PIC1_PORT_DATA;
	else{ // slave PIC
		port = PIC2_PORT_DATA;
		irq -= 8;
	}
	cpu_out8(port, cpu_in8(port) | (1 << irq));
}
void pic_clear_mask(uint8_t irq)
{
	uint16_t port;
	if(irq < 8) // master PIC
		port = PIC1_PORT_DATA;
	else{ // slave PIC
		port = PIC2_PORT_DATA;
		irq -= 8;
	}
	cpu_out8(port, cpu_in8(port) & ~(1 << irq));
}

static uint16_t pic_get_irq_register(uint8_t ocw3)
{
	cpu_out8(PIC1_PORT_COMMAND, ocw3);
	cpu_out8(PIC2_PORT_COMMAND, ocw3);
	return (cpu_in8(PIC2_PORT_COMMAND) << 8) | cpu_in8(PIC1_PORT_COMMAND);
}
uint16_t pic_get_irr(){ return pic_get_irq_register(0x0A); }
uint16_t pic_get_isr(){ return pic_get_irq_register(0x0B); }
