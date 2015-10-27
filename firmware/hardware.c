#include <avr/io.h>
#include <avr/wdt.h>	// Watchdog timer configuration functions.
#include <avr/power.h>	// Clock prescaler configuration functions.
#include <avr/interrupt.h>
#include <string.h>
#include <stdio.h>
#include <util/delay.h>
		
#include "hardware.h"

// *****************************************************************************
// Function: configure hardware. ***********************************************
// *****************************************************************************
void setupHardware(void)
{	
	// Disable watchdog if enabled by bootloader/fuses. ***********************
	MCUSR &= ~(1 << WDRF);
	wdt_disable();
	
	
	// Configure outputs. *****************************************************
	
	// Configure LEDs. Active high.
	// Orange.
	LED1DDR |= (1 << LED1PIN);
	LED1PORT &= ~(1 << LED1PIN);	//OFF
	// Green.
	LED2DDR |= (1 << LED2PIN);
	LED2PORT &= ~(1 << LED2PIN);	//OFF


	// Stepper ports. Step on rising edge, enable active low.
	// Build stepper enable.
	BUILDENABLEDDR |= (1 << BUILDENABLEPIN);
	BUILDENABLEPORT &= ~(1 << BUILDENABLEPIN);	// Disabled...
	// Build stepper direction.
	BUILDDIRDDR |= (1 << BUILDDIRPIN);
	BUILDDIRPORT |= (1 << BUILDDIRPIN);
	// Build stepper clock.
	BUILDCLOCKDDR |= (1 << BUILDCLOCKPIN);
	BUILDCLOCKPORT &= ~(1 << BUILDCLOCKPIN);
	
	// Tilt stepper enable.	Active high.
	TILTENABLEDDR |= (1 << TILTENABLEPIN);
	TILTENABLEPORT &= ~(1 << TILTENABLEPIN);	// Disabled...
	// Beamer stepper direction.
	TILTDIRDDR |= (1 << TILTDIRPIN);
	TILTDIRPORT |= (1 << TILTDIRPIN);
	// Beamer stepper clock.
	TILTCLOCKDDR |= (1 << TILTCLOCKPIN);
	TILTCLOCKPORT &= ~(1 << TILTCLOCKPIN);		// Low...
	
	// Servo port.
	SERVOCLOCKDDR |= (1 << SERVOCLOCKPIN);
	SERVOCLOCKPORT |= (1 << SERVOCLOCKPIN);
	

	// Configure inputs. ******************************************************
	
	// Limit switches using internal pull-ups. Configured as inputs by default.
	LIMITBUILDTOPPORT |= (1 << LIMITBUILDTOPPIN);		// Build platform top.
	LIMITBUILDBOTTOMPORT |= (1 << LIMITBUILDBOTTOMPIN);	// Build platform bottom.
	LIMITTILTPORT |= (1 << LIMITTILTPIN);				// Tilt.

	// Configure INT1--3 to fire on falling edge.
	EICRA |= (1 << ISC11 | 1 << ISC10 | 1 << ISC01 | 1 << ISC00);	// Fire on rising edge. Datasheed p. 86.
	EICRB |= (1 << ISC61 | 1 << ISC60);
	EIMSK |= (1 << INT6 | 1 << INT1 | 1 << INT0);

	// Configure PCINT4 for tilt.
//	PCICR |= (1 << PCIE0);
//	PCMSK0 |= (1 << PCINT4);
	

	// Configure timer0. Call ISR every 0,1 ms. *******************************
	// Prescaler:	16.000.000 Hz / 8 = 2.000.000 Hz
  	//			1 s / 2.000.000 = 0,0000005 s per clock cycle.
  	// Clock cycles per millisecond:
  	//			0,001 s / 0,0000005 s = 2.000 clock cycles per millisecond.
  	//			Overflow at 200 clock cycles comes every 0,1 ms.
  	TCCR0A |= (1 << WGM01);	// CTC mode. Data sheet page 104.
  	TCCR0B |= (1<<CS01);	// Set prescaler to 8. Data sheet page 106.
  	OCR0A = 200;		// Set channel A compare value.
  	TIMSK0 |= (1<<OCIE0A);	// Enable channel A compare interrupt.


	// Stepper PWM for beamer drive. ******************************************
	// Configure timer1 for CTC with output toggle on OC1A and interrupt. Prescaler 1.
	TCCR1A |= (1 << COM1A0);		// Toggle OC1A on compare match.
	TCCR1B |= (1 << WGM12);			// CTC with OCR1AH/OCR1AL compare registers. Data sheet page 130.
//	TCCR1B |= (1 << CS10);			// Set prescaler to 1. Datasheet page 133.				Do this later...
	TIMSK1 |= (1 << OCIE1A);		// Enable channel A CTC interrupt. Datasheet p. 136.



	// Stepper PWM for build platform drive. **********************************
	// Configure timer3 for CTC with interrupt on compare match. Prescaler 1.
	TCCR3A |= (1 << COM3A0);		// Toggle OC3A on compare match.
	TCCR3B |= (1 << WGM32);		// CTC with OCR3AH/OCR3AL compare registers. Data sheet page 130.
//	TCCR3B |= (1 << CS30);		// Set prescaler to 1. Datasheet page 133. 				DO NOT ENABLE BY DEFAULT!
	TIMSK3 |= (1 << OCIE3A);		// Enable channel A CTC interrupt. Datasheet p. 136.
	
	
	// Stepper PWM for tilt. **************************************************
	// TIMER 4 IS A BITCH!
	// SUPPOSEDLY ONLY OCR4C CAN SERVE AS TOP THAT RESETS ON COMPARE MATCH! Datasheet p 149 under Normal Mode.
	// Thus, make sure to set OCR4A and OCR4B to the same value to use OC4A output pin.
	// Configure timer4 in 8 bit mode for CTC with interrupt on compare match. Prescaler 64.
//	TCCR4A |= (1 << COM4A0);				// Toggle OC4A on compare match. Datasheet p 162.
//	TCCR4B |= (1 << CS42 | 1 << CS41 | 1 << CS40);		// Prescaler 64. Datasheet p 166. 			DISABLED! Enable in menu function.
	TIMSK4 |= (1 << OCIE4A);
	
	
	// Inititalise USB using LUFA function. ***********************************
	USB_Init();
	
	// Initialise LCD. ********************************************************
//	lcd_init(0x0C);		// LCD on, cursor off: see lcd.h.
//	lcd_clrscr();		// Clear and set cursor to home.
	
	// Initialise button. *****************************************************
//	buttonInit();
	
	// Initialise rotary encoder. *********************************************
//	rotaryEncoderInit();
	
}


