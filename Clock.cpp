/*
 * Clock.cpp
 *
 *  Created on: Feb 3, 2013
 *      Author: gem
 */


#include <avr/interrupt.h>

#include <timer0.h>
#include <atomic.h>

#include "Clock.h"

// TODO code from arduino

#define clockCyclesPerMicrosecond() ( F_CPU / 1000000L )
#define clockCyclesToMicroseconds(a) ( (a) / clockCyclesPerMicrosecond() )

#define MICROSECONDS_PER_TIMER0_OVERFLOW (clockCyclesToMicroseconds(64 * 256))

// the whole number of milliseconds per timer0 overflow
#define MILLIS_INC (MICROSECONDS_PER_TIMER0_OVERFLOW / 1000)

// the fractional number of milliseconds per timer0 overflow. we shift right
// by three to fit these numbers into a byte. (for the clock speeds we care
// about - 8 and 16 MHz - this doesn't lose precision.)
#define FRACT_INC ((MICROSECONDS_PER_TIMER0_OVERFLOW % 1000) >> 3)
#define FRACT_MAX (1000 >> 3)

static volatile Clock::clock_t mills;
static volatile uint8_t frac;

ISR(TIMER0_OVF_vect)
{
	Clock::clock_t m = mills;
	uint8_t f = frac;

	m += MILLIS_INC;
	f += FRACT_INC;
	if (f > FRACT_MAX) {
		f -= FRACT_MAX;
		++m;
	}
	mills = m;
	frac = f;
}

void Clock::start() {
	Timers::Timer0::Start(Timers::Timer0::Div64);
	Timers::Timer0::EnableInterrupt();
}

Clock::clock_t Clock::millis() {
	Atomic::DisableInterrupts di;
	return mills;
}
