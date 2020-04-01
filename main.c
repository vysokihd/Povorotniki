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

#define DELAY_OFF		200				//счётчик, задержка выключения направления
#define DELAY_ON		1000			//счётчик, задержка включения направления
#define TIMEOUT			10000			//мс, таймаут отключения направления
#define FLASHES_1		6				//Кол-во вспышек поворотников (x/2)
#define FLASHES_2		6				//Кол-во вспышек аварийкой	(x/2)

typedef enum
{
	MODE_OFF,
	MODE_LEFT,
	MODE_RIGHT,
	MODE_BOTH	
}STATE;

STATE state = MODE_OFF;				//Состояние логического автомата
volatile uint16_t tim0_tick = 0;	//Таймерная переменная
volatile bool timStart = false;		//Запуск таймерной переменной
volatile uint8_t changePin = 0;		//Счётчик изменений состояния пина обратной связи

static inline void mcuInit()
{
	//----------- Инициализация портов ввода вывода ----------
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
	uint16_t leftDir = 0;
	uint16_t rightDir = 0;
	uint16_t bothDir = 0;
	
	sei();
    while (1) 
    {
			
		switch (state)
		{
		case MODE_OFF:
			tim0_tick = 0;
			changePin = 0;
			//Задержка сканирования направлений пока активен исполнительный модуль
			while(PIN_IS_CLER (1 << FB_IN));
			//Проверка входящего сигнала налево
			if(PIN_IS_SET(1 << LEFT_IN) && PIN_IS_CLER (1 << RIGHT_IN))	
			{
					if(leftDir == DELAY_ON)
					{
						state = MODE_LEFT;
						timStart = true;
						leftDir = 0;
						rightDir = 0;
						bothDir = 0;
					}
					leftDir++;
			}
			//Проверка входящего сигнала направо
			if(PIN_IS_SET(1 << RIGHT_IN) && PIN_IS_CLER (1 << LEFT_IN))	
			{
					if(rightDir == DELAY_ON)
					{
						state = MODE_RIGHT;
						timStart = true;
						leftDir = 0;
						rightDir = 0;
						bothDir = 0;
					}
					rightDir++;
			}
			//Проверка одновременного срабатывания налево и направо (аварийка)
			if(PIN_IS_SET(1 << LEFT_IN) && PIN_IS_SET(1 << RIGHT_IN))
			{
					if(bothDir == (DELAY_ON))
					{
						state = MODE_BOTH;
						timStart = true;
						leftDir = 0;
						rightDir = 0;
						bothDir = 0;
					}
					bothDir++;
			}
			//Ничего не выбрано
			if(PIN_IS_CLER(1 << LEFT_IN) && PIN_IS_CLER(1 << RIGHT_IN))
			{
				leftDir = 0;
				rightDir = 0;
				bothDir = 0;
			}
		break;
		
		case MODE_LEFT:
			//Удержание направления налево			
			DIR_ON(1 << LEFT_OUT);
			changeDir = PIN_IS_SET(1 << FB_IN) && PIN_IS_SET(1 << RIGHT_IN);
			
			//Выход по: лимит кол-ва вспышек налево | сигналу направо | таймауту
			if(changePin >= FLASHES_1 || changeDir || tim0_tick > (TIMEOUT/10))
			{
				DIR_OFF(1 << LEFT_OUT | 1 << RIGHT_OUT);
				uint16_t i = DELAY_OFF;
				while(i)
				{
					if(PIN_IS_SET(1 << LEFT_IN)) i = DELAY_OFF;
					else i--;
				}
				state = MODE_OFF;
				timStart = false;
			}
			
		break;
		
		case MODE_RIGHT:
			//Удержание направления направо
			DIR_ON(1 << RIGHT_OUT);
			changeDir = PIN_IS_SET(1 << FB_IN) && PIN_IS_SET(1 <<LEFT_IN);
			
			//Выход по: лимит кол-ва вспышек направо | сигналу налево | таймауту
			if(changePin >= FLASHES_1 || changeDir || tim0_tick > (TIMEOUT/10)) 
			{
				DIR_OFF(1 << RIGHT_OUT | 1 << LEFT_OUT);
				uint16_t i = DELAY_OFF;
				while(i)
				{
					if(PIN_IS_SET(1 << RIGHT_IN)) i = DELAY_OFF;
					else i--;
				}
				state = MODE_OFF;
				timStart = false;
			}
			
		break;	
		
		case MODE_BOTH:
			DIR_ON(1 << RIGHT_OUT | 1 << LEFT_OUT);
			//Выход по: лимиту кол-ва вспышек аварийки | таймауту
			if(changePin >= FLASHES_2 || tim0_tick > (TIMEOUT/10))
			{
				timStart = false;
				DIR_OFF(1 << RIGHT_OUT | 1 << LEFT_OUT);
				while(PIN_IS_SET(1 << RIGHT_IN) || PIN_IS_SET (1 << LEFT_IN));
				state = MODE_OFF;
			}
		break;
		}
	}
}

//Вектор прерывания таймера
ISR (TIM0_COMPA_vect)
{
	if(timStart) 
	{
		tim0_tick++;
	}
}

//Вектор прерывания по изменению состояния пина
ISR (PCINT0_vect)
{
	if(state != MODE_OFF)
	{
		changePin++;
	}
}
