#include "isr.h"

#include "apic.h"
#include "log/boot_log.h"

noreturn void isr_exception_stub_func(uint64_t isr_num)
{
	boot_log_printf_status(BOOT_LOG_STATUS_FAIL, "CPU exception 0x%X, CPU #%u", (unsigned)isr_num, lapic_read(LAPIC_REG_ID));
	asm volatile ("cli; hlt");
}
void isr_exception_stub_func_noerr(uint64_t isr_num)
{
	boot_log_printf_status(BOOT_LOG_STATUS_WARN, "CPU exception 0x%X (stub), CPU #%u", (unsigned)isr_num, lapic_read(LAPIC_REG_ID));
}