// Define functions to set timer1 and timer3 compare values (two 8 bit registers OCR1AH / OCR1AL ).
// See data sheet p 110 for 16 bit register access.
// Function is needed to make sure that no interrupt kicks in between writing the two 8 bit registers.
void timer1SetCompareValue( uint16_t input )
{
	// Save global interrupt flag.
	uint8_t sreg;
	sreg = SREG;
	// Disable interrupts.
	cli();
	//Set TCNTn to input.
	OCR1A = input;
	// Restore global interrupt flag
	SREG = sreg;	// Makes sei() unneccessary.
}

void timer3SetCompareValue( uint16_t input )
{
	// Save global interrupt flag.
	uint8_t sreg;
	sreg = SREG;
	// Disable interrupts.
	cli();
	//Set TCNTn to input.
	OCR3A = input;
	// Restore global interrupt flag.
	SREG = sreg;	// Makes sei() unneccessary.
}

void timer4SetCompareValue( uint8_t input )
{
// NOTE: For timer 4 only OCR4C generates a reset on compare match, but only channels A, B and D have an interrupt...
//	// Save global interrupt flag.
//	uint8_t sreg;
//	sreg = SREG;
//	// Disable interrupts.
//	cli();
//	//Set TCNTn to input.
	OCR4A = input-5;
	OCR4C = input;
//	// Restore global interrupt flag.
//	SREG = sreg;	// Makes sei() unneccessary.
}

void ledYellowOn( void )
{
	LED1PORT |= (1 << LED1PIN);
}

void ledYellowOff( void )
{
	LED1PORT &= ~(1 << LED1PIN);
}

void ledYellowToggle( void )
{
	LED1PORT ^= (1 << LED1PIN);
}

void ledGreenOn( void )
{
	LED2PORT |= (1 << LED2PIN);
}

void ledGreenOff( void )
{
	LED2PORT &= ~(1 << LED2PIN);
}

void ledGreenToggle( void )
{
	LED2PORT ^= (1 << LED2PIN);
}


