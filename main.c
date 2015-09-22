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
#include <avr/eeprom.h>

#define BIT_LED 1
#define IR_REC 2

#define STARTLOWLEN 72

volatile int insleep;
volatile int irstat;
volatile int locount;
volatile int hicount;
volatile int bitcount;
volatile long lastvalue;
volatile long irvalue;

uint16_t jcom_tokyo_freq[] EEMEM = {768,774,783,789,803,809,816,822,828};

ISR(INT0_vect)
{
	if(insleep == 0) {
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
//					TIMSK &= ~(1<<TOIE0);
					irstat = 2;
				}
			}
		} else if(irstat == 2) {
		}
	}
	TCNT0 = 0x00;
}

#if 0
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
#endif

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
	int j;
	int idlecount;

	// INTピンの論理変化で割り込み
	//	MCUCR |= (1<<ISC00);
	// 外部割り込みマスクレジスタ
	GIMSK |= (1<<INT0);

	DDRB |= 1 << BIT_LED;   /* output for LED */
	PORTB |= 1 << BIT_LED; // LED off

	TCCR0B |= (1<<CS02);
	irstat = 0;
	long lastsend = 0;
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	sei();
//	TIMSK |= 1<<TOIE0;
	// INTピンの論理変化で割り込み
	MCUCR |= (1<<ISC00);
	idlecount = 0;
	insleep = 0;
	for (;;) {
		if(irstat == 0) {
			if(idlecount > 10) { // 200ms * 10 = 2 sec
//				PORTB &= ~(1 << BIT_LED); // LED on
				// INTピンのLレベルで割り込みに変更
				MCUCR &= ~(1<<ISC00);
				MCUCR &= ~(1<<ISC01);
				// Timer0停止
//				TCCR0B = 0;
				insleep = 1;

				sleep_mode();

				insleep = 0;
				idlecount = 0;
//				PORTB |= 1 << BIT_LED; // LED off
				// INTピンの論理変化で割り込みに戻す
				MCUCR |= (1<<ISC00);
//				TCCR0B |= (1<<CS02);
			} else {
				++idlecount;
			}
		}
		if(irstat == 2) {
			if(lastvalue == irvalue && lastsend != irvalue) {
				lastsend = irvalue;
				GIMSK &= ~(1<<INT0);
				int button = 0;
				switch (irvalue) {
					case 0x010:   // 1
						button = 1;
						break;
					case 0x810:   // 2
						button = 2;
						break;
					case 0x410:   // 3
						button = 3;
						break;
					case 0xc10:   // 4
						button = 4;
						break;
					case 0x210:   // 5
						button = 5;
						break;
					case 0xa10:   // 6
						button = 6;
						break;
					case 0x610:   // 7
						button = 7;
						break;
					case 0xe10:   // 8
						button = 8;
						break;
					case 0x110:   // 9
						button = 9;
						break;
					case 0x910:   // 10
					case 0x510:   // 11
					case 0xd10:   // 12
					default:
						button = 0;
						break;
				}
				if(button != 0) {
					int fqint = eeprom_read_word(&jcom_tokyo_freq[button-1]);
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

//					PORTB &= ~(1 << BIT_LED); // LED on
					uartInit();
					for (temp = 0; temp < j; temp++) {
						sendByte(messageBuf[temp]);
					}
					uartStop();
//					PORTB |= 1 << BIT_LED; // LED off

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
