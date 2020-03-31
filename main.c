/*
 * Povorotniki.c
 *
 * Created: 01.03.2020 20:24:02
 * Author : Dmitry
 */ 
#define F_CPU 1000000UL 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdbool.h>

#define LEFT_IN				PB4
#define RIGHT_IN			PB2
#define FB_IN				PB3
#define LEFT_OUT			PB0
#define RIGHT_OUT			PB1

#define PIN_IS_SET(P)		((PINB & (P)) != 0)
#define PIN_IS_CLER(P)		((PINB & (P)) == 0)

#define DIR_ON(P)			PORTB &= ~(P)
#define DIR_OFF(P)			PORTB |= (P)

#define DELAY_OFF		50				//��, �������� ����������
#define DELAY_ON		50				//��, �������� ���������
#define TIMEOUT			6000			//��, ������� ���������� �����������
#define FLASHES_1		6				//���-�� ������� ������������ (x/2)
#define FLASHES_2		6				//���-�� ������� ���������	(x/2)

typedef enum
{
	MODE_OFF,
	MODE_LEFT,
	MODE_RIGHT,
	MODE_BOTH	
}STATE;

STATE state = MODE_OFF;				//��������� ����������� ��������
volatile uint16_t tim0_tick = 0;	//��������� ����������
volatile bool timStart = false;		//������ ��������� ����������
volatile uint8_t changePin = 0;		//������� ��������� ��������� ���� �������� �����

static inline void mcuInit()
{
	//----------- ������������� ������ ����� ������ ----------
	PORTB  = (1 << LEFT_OUT) | (1 << RIGHT_OUT);
	DDRB = (1 << LEFT_OUT) | (1 << RIGHT_OUT);	
	
	//--------- Timer/Counter 0 initialization -----------
	// Clock source: System Clock
	// Clock value: 18.750 kHz
	// Mode: CTC top=OCR0A
	// OC0A output: Disconnected
	// OC0B output: Disconnected
	// Timer Period: 10.027 ms
	TCCR0A=(0<<COM0A1) | (0<<COM0A0) | (0<<COM0B1) | (0<<COM0B0) | (1<<WGM01) | (0<<WGM00);
	TCCR0B=(0<<WGM02) | (0<<CS02) | (1<<CS01) | (1<<CS00);
	TCNT0=0x00;
	OCR0A=0xBB;
	OCR0B=0x00;
	// Timer/Counter 0 Interrupt(s) initialization
	TIMSK0=(0<<OCIE0B) | (1<<OCIE0A) | (0<<TOIE0);
	
	//----------------- External Interrupt(s) initialization ----------------
	// INT0: Off
	// Interrupt on any change on pins PCINT0-5: On
	GIMSK=(0<<INT0) | (1<<PCIE);
	MCUCR=(0<<ISC01) | (0<<ISC00);
	PCMSK=(0<<PCINT5) | (0<<PCINT4) | (1<<PCINT3) | (0<<PCINT2) | (0<<PCINT1) | (0<<PCINT0);
	GIFR=(0<<INTF0) | (1<<PCIF);
	
}

