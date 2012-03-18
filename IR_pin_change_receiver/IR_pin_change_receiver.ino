#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include "my_ir_codes.h"
#define IRpin_PIN      PINB
#define IRpin          0

#define MAXPULSE 65000U
#define MINPULSE 5U
#define REPEAT_CODE_PAUSE 250 // this has to be signed !
#define NUMPULSES 72U
#define FUZZINESS 20U

//#define DEBUG

// double buffering
uint16_t pulses_a[NUMPULSES];
uint16_t pulses_b[NUMPULSES];
uint16_t *pulses_write_to = pulses_a;
uint16_t *pulses_read_from = pulses_b;

uint32_t last_IR_activity = 0;

int main(void) {
  
        init();
        
	Serial.begin(115200);
	Serial.println("Ready to decode IR!");
	pinMode(13, OUTPUT);
	digitalWrite(13, LOW);
	zero_pulses(pulses_read_from);
	zero_pulses(pulses_write_to);
        
        DDRB &= ~_BV(PB0); // PB0 as input (arduino pin #8)
        PORTB |= _BV(PB0); // pull-up on
        
	PCICR |= _BV(PCIE0);	// enable pin-change interrupt for pin-group 0
	PCMSK0 |= _BV(PCINT0);	// enable pin-change interrupt por pin PB0 (PCINT0)  
          
        while(1) {
	        static uint16_t pulse_counter = 0;
        	if (IR_available()) {
	        	Serial.print("pulse #: ");
        		Serial.print(pulse_counter);
                        switch (eval_IR_code(pulses_read_from, IRsignal_vol_down)) {
                                case 1:
                                        Serial.print(" - volume down");
                                break;
                                case 2:
                                        Serial.print(" - repeat code");
                                default:
                                break;
                        }
                        switch (eval_IR_code(pulses_read_from, IRsignal_vol_up)) {
                                case 1:
                                        Serial.print(" - volume up");
                                break;
                                case 2:
                                        Serial.print(" - repeat code");
                                default:
                                break;
                        }
                        switch (eval_IR_code(pulses_read_from, IRsignal_stop_mode)) {
                                case 1:
                                        Serial.print(" - stop/mode");
                                break;
                                case 2:
                                        Serial.print(" - repeat code");
                                default:
                                break;
                        }                        
                        switch (eval_IR_code(pulses_read_from, IRsignal_play_pause)) {
                                case 1:
                                        Serial.print(" - play/pause");
                                break;
                                case 2:
                                        Serial.print(" - repeat code");
                                default:
                                break;
                        }                  
                        Serial.println("");
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

void zero_pulses(uint16_t * array)
{
	uint8_t ctr;
	for (ctr = 0; ctr < NUMPULSES; ctr++) {
		array[ctr] = 0;
	}
}

void flip_buffers(void)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		uint16_t *tmp;
		tmp = pulses_read_from;
		pulses_read_from = pulses_write_to;
		pulses_write_to = tmp;
	}
}

uint8_t IR_available()
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		if (last_IR_activity != 0
		    && ((micros() - last_IR_activity) > MAXPULSE)) {
			flip_buffers();
			last_IR_activity = 0;
			return 1;
		}
	}
}

uint8_t eval_IR_code(uint16_t * pulses_measured,
		     const uint16_t * pulses_reference)
{
	uint8_t ctr;
#ifdef DEBUG
	Serial.println("\r\nstart:");
#endif
	for (ctr = 0; ctr < NUMPULSES-6; ctr++) {
		int16_t measured = (int16_t) (pulses_measured[ctr]);
		int16_t reference =
		    (int16_t) (pgm_read_word(&pulses_reference[ctr]));
		uint16_t delta = (uint16_t) abs(measured - reference);
                uint16_t delta_repeat = (uint16_t) abs(measured - REPEAT_CODE_PAUSE);
#ifdef DEBUG
		Serial.print("measured: ");
		Serial.print(measured);
		Serial.print(" - reference: ");
		Serial.print(reference);
		Serial.print(" - delta: ");
		Serial.print(delta);
                Serial.print(" - delta_rpt_code: ");
                Serial.print(delta_repeat);
#endif
		if ( delta > (reference * FUZZINESS / 100) ) {
                        if( delta_repeat < REPEAT_CODE_PAUSE * FUZZINESS / 100) {
#ifdef DEBUG                          
                                Serial.println(" - repeat code (ok)");
#endif                                
                                zero_pulses(pulses_measured);
                                return 2;
                        }
#ifdef DEBUG
			Serial.println(" - (x)");
#endif
                        //
                        // prints the whole failed IR data
                        //
                        //print_pulse(pulses_measured, pulses_reference); // print failed IR data
			return 0;
		} else {
#ifdef DEBUG
			Serial.println(" - (ok)");
#endif
		}
	}
	zero_pulses(pulses_measured);
	return 1;
}

void print_pulse(uint16_t * pulses_measured, const uint16_t * pulses_reference) {
        uint8_t ctr;
      	Serial.println("\r\nstart:");
	for (ctr = 0; ctr < NUMPULSES-6; ctr++) {
		int16_t measured = (int16_t) (pulses_measured[ctr]);
		int16_t reference =
		    (int16_t) (pgm_read_word(&pulses_reference[ctr]));
		uint16_t delta = (uint16_t) abs(measured - reference);
		Serial.print("measured: ");
		Serial.print(measured);
		Serial.print(" - reference: ");
		Serial.print(reference);
		Serial.print(" - delta: ");
        	Serial.print(delta);  

		if (delta > (reference * FUZZINESS / 100)) {
			Serial.println(" - (x)");

		} else {
			Serial.println(" - (ok)");
		}        
        }
}
