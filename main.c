// This code based on as follow code. But also copy from other location
// https://github.com/itdaniher/Thermocouple-Interfaces
//

#include "globals.h"
#include "uart.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>
#include <avr/sleep.h>

#define BIT_LED 1
#define IR_REC 2

#define STARTLOWLEN 72

volatile int irstat;
volatile int locount;
volatile int hicount;
volatile int bitcount;
volatile long lastvalue;
volatile long irvalue;

ISR(INT0_vect)
{
	if((PINB >> IR_REC) & 0x01) {
		locount = TCNT0;
	} else {
		hicount = TCNT0;
	}
	if(irstat == 0 && ((PINB >> IR_REC) & 0x01) && locount > STARTLOWLEN) {
		irstat = 1;
		bitcount = 0;
		PORTB &= ~(1 << BIT_LED); // LED on
		irvalue = 0;
//		TCCR0A = 0b00000010;
//		OCR0A = 50;
	} else if(irstat == 1) {
		if((PINB >> IR_REC) & 0x01) {
			if(locount > hicount * 2) {
				irvalue = (irvalue << 1) | 1;
			} else {
				irvalue = (irvalue << 1);
			}
			++bitcount;
			if(bitcount == 12) {
				PORTB |= 1 << BIT_LED; // LED off
				TIMSK &= ~(1<<TOIE0);
				irstat = 2;
			}
		}
	} else if(irstat == 2) {
	}
	TCNT0 = 0x00;
}

ISR(TIM0_OVF_vect){
	/*
	if(irstat == 1) {
		PORTB |= 1 << BIT_LED; // LED off
		irstat = 2;
		TIMSK &= ~(1<<TOIE0);
		TCCR0A = 0b00000000;
	}
	 */
}

char hexchar(int val)
{
	if(val <= 9) {
		return '0' + val;
	} else {
		return 'A' + (val - 10);
	}
}

int main ( void )
{
	// 0		1		2		3
	// 192.0	170.7		153.6		139.6
	// 4		5		6		7
	// 128.0	118.2		109.7		102.4	
	// 8		9		10		11
	// 96.0		90.4		85.3		80.8
	// 12		13		14		15
	// 76.8		73.1		69.8		66.8
	// default 3
	int level = 3;
	// 0      1      2      3
	// 236kHz 198kHz 162kHz 126kHz
	// default 1
	int band = 1;
	int mono = 0; // 0,1
	int i;
	int j;

	MCUCR |= (1<<ISC00);
	GIMSK |= (1<<INT0);

	DDRB |= 1 << BIT_LED;   /* output for LED */
	PORTB |= 1 << BIT_LED; // LED off

	TCCR0B |= (1<<CS02);
	TIMSK |= 1<<TOIE0;
	uartInit();
	sei();
	irstat = 0;
	long lastsend = 0;
//	set_sleep_mode(SLEEP_MODE_PWR_DOWN);
//	sleep_mode(); 
	for (;;) {
		if(irstat == 2) {
			if(lastvalue == irvalue && lastsend != irvalue) {
				lastsend = irvalue;
				GIMSK &= ~(1<<INT0);
				long fqint;
				switch (irvalue) {
					case 0x010:   // 1
						fqint = 768;
						break;
					case 0x810:   // 2
						fqint = 774;
						break;
					case 0x410:   // 3
						fqint = 783;
						break;
					case 0xc10:   // 4
						fqint = 789;
						break;
					case 0x210:   // 5
						fqint = 803;
						break;
					case 0xa10:   // 6
						fqint = 809;
						break;
					case 0x610:   // 7
						fqint = 816;
						break;
					case 0xe10:   // 8
						fqint = 822;
						break;
					case 0x110:   // 9
						fqint = 828;
						break;
					default:
						fqint = 0;
						break;
				}
				if(fqint != 0) {
					j = 0;
					messageBuf[j] = (fqint / 100) + '0';
					++j;
					messageBuf[j] = (fqint / 10) % 10 + '0';
					++j;
					messageBuf[j] = fqint % 10 + '0';
					++j;
					messageBuf[j] = hexchar(level);
					++j;
					messageBuf[j] = hexchar(band << 2);
					++j;
					messageBuf[j] = hexchar(mono);
					++j;
					messageBuf[j] = '\n';
					++j;
					//iterate through the first ten items of messageBuf, sending values over the software UART
					for (temp = 0; temp < j; temp++) {
						sendByte(messageBuf[temp]);
					}
				}
				GIMSK |= (1<<INT0);
			}
			lastvalue = irvalue;
			irstat = 0;
		}
	_delay_ms(200);
	}
	return 0;
}
