#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include "my_ir_codes.h"

//#define DEBUG
#define TRANSLATE_REPEAT_CODE	// instead of outputting 'repeat code' output the previously recognized IR code

// double buffering
volatile uint16_t pulses_a[NUMPULSES];
volatile uint16_t pulses_b[NUMPULSES];
volatile uint16_t *pulses_write_to = pulses_a;
volatile uint16_t *pulses_read_from = pulses_b;

volatile uint32_t last_IR_activity = 0;

int main(void)
{
	init();			// take care of Arduino timers etc. etc.

	Serial.begin(115200);
	Serial.println(F("Ready to decode IR!"));
	pinMode(13, OUTPUT);
	digitalWrite(13, LOW);
	zero_pulses(pulses_read_from);
	zero_pulses(pulses_write_to);

	DDRB &= ~_BV(PB0);	// PB0 as input (arduino pin #8)
	PORTB |= _BV(PB0);	// pull-up on

	PCICR |= _BV(PCIE0);	// enable pin-change interrupt for pin-group 0
	PCMSK0 |= _BV(PCINT0);	// enable pin-change interrupt por pin PB0 (PCINT0)  

	while (1) {
		static uint16_t pulse_counter = 0;
		if (IR_available()) {
#ifdef DEBUG
			Serial.print(F("\r\n\npulse #: "));
#else
			Serial.print(F("pulse #: "));
#endif
			Serial.print(pulse_counter);
			Serial.print(F(" - "));
			switch (eval_IR_code(pulses_read_from)) {
			case VOL_DOWN:
				Serial.println(F("volume down"));
				break;
			case PLAY_PAUSE:
				Serial.println(F("play/pause"));
				break;
			case VOL_UP:
				Serial.println(F("volume up"));
				break;
			case SETUP_KEY:
				Serial.println(F("setup"));
				break;
			case ARROW_UP:
				Serial.println(F("arrow up"));
				break;
			case STOP_MODE:
				Serial.println(F("stop/mode"));
				break;
			case ARROW_LEFT:
				Serial.println(F("arrow left"));
				break;
			case ENTER_SAVE:
				Serial.println(F("enter/save"));
				break;
			case ARROW_RIGHT:
				Serial.println(F("arrow right"));
				break;
			case DIGIT_0_OR_10:
				Serial.println(F("0 or 10"));
				break;
			case ARROW_DOWN:
				Serial.println(F("arrow down"));
				break;
			case ARROW_REPEAT:
				Serial.println(F("arrow repeat"));
				break;
			case DIGIT_1:
				Serial.println(F("1"));
				break;
			case DIGIT_2:
				Serial.println(F("2"));
				break;
			case DIGIT_3:
				Serial.println(F("3"));
				break;
			case DIGIT_4:
				Serial.println(F("4"));
				break;
			case DIGIT_5:
				Serial.println(F("5"));
				break;
			case DIGIT_6:
				Serial.println(F("6"));
				break;
			case DIGIT_7:
				Serial.println(F("7"));
				break;
			case DIGIT_8:
				Serial.println(F("8"));
				break;
			case DIGIT_9:
				Serial.println(F("9"));
				break;
			case REPEAT_CODE:
				Serial.println(F("repeat code"));
				break;
			case MISMATCH:
				Serial.
				    println(F
					    ("UNKNOWN / CODE MISMATCH - ARGH ARGH ARGH"));
				break;
			case NOT_SURE_YET:
				Serial.println(F("this should never be seen"));
				break;
			default:
				break;
			}
			pulse_counter++;
		}
	}
}

ISR(PCINT0_vect)
{
	PORTB ^= _BV(PB5);	// toggle arduino pin 13
	static uint8_t pulse_counter = 0;
	static uint32_t last_run = 0;
	uint32_t now = micros();
	uint32_t pulse_length = abs(now - last_run);
	if (pulse_length > MAXPULSE) {
		zero_pulses(pulses_write_to);	// clear the buffer after a timeout has occurred
		pulse_counter = 0;
	}
	if (pulse_length < MINPULSE) {
		return;		// got some bouncing ? ignore that.
	}
	if (pulse_counter > 0) {
		pulses_write_to[pulse_counter - 1] =
		    (uint16_t) (pulse_length / 10);
	} else {
		// exit asap
	}
	last_run = micros();
	pulse_counter++;
	if (pulse_counter > NUMPULSES) {
		pulse_counter = 0;
	}
	last_IR_activity = micros();
}

void zero_pulses(volatile uint16_t * array)
{
	uint8_t ctr;
	for (ctr = 0; ctr < NUMPULSES; ctr++) {
		array[ctr] = 0;
	}
}

void flip_buffers(void)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		volatile uint16_t *tmp;
		tmp = pulses_read_from;
		pulses_read_from = pulses_write_to;
		pulses_write_to = tmp;
	}
}

uint8_t IR_available(void)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		if (last_IR_activity != 0
		    && ((micros() - last_IR_activity) > MAXPULSE)) {
			flip_buffers();
			last_IR_activity = 0;
			return 1;
		}
	}
	return 0;
}

IR_code_t eval_IR_code(volatile uint16_t * pulses_measured)
{
	uint8_t ctr1;
	uint8_t ctr2;
#ifdef TRANSLATE_REPEAT_CODE
	static IR_code_t prev_IR_code = NOT_SURE_YET;
#endif
	IR_code_t IR_code;
	for (ctr2 = 0; ctr2 < NUMBER_OF_IR_CODES; ctr2++) {
#ifdef DEBUG
		Serial.print(F("\r\nChecking against array element #: "));
		Serial.println(ctr2);
#endif
		IR_code = NOT_SURE_YET;

		for (ctr1 = 0; ctr1 < NUMPULSES - 6; ctr1++) {
			int16_t measured = (int16_t) (pulses_measured[ctr1]);
			int16_t reference =
			    (int16_t) pgm_read_word(&IRsignals[ctr2][ctr1]);
			uint16_t delta = (uint16_t) abs(measured - reference);
			uint16_t delta_repeat =
			    (uint16_t) abs(measured - REPEAT_CODE_PAUSE);
#ifdef DEBUG
			Serial.print(F("measured: "));
			Serial.print(measured);
			Serial.print(F(" - reference: "));
			Serial.print(reference);
			Serial.print(F(" - delta: "));
			Serial.print(delta);
			Serial.print(F(" - delta_rpt_code: "));
			Serial.print(delta_repeat);
#endif
			if (delta > (reference * FUZZINESS / 100)) {
				if (delta_repeat <
				    REPEAT_CODE_PAUSE * FUZZINESS / 100) {
#ifdef DEBUG
					Serial.println(F
						       (" - repeat code (ok)"));
#endif
					IR_code = REPEAT_CODE;
					break;
				}
#ifdef DEBUG
				Serial.println(F(" - (x)"));
#endif
				IR_code = MISMATCH;
				break;
			} else {
#ifdef DEBUG
				Serial.println(F(" - (ok)"));
#endif
			}
		}
		if (IR_code == REPEAT_CODE) {
#ifdef TRANSLATE_REPEAT_CODE
			IR_code = prev_IR_code;
#endif
			break;
		}
		if (IR_code == NOT_SURE_YET) {
			IR_code = (IR_code_t) (ctr2);
			break;
		}
	}
#ifdef TRANSLATE_REPEAT_CODE
	prev_IR_code = IR_code;
#endif
	zero_pulses(pulses_measured);
	return IR_code;
}
