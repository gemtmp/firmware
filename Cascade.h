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
#include "OneWire.h"

template <typename D, int Up, int Down>
class Action {
public:
	static void stop() {
		D::data &= ~((1 << Up) | (1 << Down));
	}
	static void up() {
		D::data &= ~(1 << Down);
		D::data |= (1 << Up);
	}
	static void down() {
		D::data |= (1 << Down);
		D::data &= ~(1 << Up);
	}
};

struct Data {
	static uint8_t data;
};

template <class R, class Action>
class Cascade {
public:
	typedef Action action_t;
	typedef typename R::input_t input_t;
	Cascade(input_t target) : failCount(0), current(0), regul(target) {}
	void processSensor(const OneWire::Addr& addr, input_t value) { }
	// returns false on fail
	bool step() {
		if (failCount > 5) {
			action_t::down();
			return false;
		}
		output_t output = regul.step(current);

		if (output < 0) {
			Action::up();
		} else {
			Action::down();
		}

		failCount++;
		return true;
	}
	template <class S>
	S& log(S& s) const {
		s << "Current: " << current << ", ";
		regul.log(s);
		return s;
	}
	output_t getOutput() const {
		return regul.getOutput();
	}
	output_t getAbsOutput() const {
		output_t v = regul.getOutput();
		return v < 0 ? -v : v;
	}
protected:
	uint8_t failCount;
	input_t current;
	R regul;
};

template <class S, class R, class A>
S& operator<<(S& s, const Cascade<R, A>& x)
{
	return x.log(s);
}

typedef Cascade<Regul<int16_t, 3200, -3200>, Action<Data, 6, 7>> RadiatorCascadeParent;
class RadiatorCascade: public RadiatorCascadeParent {
public:
	typedef RadiatorCascadeParent parent_t;
	using parent_t::input_t;
	static const input_t Min = OneWire::Temperature::toInt(20);
	static const input_t Max = OneWire::Temperature::toInt(70);
	static const input_t Zero = OneWire::Temperature::toInt(32);
	RadiatorCascade() :  parent_t(Zero) {}
	void processSensor(const OneWire::Addr& addr, input_t value) {
		if (radiatorSensor == addr) {
			failCount = 0;
			current = value;
		} else if (outdoorSensor == addr) {
			int16_t target = Zero - value;
			if (target < Min) target = Min;
			if (target > Max) target = Max;
			regul.setTarget(target);
		}
	}
private:
	OneWire::ConstAddr<0x28, 0xD9, 0xF8, 0xD5, 0x03, 0x00, 0x00, 0xB0> radiatorSensor;
	OneWire::ConstAddr<0x28, 0x0A, 0xFB, 0xD5, 0x03, 0x00, 0x00, 0x63> outdoorSensor;
};

typedef Cascade<Regul<int16_t, 3200, -3200>, Action<Data, 4, 5>> BoilerCascadeParent;
class BoilerCascade: public BoilerCascadeParent {
public:
	typedef BoilerCascadeParent parent_t;
	using parent_t::input_t;
	const static input_t Target = OneWire::Temperature::toInt(75);
	const static input_t MaxDelta = OneWire::Temperature::toInt(25);
	BoilerCascade() :  parent_t(Target), inTemp(0), outTemp(0) {}
	void processSensor(const OneWire::Addr& addr, input_t value) {
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
			regul.setTarget(min(outTemp - MaxDelta, Target));
			regul.reset();
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
	input_t inTemp;
	input_t outTemp;
};

#endif /* CASCADE_H_ */
