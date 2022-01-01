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
#define TIMEOUT			5000			//Таймаут выключения направления если что-то пошло не так, мс
#define DELAY_SCAN		50				//Задержка перед сканированием направления, мс
#define DELAY_OFF		250				//Задержка перед переходом в MODE_OFF, мс
#define DELAY_ON		20				//Задержка включения направления, мс
#define FLASHES_1		5				//Кол-во вспышек поворотников (x+1)/2
#define FLASHES_2		3				//Кол-во вспышек аварийкой	(x+1)/2

typedef enum
{
	MODE_OFF,
	MODE_LEFT,
	MODE_RIGHT,
	MODE_BOTH,
	MODE_PREOFF,	
}STATE;

STATE state = MODE_OFF;				//Состояние логического автомата
volatile uint8_t chgCntr = 0;		//Счётчик изменений состояния пина обратной связи
volatile uint16_t tim_ms = 0;		//Время, мс

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
	
	//----------------- Инициализация таймера TC0 -------------------
	TCNT0 = 0x00;									//Сброс счётного регистра
	OCR0A = 126;									//Установка регистра сравнения A
	OCR0B = 0x00;									//Сброс регистра сравнения B
	TCCR0A = (0<<COM0A1) | (0<<COM0A0) |			//OC0A - отключён
			 (0<<COM0B1) | (0<<COM0B0) |			//OC0B - отключён
			 (1<<WGM01) | (0<<WGM00);				//Режим работы таймера (сброс при совпадении с OCR0A)
	TCCR0B = (0<<WGM02) |							//Режим работы таймера
			 (0<<CS02) | (1<<CS01) | (0<<CS00);		//Запуск таймера с делителем 8
	TIMSK0 = (0<<OCIE0B) |							//Прерывание по сравнению с регистром сравнения B
			 (1<<OCIE0A) |							//Прерывание по сравнению с регистром сравнения A
			 (0<<TOIE0);							//Прерывание по переполнению
}

uint16_t msTime()
{
	cli();
	uint16_t tm = tim_ms;
	sei();
	return tm;
}
void msTimeClr()
{
	cli();
	tim_ms = 0;
	sei();
}

int main(void)
{
	mcuInit();
	bool changeDir = false;
	DIR_OFF(1 << LEFT_OUT | 1 << RIGHT_OUT);	
	sei();

	while (1) 
	{
		switch (state)
		{
		case MODE_OFF:
			//chgCntr = 0;
			//DIR_OFF(1 << LEFT_OUT | 1 << RIGHT_OUT);
			//Ожидание активной работы модуля (пока FB_IN==0 модуль не активен)
			while(PIN_IS_CLER(1 << FB_IN));
			_delay_ms(DELAY_SCAN);
			//Проверка входящего сигнала налево
			if(PIN_IS_SET(1 << LEFT_IN) && PIN_IS_CLER (1 << RIGHT_IN))	
			{	
				_delay_ms(DELAY_ON);
				if(PIN_IS_SET(1 << LEFT_IN) && PIN_IS_CLER (1 << RIGHT_IN))
				{
					state = MODE_LEFT;
					DIR_ON(1 << LEFT_OUT);
					chgCntr = 0;
					msTimeClr();
				}
			}
			//Проверка входящего сигнала направо
			if(PIN_IS_SET(1 << RIGHT_IN) && PIN_IS_CLER (1 << LEFT_IN))
			{
				_delay_ms(DELAY_ON);
				if(PIN_IS_SET(1 << RIGHT_IN) && PIN_IS_CLER (1 << LEFT_IN))
				{
					state = MODE_RIGHT;
					DIR_ON(1 << RIGHT_OUT);
					chgCntr = 0;
					msTimeClr();
				}
			}
			//Проверка одновременного срабатывания налево и направо (аварийка)
			if(PIN_IS_SET(1<<LEFT_IN) && PIN_IS_SET(1<<RIGHT_IN))
			{
				_delay_ms(DELAY_ON);
				if(PIN_IS_SET(1<<LEFT_IN) && PIN_IS_SET(1<<RIGHT_IN))
				{
					state = MODE_BOTH;
					DIR_ON(1 << RIGHT_OUT | 1 << LEFT_OUT);
					chgCntr = 0;
					msTimeClr();
				}
			}
		break;
		
		case MODE_LEFT:
			//Проверка смены направления			
			changeDir = PIN_IS_SET(1<<FB_IN) && PIN_IS_SET(1<<RIGHT_IN);
			_delay_ms(5);
			changeDir &= PIN_IS_SET(1<<FB_IN) && PIN_IS_SET(1<<RIGHT_IN);
			
			//Выход по: лимит кол-ва вспышек налево | смене направления | таймауту
			if(chgCntr >= FLASHES_1 || changeDir || msTime() > TIMEOUT)
			{
				state = MODE_PREOFF;
				msTimeClr();
			}
		break;
		
		case MODE_RIGHT:
			//Проверка смены направления
			changeDir = PIN_IS_SET(1<<FB_IN) && PIN_IS_SET(1<<LEFT_IN);
			_delay_ms(5);
			changeDir &= PIN_IS_SET(1<<FB_IN) && PIN_IS_SET(1<<LEFT_IN);
			
			//Выход по: лимит кол-ва вспышек направо | смене напрвыления | таймауту
			if(chgCntr >= FLASHES_1 || changeDir || msTime() > TIMEOUT) 
			{
				state = MODE_PREOFF;
				msTimeClr();
			}
		break;	
		
		case MODE_BOTH:
			//Выход по: лимиту кол-ва вспышек аварийки | таймауту
			if(chgCntr >= FLASHES_2 || msTime() > TIMEOUT)
			{
				state = MODE_PREOFF;
				msTimeClr();
			}
		break;
		
		case MODE_PREOFF:
			DIR_OFF(1 << LEFT_OUT | 1 << RIGHT_OUT);
			//Ожидание полной неактивности модуля поворотников
			while(msTime() < DELAY_OFF)
			{
				if(PIN_IS_SET(1<<LEFT_IN) || PIN_IS_SET(1<<RIGHT_IN) || PIN_IS_SET(1<<FB_IN))
				{
					msTimeClr();
				}
			}
			state = MODE_OFF;
		break;
		}
	}
}

//Вектор прерывания таймера
ISR (TIM0_COMPA_vect)
{
	tim_ms++;
}

//Вектор прерывания по изменению состояния пина
ISR (PCINT0_vect)
{
	chgCntr++;
}