int main(void)
{
	mcuInit();
	bool changeDir = false;
	//uint8_t leftDir = 0;
	//uint8_t rightDir = 0;
	//uint8_t bothDir = 0;
	
	sei();
    while (1) 
    {
			
		switch (state)
		{
		case MODE_OFF:
			tim0_tick = 0;
			changePin = 0;	
						
			//�������� ��������� ������� ������
			if(PIN_IS_SET(1 << LEFT_IN) && PIN_IS_CLER (1 << RIGHT_IN))	
			{
					_delay_ms(DELAY_ON);
					if(PIN_IS_SET(1 << LEFT_IN) && PIN_IS_CLER (1 << RIGHT_IN))
					{
						state = MODE_LEFT;
						timStart = true;
					}
			}
			//�������� ��������� ������� �������
			else if(PIN_IS_SET(1 << RIGHT_IN) && PIN_IS_CLER (1 << LEFT_IN))	
			{
					_delay_ms(DELAY_ON);
					if(PIN_IS_SET(1 << RIGHT_IN) && PIN_IS_CLER (1 << LEFT_IN))
					{
						state = MODE_RIGHT;
						timStart = true;
					}
			}
			//�������� �������������� ������������ ������ � ������� (��������)
			else if(PIN_IS_SET(1 << LEFT_IN) && PIN_IS_SET(1 << RIGHT_IN))
			{
					_delay_ms(DELAY_ON);
					if(PIN_IS_SET(1 << LEFT_IN) && PIN_IS_SET(1 << RIGHT_IN))
					{
						state = MODE_BOTH;
						timStart = true;
					}
			}
		break;
		
		case MODE_LEFT:
			//��������� ����������� ������			
			DIR_ON(1 << LEFT_OUT);
			changeDir = PIN_IS_SET(1 << FB_IN) && PIN_IS_SET(1 << RIGHT_IN);
			
			//����� ��: ����� ���-�� ������� ������ | ������� ������� | ��������
			if(changePin >= FLASHES_1 || changeDir || tim0_tick > (TIMEOUT/10))
			{
				DIR_OFF(1 << LEFT_OUT | 1 << RIGHT_OUT);
				while(PIN_IS_SET(1 << LEFT_IN) || PIN_IS_SET(1 << FB_IN)); // || PIN_IS_SET(1 << RIGHT_IN) || PIN_IS_SET(1 << FB_IN));
				_delay_ms(DELAY_ON);
				if(changeDir) 
				{
					state = MODE_RIGHT;
					tim0_tick = 0;
					changePin = 0;
				}
				else 
				{
					state = MODE_OFF;
					timStart = false;
					_delay_ms(DELAY_OFF);
				}
				
			}
			
		break;
		
		case MODE_RIGHT:
			//��������� ����������� �������
			DIR_ON(1 << RIGHT_OUT);
			changeDir = PIN_IS_SET(1 << FB_IN) && PIN_IS_SET(1 <<LEFT_IN);
			
			//����� ��: ����� ���-�� ������� ������� | ������� ������ | ��������
			if(changePin >= FLASHES_1 || changeDir || tim0_tick > (TIMEOUT/10)) 
			{
				DIR_OFF(1 << RIGHT_OUT | 1 << LEFT_OUT);
				while(PIN_IS_SET(1 << RIGHT_IN) || PIN_IS_SET(1 << FB_IN)); // || PIN_IS_SET(1 << RIGHT_IN) || PIN_IS_SET(1 << FB_IN));
				_delay_ms(DELAY_ON);
				if(changeDir)
				{
					state = MODE_LEFT;
					tim0_tick = 0;
					changePin = 0;
				}
				else 
				{
					state = MODE_OFF;
					timStart = false;
					_delay_ms(DELAY_OFF);
				}
			}
			
		break;	
		
		case MODE_BOTH:
			DIR_ON(1 << RIGHT_OUT | 1 << LEFT_OUT);
			//����� ��: ������ ���-�� ������� �������� | ��������
			if(changePin >= FLASHES_2 || tim0_tick > (TIMEOUT/10))
			{
				timStart = false;
				DIR_OFF(1 << RIGHT_OUT | 1 << LEFT_OUT);
				while(PIN_IS_SET(1 << RIGHT_IN) || PIN_IS_SET (1 << LEFT_IN) || PIN_IS_SET(1 << FB_IN));
				state = MODE_OFF;
				_delay_ms(DELAY_OFF);
			}
		break;
		}
    }
}

//������ ���������� �������
ISR (TIM0_COMPA_vect)
{
	if(timStart) 
	{
		tim0_tick++;
	}
}

//������ ���������� �� ��������� ��������� ����
ISR (PCINT0_vect)
{
	if(state != MODE_OFF)
	{
		changePin++;
	}
}