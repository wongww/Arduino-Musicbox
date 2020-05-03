#include <avr/io.h>
#include "adc.h"
void adc_init(void)
{
    // Initialize the ADC
    //Set ADMUX REF to 0 1 in bits 7 and 6 respectively.
    //Set/clear the REFS[1:0] bits in ADMUX to select the high voltage reference. 
    Using the AVCC reference is appropriate.
    ADMUX |= 1 << REFS0;
    ADMUX &= ~(1 << REFS1);

    //Set Prescalar to 128
    //Set to 8 bit convertsion
    ADMUX |= 1 << ADLAR;
    //Turn on ADEN
    ADCSRA |= 1 << ADEN;
    //Upper bound is 12,500 cycles and start counting
    ADCSRA |= 0b00000111; 

}
unsigned char adc_sample(unsigned char channel)
{
    // Set ADC input mux bits to 'channel' value
    //Clear channel
    ADMUX &= 0b11110000;
    // Convert an analog input and return the 8-bit result
    ADMUX |= channel;
    ADCSRA |=  1 << ADSC;
    //While ADSC is 1, continue conversion
    while((ADCSRA & 0b01000000) != 0){
    	//Do nothing until ADC is done
    }
    return ADCH;
}
