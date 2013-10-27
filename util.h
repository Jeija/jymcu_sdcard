#include <avr/io.h>

#ifndef UTIL_H
#define UTIL_H

// ADC
#define ADC_ENABLE (1<<ADEN)
#define ADC_START  (1<<ADSC)
#define ADC_FACTOR_128 ((1<<ADPS2) | (1<<ADPS1) | (1<<ADPS0))
#define ADC_REFERENCE_INTERNAL ((1<<REFS1) | (1<<REFS0))
void adc_request(uint8_t port);
uint16_t adc_read(void);

#endif