/*
#include <avr/io.h>
#include <avr/wdt.h>	// Watchdog timer configuration functions.
#include <avr/power.h>	// Clock prescaler configuration functions.
#include <avr/interrupt.h>
#include <string.h>
#include <stdio.h>
#include <util/delay.h>
		
#include "hardware.h"

// *****************************************************************************
// Function: configure hardware. ***********************************************
// *****************************************************************************
void setupHardware(void)
{	
	// Disable watchdog if enabled by bootloader/fuses. ***********************
	MCUSR &= ~(1 << WDRF);
	wdt_disable();
	
	
	// Configure outputs. *****************************************************
	
	// Configure LEDs. Active high.
	// Yellow 1: PB0. USED FOR LCD DATA LINE!
//	LED1DDR |= (1 << LED1PIN);
//	LED1PORT |= (1 << LED1PIN);
	// Yellow 2: PD5.
//	LED2DDR |= (1 << LED2PIN);
//	LED2PORT |= (1 << LED2PIN);
	LEDDDR |= (1 << LEDPIN);
	LEDPORT &= ~(1 << LEDPIN);	//OFF
	// Green: PC7. SAME AS TILT STEPPER CLOCK!
//	LED3DDR |= (1 << LED3PIN);
//	LED3PORT |= (1 << LED3PIN);

	// Stepper ports. Step on rising edge, enable active low.
	// Build stepper enable.
	BUILDENABLEDDR |= (1 << BUILDENABLEPIN);
	BUILDENABLEPORT |= (1 << BUILDENABLEPIN);	// Disabled...
	// Build stepper direction.
	BUILDDIRDDR |= (1 << BUILDDIRPIN);
	BUILDDIRPORT |= (1 << BUILDDIRPIN);
	// Build stepper clock.
	BUILDCLOCKDDR |= (1 << BUILDCLOCKPIN);
	BUILDCLOCKPORT &= ~(1 << BUILDCLOCKPIN);
	
	// Beamer stepper enable.
	BEAMERENABLEDDR |= (1 << BEAMERENABLEPIN);
	BEAMERENABLEPORT |= (1 << BEAMERENABLEPIN);	// Disabled...
	// Beamer stepper direction.
	BEAMERDIRDDR |= (1 << BEAMERDIRPIN);
	BEAMERDIRPORT |= (1 << BEAMERDIRPIN);
	// Beamer stepper clock.
	BEAMERCLOCKDDR |= (1 << BEAMERCLOCKPIN);
	BEAMERCLOCKPORT &= ~(1 << BEAMERCLOCKPIN);	// Low...
	
	// Tilt stepper enable.	Active low.
	TILTENABLEDDR |= (1 << TILTENABLEPIN);
	TILTENABLEPORT |= (1 << TILTENABLEPIN);	// Disabled...
	// Beamer stepper direction.
	TILTDIRDDR |= (1 << TILTDIRPIN);
	TILTDIRPORT |= (1 << TILTDIRPIN);
	// Beamer stepper clock.
	TILTCLOCKDDR |= (1 << TILTCLOCKPIN);
	TILTCLOCKPORT &= ~(1 << TILTCLOCKPIN);		// Low...
	

	// Configure inputs. ******************************************************
	
	// Limit switches using internal pull-ups. Configured as inputs by default.
	LIMITBUILDTOPPORT |= (1 << LIMITBUILDTOPPIN);		// Build platform top.
	LIMITBUILDBOTTOMPORT |= (1 << LIMITBUILDBOTTOMPIN);	// Build platform bottom.
	LIMITBEAMERTOPPORT |= (1 << LIMITBEAMERTOPPIN);		// Beamer top.
	LIMITBEAMERBOTTOMPORT |= (1 << LIMITBEAMERBOTTOMPIN);	// Beamer bottom.
	LIMITTILTPORT |= (1 << LIMITTILTPIN);				// Tilt.

	// Configure INT1--3 to fire on falling edge.
	EICRA |= (1 << ISC31 | 1 << ISC21 | 1 << ISC11);	// Fire on falling edge. Datasheed p. 86.
	EICRB |= (1 << ISC61);
	EIMSK |= (1 << INT6 | 1 << INT3 | 1 << INT2 | 1 << INT1);

	// Configure PCINT4 for tilt.
	PCICR |= (1 << PCIE0);
	PCMSK0 |= (1 << PCINT4);
	

	// Configure timer0. Call ISR every 0,1 ms. *******************************
	// Prescaler:	16.000.000 Hz / 8 = 2.000.000 Hz
  	//			1 s / 2.000.000 = 0,0000005 s per clock cycle.
  	// Clock cycles per millisecond:
  	//			0,001 s / 0,0000005 s = 2.000 clock cycles per millisecond.
  	//			Overflow at 200 clock cycles comes every 0,1 ms.
  	TCCR0A |= (1 << WGM01);	// CTC mode. Data sheet page 104.
  	TCCR0B |= (1<<CS01);	// Set prescaler to 8. Data sheet page 106.
  	OCR0A = 200;		// Set channel A compare value.
  	TIMSK0 |= (1<<OCIE0A);	// Enable channel A compare interrupt.


	// Stepper PWM for beamer drive. ******************************************
	// Configure timer1 for CTC with output toggle on OC1A and interrupt. Prescaler 1.
	TCCR1A |= (1 << COM1A0);		// Toggle OC1A on compare match.
	TCCR1B |= (1 << WGM12);			// CTC with OCR1AH/OCR1AL compare registers. Data sheet page 130.
//	TCCR1B |= (1 << CS10);			// Set prescaler to 1. Datasheet page 133.				Do this later...
	TIMSK1 |= (1 << OCIE1A);		// Enable channel A CTC interrupt. Datasheet p. 136.



	// Stepper PWM for build platform drive. **********************************
	// Configure timer3 for CTC with interrupt on compare match. Prescaler 1.
	TCCR3A |= (1 << COM3A0);		// Toggle OC3A on compare match.
	TCCR3B |= (1 << WGM32);			// CTC with OCR3AH/OCR3AL compare registers. Data sheet page 130.
//	TCCR3B |= (1 << CS30);			// Set prescaler to 1. Datasheet page 133. 				DO NOT ENABLE BY DEFAULT!
	TIMSK3 |= (1 << OCIE3A);		// Enable channel A CTC interrupt. Datasheet p. 136.
	
	
	// Stepper PWM for tilt. **************************************************
	// TIMER 4 IS A BITCH!
	// SUPPOSEDLY ONLY OCR4C CAN SERVE AS TOP THAT RESETS ON COMPARE MATCH! Datasheet p 149 under Normal Mode.
	// Thus, make sure to set OCR4A and OCR4B to the same value to use OC4A output pin.
	// Configure timer4 in 8 bit mode for CTC with interrupt on compare match. Prescaler 64.
//	TCCR4A |= (1 << COM4A0);				// Toggle OC4A on compare match. Datasheet p 162.
//	TCCR4B |= (1 << CS42 | 1 << CS41 | 1 << CS40);		// Prescaler 64. Datasheet p 166. 			DISABLED! Enable in menu function.
	TIMSK4 |= (1 << OCIE4A);
	
	
	// Inititalise USB using LUFA function. ***********************************
	USB_Init();
	
	// Initialise LCD. ********************************************************
	lcd_init(0x0C);		// LCD on, cursor off: see lcd.h.
	lcd_clrscr();		// Clear and set cursor to home.
	
	// Initialise button. *****************************************************
	buttonInit();
	
	// Initialise rotary encoder. *********************************************
	rotaryEncoderInit();
	
}


// Define functions to set timer1 and timer3 compare values (two 8 bit registers OCR1AH / OCR1AL ).
// See data sheet p 110 for 16 bit register access.
// Function is needed to make sure that no interrupt kicks in between writing the two 8 bit registers.
void timer1SetCompareValue( uint16_t input )
{
	// Save global interrupt flag.
	uint8_t sreg;
	sreg = SREG;
	// Disable interrupts.
	cli();
	//Set TCNTn to input.
	OCR1A = input;
	// Restore global interrupt flag
	SREG = sreg;	// Makes sei() unneccessary.
}

void timer3SetCompareValue( uint16_t input )
{
	// Save global interrupt flag.
	uint8_t sreg;
	sreg = SREG;
	// Disable interrupts.
	cli();
	//Set TCNTn to input.
	OCR3A = input;
	// Restore global interrupt flag.
	SREG = sreg;	// Makes sei() unneccessary.
}

void timer4SetCompareValue( uint8_t input )
{
// NOTE: For timer 4 only OCR4C generates a reset on compare match, but only channels A, B and D have an interrupt...
//	// Save global interrupt flag.
//	uint8_t sreg;
//	sreg = SREG;
//	// Disable interrupts.
//	cli();
//	//Set TCNTn to input.
	OCR4A = input-5;
	OCR4C = input;
//	// Restore global interrupt flag.
//	SREG = sreg;	// Makes sei() unneccessary.
}
*/