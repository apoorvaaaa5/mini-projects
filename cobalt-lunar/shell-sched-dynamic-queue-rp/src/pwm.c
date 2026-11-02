#include <stdint.h>
#include <mmio.h>
#include <pwm.h>

/* RP2040 Subsystem Resets */
#define RESETS_RESET            0x4000C000
#define RESETS_RESET_DONE       0x4000C008
#define RESETS_PWM_BIT          (1 << 14)

/* GPIO 25 (Pico Onboard LED) Control Register */
#define IO_BANK0_GPIO25_CTRL    0x400140CC

/* PWM Hardware Registers (Slice 4 is hardwired to GPIO 24 & GPIO 25) */
#define PWM_BASE                0x40050000
#define PWM_CH4_CSR             (PWM_BASE + 0x50)
#define PWM_CH4_DIV             (PWM_BASE + 0x54)
#define PWM_CH4_CTR             (PWM_BASE + 0x58)
#define PWM_CH4_CC              (PWM_BASE + 0x5C)
#define PWM_CH4_TOP             (PWM_BASE + 0x60)

/* Target 2 kHz PWM frequency:
 * Clock = 125 MHz / 125 (DIV) = 1 MHz
 * TOP = 500 ticks -> Frequency = 1 MHz / 500 = 2,000 Hz (2 kHz)
 */
#define PWM_TOP_VAL             500

void pwm_init(void)
{
	/* 1. Take PWM block out of reset */
	mmio32(RESETS_RESET) &= ~RESETS_PWM_BIT;
	while ((mmio32(RESETS_RESET_DONE) & RESETS_PWM_BIT) == 0);

	/* 2. Set GPIO 25 function to PWM (Function 4 = PWM4_B) */
	mmio32(IO_BANK0_GPIO25_CTRL) = 4;

	/* 3. Set clock divider: 125.0 (int = 125, frac = 0) -> 125 MHz / 125 = 1 MHz */
	mmio32(PWM_CH4_DIV) = (125 << 4);

	/* 4. Set wrap/period value for 2 kHz frequency */
	mmio32(PWM_CH4_TOP) = PWM_TOP_VAL;

	/* 5. Start with 0% duty cycle (LED OFF) */
	mmio32(PWM_CH4_CC) = 0;

	/* 6. Enable PWM Slice 4 (Autonomous hardware PWM engine) */
	mmio32(PWM_CH4_CSR) = 1;
}

void pwm_set(uint32_t duty)
{
	if (duty > 100) {
		duty = 100;
	}

	/* Scale duty percentage (0-100) to counter compare (0-500)
	 * GPIO 25 is Channel B of Slice 4, which occupies bits [31:16] of PWM_CC
	 */
	uint32_t cc_b = (duty * PWM_TOP_VAL) / 100;
	mmio32(PWM_CH4_CC) = (cc_b << 16);
}
