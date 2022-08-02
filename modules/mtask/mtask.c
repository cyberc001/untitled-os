#include "mtask.h"
#include "smp_trampoline.h"

#include "string.h"

#include "cpu/x86/apic.h"
#include "cpu/x86/pit.h"
#include "cpu/cpu_io.h"
#include "cpu/cpu_int.h"

#include "log/boot_log.h"
#include "dev/uart.h"

#include "kernlib/kernmem.h"

#include "acpi.h"

static void ap_periodic_switch()
{
	uart_printf("time's up\r\n");
	lapic_write(LAPIC_REG_EOI, 0);
}

uint8_t core_num = 1, bsp_lapic_id = 0;
void** ap_jmp_locs;
uint8_t* lapic_ids;

uint8_t get_core_num() { return core_num; }
size_t get_bsp_idx()
{
	for(size_t i = 0; i < core_num; ++i)
		if(lapic_ids[i] == bsp_lapic_id)
			return i;
	return 0;
}

/* 1 - success, 0 - timeout waiting on trampoline */
int start_ap(size_t core_ind);

int init()
{
	rsdp* _rsdp = find_rsdp();
	if(!_rsdp)
		return MTASK_ERR_CANT_FIND_RSDP;

	lapic_ids = kmalloc(256);

	if(_rsdp->revision < 2)
		core_num = detect_cpus((void*)(uintptr_t)_rsdp->rsdt_addr, lapic_ids, &bsp_lapic_id);
	else
		core_num = detect_cpus((void*)_rsdp->xsdt_addr, lapic_ids, &bsp_lapic_id);

	lapic_ids = krealloc(lapic_ids, core_num);
	ap_jmp_locs = kmalloc(core_num * sizeof(void*));
	memset(ap_jmp_locs, 0, core_num * sizeof(void*));

	boot_log_printf_status(BOOT_LOG_STATUS_RUNNING, "Detected %u APs, trying to start them", core_num - 1);
	boot_log_increase_nest_level();
	for(uint8_t i = 0; i < core_num; ++i){
		if(lapic_ids[i] != bsp_lapic_id){
			boot_log_printf_status(BOOT_LOG_STATUS_RUNNING, "Starting AP #%u", lapic_ids[i]);
			int status = start_ap(i);
			if(!status)
				boot_log_printf_status(BOOT_LOG_STATUS_FAIL, "Failed to start AP #%u", lapic_ids[i]);
			else
				boot_log_printf_status(BOOT_LOG_STATUS_SUCCESS, "Starting AP #%u", lapic_ids[i]);
		}
	}
	boot_log_decrease_nest_level();
	boot_log_printf_status(BOOT_LOG_STATUS_SUCCESS, "Detected %u APs, trying to start them", core_num - 1);

	//set APIC timer for task switching for BP aswell
	/*if(!cpu_interrupt_set_gate(ap_periodic_switch, MTASK_SWITCH_TIMER_GATE, CPU_INT_TYPE_INTERRUPT))
		return MTASK_ERR_GATE_OOB;
	apic_set_timer(APIC_TIMER_PERIODIC, MTASK_SWITCH_TIMER_TIME, MTASK_SWITCH_TIMER_GATE);*/

	return 0;
}

int start_ap(size_t core_ind)
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

	tramp_data->boot_flag = 0;
	tramp_data->jmp_loc = (uintptr_t)(ap_jmp_locs + core_ind);

	tramp_data->ap_timer_set_func = (uintptr_t)ap_set_timer;

	memcpy(conv_mem, smp_trampoline_start, smp_trampoline_size);

	lapic_write(LAPIC_REG_ICR1, lapic_ids[core_ind] << 24);
	lapic_write(LAPIC_REG_ICR0, LAPIC_IPI_INIT);
	pit_sleep_ms(10);

	lapic_write(LAPIC_REG_ICR1, lapic_ids[core_ind] << 24);
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


int ap_jump(size_t ap_idx, void* loc)
{
	if(ap_idx >= core_num)
		return MTASK_ERR_AP_IDX_DOESNT_EXIST;

	void* jmp_loc = &ap_jmp_locs[ap_idx];
	asm volatile("mov %1, %%rax\n\t"
				 "lock xchg (%0), %%rax\n\t" :: "d"(jmp_loc), "m"(loc) : "memory");
	return 0;
}

int ap_set_timer()
{
	apic_enable_spurious_ints();

	if(!cpu_interrupt_set_gate(ap_periodic_switch, MTASK_SWITCH_TIMER_GATE, CPU_INT_TYPE_INTERRUPT))
		return MTASK_ERR_GATE_OOB;
	apic_set_timer(APIC_TIMER_PERIODIC, MTASK_SWITCH_TIMER_TIME, MTASK_SWITCH_TIMER_GATE);
	return 0;
}
