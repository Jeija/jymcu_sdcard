#include <avr/io.h>
#include <avr/interrupt.h>

#include "global.h"

#define TIMER_PRESC	256
#define TIMER_PRESC_256	0b00000100

float SYSTIME;

// Timer
void systimer_init(void)
{
	TCCR0B = TIMER_PRESC_256;
	sbi(TIMSK0, TOIE0); // Timer interrupt enable
}

ISR(TIMER0_OVF_vect)
{
	SYSTIME += ((256. * TIMER_PRESC) / F_CPU );
}
