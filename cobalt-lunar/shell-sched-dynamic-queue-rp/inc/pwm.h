#ifndef PWM_H
#define PWM_H

#include <stdint.h>

void pwm_init(void);
void pwm_set(uint32_t duty);

#endif /* PWM_H */
