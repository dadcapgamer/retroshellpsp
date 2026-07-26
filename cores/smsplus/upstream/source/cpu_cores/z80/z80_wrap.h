#ifndef WRAP_H
#define WRAP_H

#define ASSERT_LINE 1
#define CLEAR_LINE 0
#define IRQ_LINE_NMI 127
#define INPUT_LINE_NMI 32
#define INPUT_LINE_IRQ0 0

#define CPUZ80_Init() \
	z80_init(sms_irq_callback);

#define CPUZ80_Reset() \
	z80_reset(); \
	z80_reset_cycle_count(); \
	z80_set_irq_line (0, CLEAR_LINE);

#define CPUIRQ_Pause() \
	z80_set_irq_line(INPUT_LINE_NMI, ASSERT_LINE); \
	z80_set_irq_line(INPUT_LINE_NMI, CLEAR_LINE);

extern uint8_t *cpu_readmap[64];
extern uint8_t *cpu_writemap[64];

#endif
