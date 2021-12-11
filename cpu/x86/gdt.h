#ifndef X86_GDT_H
#define X86_GDT_H

/* Sets GDT register to point to a staticly stored flat kernel-only GDT.
*  Also reloads segment registers with GDT data selector in order for GDT to take effect.
*/
void init_gdt_flat();

#endif
