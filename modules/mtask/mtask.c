#include "mtask.h"
#include "smp_trampoline.h"

#include "string.h"

#include "cpu/x86/apic.h"
#include "cpu/cpu_io.h"

#include "log/boot_log.h"
#include "dev/uart.h"

#include "acpi.h"

/* 1 - success, 0 - timeout waiting on trampoline */
int start_ap(uint32_t lapic_id);

int init()
{
	rsdp* _rsdp = find_rsdp();
	if(!_rsdp)
		return MTASK_ERR_CANT_FIND_RSDP;

	uint8_t core_num, bsp_lapic_id;
	uint8_t lapic_ids[256];

	if(_rsdp->revision < 2)
		core_num = detect_cpus((void*)(uintptr_t)_rsdp->rsdt_addr, lapic_ids, &bsp_lapic_id);
	else
		core_num = detect_cpus((void*)_rsdp->xsdt_addr, lapic_ids, &bsp_lapic_id);

	boot_log_printf_status(BOOT_LOG_STATUS_RUNNING, "Detected %u APs, trying to start them", core_num - 1);
	boot_log_increase_nest_level();
	for(uint8_t i = 0; i < core_num; ++i){
		if(lapic_ids[i] != bsp_lapic_id){
			boot_log_printf_status(BOOT_LOG_STATUS_RUNNING, "Starting AP #%u", lapic_ids[i]);
			int status = start_ap(lapic_ids[i]);
			if(!status)
				boot_log_printf_status(BOOT_LOG_STATUS_FAIL, "Failed to start AP #%u", lapic_ids[i]);
			else
				boot_log_printf_status(BOOT_LOG_STATUS_SUCCESS, "Starting AP #%u", lapic_ids[i]);
		}
	}
	boot_log_decrease_nest_level();
	boot_log_printf_status(BOOT_LOG_STATUS_SUCCESS, "Detected %u APs, trying to start them", core_num - 1);
	return 0;
}


void pit_sleep_ms(uint64_t ms)
{
	uint64_t total_count = 0x4A9 * ms;
	do {
		uint16_t count = total_count > 0xFFFFU ? 0xFFFFU : total_count;
		cpu_out8(0x43, 0x30);
		cpu_out8(0x40, count & 0xFF);
		cpu_out8(0x40, count >> 8);
		do {
			__builtin_ia32_pause();
			cpu_out8(0x43, 0xE2);
		} while ((cpu_in8(0x40) & (1 << 7)) == 0);
		total_count -= count;
	} while ((total_count & ~0xFFFF) != 0);
}

int start_ap(uint32_t lapic_id)
{
	void* conv_mem = (void*)0x9000;
	size_t smp_trampoline_size = smp_trampoline_end - smp_trampoline_start;

	// use GDT, IDT and CR3 from kernel to start the core
	smp_trampoline_data* tramp_data = (void*)smp_trampoline_start + SMP_TRAMP_DATA_OFFSET;

	asm volatile("sgdt %0" : "=m" (tramp_data->gdt_ptr));
	asm volatile("sidt %0" : "=m" (tramp_data->idt_ptr));
	uint64_t cr3;
	asm volatile("mov %%cr3, %%rax\n\t"
				 "mov %%rax, %0" : "=m" (cr3));
	tramp_data->page_table = cr3;

	uart_printf("starting core #%u\r\n", lapic_id);
	memcpy(conv_mem, smp_trampoline_start, smp_trampoline_size);

	lapic_write(LAPIC_REG_ICR1, lapic_id << 24);
	lapic_write(LAPIC_REG_ICR0, LAPIC_IPI_INIT);
	pit_sleep_ms(10);

	lapic_write(LAPIC_REG_ICR1, lapic_id << 24);
	lapic_write(LAPIC_REG_ICR0, LAPIC_IPI_STARTUP | ((uint32_t)(uintptr_t)conv_mem / 4096));
	pit_sleep_ms(5);

	// now read data from trampoline (boot flag)
	tramp_data = conv_mem + SMP_TRAMP_DATA_OFFSET;
	uint8_t boot_flag;
	size_t tries = 0;
	do{
		asm volatile("xor %%al, %%al\n\t"
					 "lock xadd %%al, %1\n\t"
					 "mov %%al, %0\n\t" : "=m" (boot_flag) : "m" (tramp_data->boot_flag));
		pit_sleep_ms(5);
	} while(!boot_flag && tries++ < MTASK_AP_BOOT_TRY_COUNT);

	if(tries >= MTASK_AP_BOOT_TRY_COUNT)
		return 0;
	return 1;
}

