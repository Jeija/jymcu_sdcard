#include "util.h"
#include <avr/io.h>

// ADC
void adc_request(uint8_t port)
{
	ADCSRA = ADC_ENABLE | ADC_FACTOR_128;
	ADMUX = ADC_REFERENCE_INTERNAL | port;

	ADCSRA |= ADC_START;
}

uint16_t adc_read(void)
{
	while (ADCSRA & (1<<ADSC));
	uint16_t output = ADCW;

	ADCSRA &= ~(1<<ADEN);
	return output;
}
