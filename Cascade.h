/*
 * Cascade.h
 *
 *  Created on: Jan 27, 2013
 *      Author: gem
 */

#ifndef CASCADE_H_
#define CASCADE_H_

#include <util.h>

#include "Regulator.h"

struct Data {
	static uint8_t data;
};

template <class R>
class Cascade {
public:
	Cascade(int8_t target) : failCount(0), current(0), regul(target) {}
	void processSensor(const OneWire::Addr& addr, int8_t value) { }
	// returns false on fail
	bool step() {
		if (failCount > 5) {
			R::action_t::down();
			return false;
		}
		regul.step(current);

		failCount++;
		return true;
	}
	template <class S>
	S& log(S& s) const {
		s << "Current: " << current << ", Target: " << regul.getTarget()
			<< ", Action: " << regul.getLastAction();
		return s;
	}
protected:
	uint8_t failCount;
	int8_t current;
	R regul;
};

template <class S, class R>
S& operator<<(S& s, const Cascade<R>& x)
{
	return x.log(s);
}


class RadiatorCascade: public Cascade<Regul<Action<Data, 6, 7>>> {
public:
	typedef Cascade<Regul<Action<Data, 6, 7>>> parent_t;
	RadiatorCascade() :  parent_t(35) {}
	void processSensor(const OneWire::Addr& addr, int8_t value) {
		if (radiatorSensor == addr) {
			failCount = 0;
			current = value;
		} else if (outdoorSensor == addr) {
			int8_t target = 30 - value;
			if (target < 20) target = 20;
			if (target > 70) target = 70;
			regul.setTarget(target);
		}
	}
private:
	OneWire::ConstAddr<0x28, 0xD9, 0xF8, 0xD5, 0x03, 0x00, 0x00, 0xB0> radiatorSensor;
	OneWire::ConstAddr<0x28, 0x0A, 0xFB, 0xD5, 0x03, 0x00, 0x00, 0x63> outdoorSensor;
};

class BoilerCascade: public Cascade<Regul<Action<Data, 4, 5>>> {
public:
	const static uint8_t Target = 75;
	const static uint8_t MaxDelta = 25;
	typedef Cascade<Regul<Action<Data, 4, 5>>> parent_t;
	BoilerCascade() :  parent_t(35), inTemp(0), outTemp(0) {}
	void processSensor(const OneWire::Addr& addr, int8_t value) {
		if (boilerInSensor == addr) {
			inTemp = value;
		} else if (boilerOutSensor == addr) {
			failCount = 0;
			outTemp = value;
		}
	}
	bool step() {
		if (outTemp - inTemp > MaxDelta) {
			// input too cold
			regul.setTarget(min(uint8_t(outTemp - MaxDelta), Target));
			current = inTemp;
		} else {
			regul.setTarget(Target);
			current = max(outTemp, inTemp);
		}

		return parent_t::step();
	}
private:
	OneWire::ConstAddr<0x10, 0x3F, 0x9D, 0x0F, 0x02, 0x08, 0x00, 0xCA> boilerOutSensor;
	OneWire::ConstAddr<0x28, 0x50, 0x05, 0xD6, 0x03, 0x00, 0x00, 0x0E> boilerInSensor;
	int8_t inTemp;
	int8_t outTemp;
};

#endif /* CASCADE_H_ */
