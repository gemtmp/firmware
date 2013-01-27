/*
 * TWI.h
 *
 *  Created on: Jan 26, 2013
 *      Author: gem
 */

#ifndef TWI_H_
#define TWI_H_

#include <avr/io.h>
#include <util/twi.h>

class TWI {
public:
	static void init() {
		//set SCL to 100kHz
		TWSR = 0x00;
		TWBR = 72;
		TWCR = (1<<TWEN);
	}
	static void disable() {
		TWCR = 0;
	}
	static void start() {
		TWCR = (1<<TWINT)|(1<<TWSTA)|(1<<TWEN);
		wait();
	}
	static void stop() {
		TWCR = (1<<TWINT)|(1<<TWSTO)|(1<<TWEN);
		while (TWCR & _BV(TWSTO));
	}
	static void write(uint8_t data) {
		TWDR = data;
		TWCR = (1<<TWINT)|(1<<TWEN);
		wait();
	}
	static bool write(uint8_t addr, uint8_t data) {
		start();
		if (status() != TW_START)
			return false;
		write(addr);
		if (status() != TW_MT_SLA_ACK)
			return false;
		write(data);
		if (status() != TW_MT_DATA_ACK)
			return false;
		stop();
		return true;
	}
	static uint8_t status() {
		uint8_t status;
		status = TWSR & 0xF8;
		return status;
	}
private:
	static void wait() {
		while ((TWCR & (1<<TWINT)) == 0);
	}
};


#endif /* TWI_H_ */
