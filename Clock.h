/*
 * Clock.h
 *
 *  Created on: Feb 3, 2013
 *      Author: gem
 */

#ifndef CLOCK_H_
#define CLOCK_H_


class Clock {
public:
	typedef uint16_t clock_t;
	typedef void (*func)();

	static void start();
	static clock_t millis();
};


#endif /* CLOCK_H_ */
