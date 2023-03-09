#include "mtask.h"
#include "smp_trampoline.h"

#include "string.h"

#include "cpu/x86/apic.h"
#include "cpu/x86/pit.h"
#include "cpu/x86/gdt.h"
#include "cpu/x86/rsdp.h"
#include "cpu/x86/hpet.h"
#include "cpu/cpu_io.h"
#include "cpu/cpu_int.h"

#include "log/boot_log.h"
#include "dev/uart.h"

#include "kernlib/kernmem.h"

#include "ap_periodic_switch.h"
#include "acpi.h"
#include "scheduler.h"

#include "modules/vmemory/vmemory.h"

uint8_t core_num, bsp_lapic_id;
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
int start_ap(size_t core_ind, uint32_t task_reg);

int mtask_init()
{
	size_t hpet_timer_blocks_cnt;
	hpet_desc_table** hpet_timer_blocks = hpet_get_timer_blocks(&hpet_timer_blocks_cnt);
	uart_printf("COUNTER_CLK_PERIOD: %lu fs\r\n", HPET_COUNTER_CLK_PERIOD(HPET_READ_REG(hpet_timer_blocks[0]->base_addr.addr, HPET_GENREG_CAP_ID)));
	HPET_SET_ENABLE_CNF(HPET_PTR_REG(hpet_timer_blocks[0]->base_addr.addr, HPET_GENREG_CONF)); // enable timer 0

	rsdp* _rsdp = find_rsdp(get_mem_unit_size());
	if(!_rsdp)
		return MTASK_ERR_CANT_FIND_RSDP;

	lapic_ids = kmalloc(256);

	core_num = detect_cpus(RSDP_GET_PTR(_rsdp), lapic_ids, &bsp_lapic_id);

	lapic_ids = krealloc(lapic_ids, core_num);
	core_info = kmalloc(core_num * sizeof(core_info_t));
	memset(core_info, 0, core_num * sizeof(core_info_t));

	boot_log_printf_status(BOOT_LOG_STATUS_RUNNING, "Detected %u APs, trying to start them", core_num - 1);
	boot_log_increase_nest_level();
	for(uint8_t i = 0; i < core_num; ++i){
		if(lapic_ids[i] != bsp_lapic_id){
			gdt_tss_desc* tr = kmalloc(sizeof(gdt_tss_desc));
			memset(tr, 0, sizeof(*tr));
			void* tsss = kmalloc(MTASK_TSS_STACK_SIZE);
			tr->ist[0] = (uintptr_t)(tsss + MTASK_TSS_STACK_SIZE);
			uint32_t gd = gdt_add_tss_desc(tr);
			if(gd == (uint32_t)-1){
				boot_log_decrease_nest_level();
				boot_log_printf_status(BOOT_LOG_STATUS_FAIL, "Cannot start APs: out of space in GDT, needed for TSS");
				boot_log_increase_nest_level();
				break;
			}
			boot_log_printf_status(BOOT_LOG_STATUS_RUNNING, "Starting AP #%u", lapic_ids[i]);
			int status = start_ap(i, gd);
			if(!status)
				boot_log_printf_status(BOOT_LOG_STATUS_FAIL, "Failed to start AP #%u", lapic_ids[i]);
			else
				boot_log_printf_status(BOOT_LOG_STATUS_SUCCESS, "Starting AP #%u", lapic_ids[i]);
		}
		else
			core_info[i].flags = MTASK_CORE_FLAG_BSP;
	}
	boot_log_decrease_nest_level();
	boot_log_printf_status(BOOT_LOG_STATUS_SUCCESS, "Detected %u APs, trying to start them", core_num - 1);

	// init scheduler
	int err = scheduler_init();
	if(err)
		return err;

	// set task segment register for BSP
	gdt_tss_desc* tr = kmalloc(sizeof(gdt_tss_desc));
	memset(tr, 0, sizeof(*tr));
	void* tsss = kmalloc(MTASK_TSS_STACK_SIZE);
	tr->ist[0] = (uintptr_t)(tsss + MTASK_TSS_STACK_SIZE);
	uint32_t gd = gdt_add_tss_desc(tr);
	if(gd == (uint32_t)-1)
		boot_log_printf_status(BOOT_LOG_STATUS_FAIL, "Cannot set TR for BSP: out of space in GDT");
	else{
		uint16_t gd16 = (uint16_t)gd;
		asm volatile("ltr %0" :: "r"(gd16));
	}

	// set APIC timer for task switching for BSP
	if(!cpu_interrupt_set_gate(ap_periodic_switch, MTASK_SWITCH_TIMER_GATE, CPU_INT_TYPE_INTERRUPT))
		return MTASK_ERR_GATE_OOB;
	apic_set_timer(APIC_TIMER_PERIODIC, MTASK_SWITCH_TIMER_TIME, MTASK_SWITCH_TIMER_GATE);

	return 0;
}

int start_ap(size_t core_ind, uint32_t task_reg)
{
	void* conv_mem = (void*)0x9000;
	size_t smp_trampoline_size = smp_trampoline_end - smp_trampoline_start;
	static int trampoline_is_copied = 0;
	memcpy(conv_mem, smp_trampoline_start, smp_trampoline_size);

	// use GDT, IDT and CR3 from kernel to start the core
	smp_trampoline_data* tramp_data = (void*)conv_mem + SMP_TRAMP_DATA_OFFSET;

	tramp_data->tss = task_reg;
	asm volatile("sgdt %0" : "=m" (tramp_data->gdt_ptr));
	asm volatile("sidt %0" : "=m" (tramp_data->idt_ptr));
	uint64_t cr3;
	asm volatile("mov %%cr3, %%rax\n\t"
				 "mov %%rax, %0" : "=m" (cr3));
	tramp_data->page_table = cr3;

	tramp_data->boot_flag = 0;
	tramp_data->jmp_loc = (uintptr_t)(&core_info[core_ind].jmp_loc);
	tramp_data->no_code_fallback_jmp = (uintptr_t)(&core_info[core_ind].no_code_fallback_jmp);

	tramp_data->ap_timer_set_func = (uintptr_t)ap_set_timer;

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

	void* jmp_loc = &core_info[ap_idx].jmp_loc;
	asm volatile("mov %1, %%rax\n\t"
				 "lock xchg (%0), %%rax\n\t" :: "d"(jmp_loc), "m"(loc) : "memory");
	return 0;
}

int ap_set_timer()
{
	apic_enable_spurious_ints();
	apic_set_timer(APIC_TIMER_PERIODIC, MTASK_SWITCH_TIMER_TIME, MTASK_SWITCH_TIMER_GATE);
	return 0;
}


extern uint64_t _ts_scheduler_switch_enable_flag[1];
void toggle_sts(int enable)
{
	*_ts_scheduler_switch_enable_flag = enable;
}
