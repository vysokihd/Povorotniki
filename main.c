/*
 * Povorotniki.c
 *
 * Created: 01.03.2020 20:24:02
 * Author : Dmitry
 */ 
#define F_CPU 1200000UL 

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
//--------------------- Макросы -------------------------
#define PIN_IS_SET(P)		((PINB & (P)) != 0)
#define PIN_IS_CLER(P)		((PINB & (P)) == 0)
#define DIR_ON(P)			PORTB &= ~(P)
#define DIR_OFF(P)			PORTB |= (P)

//------------------ Тайминги ---------------------------
#define DELAY_OFF		50
#define DELAY_ON		20				//Задержка включения направления, мс
#define FLASHES_1		5				//Кол-во вспышек поворотников
#define FLASHES_2		3				//Кол-во вспышек аварийкой

typedef enum
{
	MODE_OFF,
	MODE_LEFT,
	MODE_RIGHT,
	MODE_BOTH	
}STATE;

STATE state = MODE_OFF;				//Состояние логического автомата
//volatile uint16_t tim0_tick = 0;	//Таймерная переменная
//volatile bool timStart = false;		//Запуск таймерной переменной
volatile uint8_t chgCntr = 0;		//Счётчик изменений состояния пина обратной связи

static inline void mcuInit()
{
	//----------- Инициализация портов ввода вывода ----------
	PORTB  = (1 << LEFT_OUT) | (1 << RIGHT_OUT);
	DDRB = (1 << LEFT_OUT) | (1 << RIGHT_OUT);	
	
	//----------- Инициализация внешнего прерывания -------------
	MCUCR = (0 << ISC01) | (0 << ISC00);		//Прерывание по: 00 - низкому уровню, 01 - изменению состояния, 10 - отриц. фронту, 11 - полож. фронту
	PCMSK = (0 << PCINT5) | (0 << PCINT4) | (1 << PCINT3) | (0 << PCINT2) | (0 << PCINT1) | (0 << PCINT0); //Прерывание по пину PCINT0..PCINT5
	GIFR = (0 << INTF0) | (1 << PCIF);			//Регистр флагов прерываний INT0 и PCINT0..PCINT5, 1 - сброс флага перывания
	GIMSK = (0 << INT0) | (1 << PCIE);			//Разрешение прерывание INT0, PCINT0..PCINT5
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
			chgCntr = 0;
			DIR_OFF(1 << LEFT_OUT | 1 << RIGHT_OUT);
			//Ожидание активной работы модуля
			while(PIN_IS_CLER(1 << FB_IN));
			_delay_ms(50);
			//Проверка входящего сигнала налево
			if(PIN_IS_SET(1 << LEFT_IN) && PIN_IS_CLER (1 << RIGHT_IN))	
			{	
				_delay_ms(DELAY_ON);
				if(PIN_IS_SET(1 << LEFT_IN) && PIN_IS_CLER (1 << RIGHT_IN))
				{
					state = MODE_LEFT;
				}
			}
			//Проверка входящего сигнала направо
			if(PIN_IS_SET(1 << RIGHT_IN) && PIN_IS_CLER (1 << LEFT_IN))
			{
				_delay_ms(DELAY_ON);
				if(PIN_IS_SET(1 << RIGHT_IN) && PIN_IS_CLER (1 << LEFT_IN))
				{
					state = MODE_RIGHT;
				}
			}
			//Проверка одновременного срабатывания налево и направо (аварийка)
			if(PIN_IS_SET(1 << LEFT_IN) && PIN_IS_SET(1 << RIGHT_IN))
			{
				_delay_ms(DELAY_ON);
				if(PIN_IS_SET(1 << LEFT_IN) && PIN_IS_SET(1 << RIGHT_IN))
				{
					state = MODE_BOTH;
				}
			}
		break;
		
		case MODE_LEFT:
			//Удержание направления налево			
			DIR_ON(1 << LEFT_OUT);
			changeDir = PIN_IS_SET(1 << FB_IN) && PIN_IS_SET(1 << RIGHT_IN);
			_delay_ms(10);
			changeDir &= PIN_IS_SET(1 << FB_IN) && PIN_IS_SET(1 << RIGHT_IN);
			
			//Выход по: лимит кол-ва вспышек налево | сигналу направо
			if(chgCntr >= FLASHES_1 || changeDir)
			{
				DIR_OFF(1 << LEFT_OUT | 1 << RIGHT_OUT);
				while(PIN_IS_SET (1 << LEFT_IN));
				if(changeDir)
				{
					state = MODE_RIGHT;
					chgCntr = 0;
				}
				else state = MODE_OFF;
				//_delay_ms(DELAY_OFF);
			}
		break;
		
		case MODE_RIGHT:
			//Удержание направления направо
			DIR_ON(1 << RIGHT_OUT);
			changeDir = PIN_IS_SET(1 << FB_IN) && PIN_IS_SET(1 << LEFT_IN);
			_delay_ms(10);
			changeDir &= PIN_IS_SET(1 << FB_IN) && PIN_IS_SET(1 << LEFT_IN);
			
			//Выход по: лимит кол-ва вспышек направо | сигналу налево | таймауту
			if(chgCntr >= FLASHES_1 || changeDir) 
			{
				DIR_OFF(1 << RIGHT_OUT | 1 << LEFT_OUT);
				while(PIN_IS_SET(1 << RIGHT_IN));
				if(changeDir)
				{
					state = MODE_LEFT;
					chgCntr = 0;
				}
				else state = MODE_OFF;
				//_delay_ms(DELAY_OFF);
			}
		break;	
		
		case MODE_BOTH:
			DIR_ON(1 << RIGHT_OUT | 1 << LEFT_OUT);
			//Выход по: лимиту кол-ва вспышек аварийки | таймауту
			if(chgCntr >= FLASHES_2)
			{
				DIR_OFF(1 << RIGHT_OUT | 1 << LEFT_OUT);
				while(PIN_IS_SET(1 << RIGHT_IN) || PIN_IS_SET (1 << LEFT_IN));
				state = MODE_OFF;
				//_delay_ms(DELAY_OFF);
			}
		break;
		}
	}
}

//Вектор прерывания таймера
ISR (TIM0_COMPA_vect)
{
	//if(timStart) 
	//{
		//tim0_tick++;
	//}
}

//Вектор прерывания по изменению состояния пина
ISR (PCINT0_vect)
{
	if(state != MODE_OFF)
	{
		chgCntr++;
	}
}
