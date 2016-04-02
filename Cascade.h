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

template <typename D, int Up, int Down = 0>
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

template <typename D, int On>
class Action<D, On, 0> {
public:
	static void stop() {
		D::data &= ~(1 << On);
	}
	static void start() {
		D::data |= (1 << On);
	}
	static bool status() {
		return (D::data & (1 << On)) != 0;
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

template <class S, class T, typename T::input_t dummy = 0>
S& operator<<(S& s, const T& x)
{
	return x.log(s);
}

typedef Cascade<Regul<int16_t, 4000, -4000>, Action<Data, 6, 7>> RadiatorCascadeParent;
class RadiatorCascade: public RadiatorCascadeParent {
public:
	typedef RadiatorCascadeParent parent_t;
	using parent_t::input_t;
	static const input_t Min = Temperature::toInt(22);
	static const input_t Max = Temperature::toInt(70);
	static const input_t Zero = Temperature::toInt(50);
	static const input_t k = 2;
	static const input_t IndoorTarget = Temperature::toInt(22);
	// target temperature is (Zero - outdoor) / k + (IndoorTarget - indoor) * 4,
	// limited by Min and Max

	static const input_t Fail = Temperature::toInt(85);

	RadiatorCascade() :  parent_t(Zero), indoor(IndoorTarget) {}
	void processSensor(const OneWire::Addr& addr, input_t value) {
		if (radiatorSensor == addr) {
			if (value == Fail && current < Temperature::toInt(60)) {
				// problem with power level on ds1820, skip value
				return;
			}
			failCount = 0;
			current = value;
		} else if (indoorSensor == addr) {
			indoor = value;
		} else if (outdoorSensor == addr) {
			input_t target = Zero - value / k + (IndoorTarget - indoor) * 8;
			if (target < Min) target = Min;
			if (target > Max) target = Max;
			regul.setTarget(target);
		}
	}
private:
	input_t indoor;
	OneWire::ConstAddr<0x28, 0xD9, 0xF8, 0xD5, 0x03, 0x00, 0x00, 0xB0> radiatorSensor;
	OneWire::ConstAddr<0x28, 0x0A, 0xFB, 0xD5, 0x03, 0x00, 0x00, 0x63> outdoorSensor;
	OneWire::ConstAddr<0x28, 0xC3, 0xE0, 0xD5, 0x03, 0x00, 0x00, 0x66> indoorSensor;
};

typedef Cascade<Regul<int16_t, 4000, -4000>, Action<Data, 4, 5>> BoilerCascadeParent;
class BoilerCascade: public BoilerCascadeParent {
public:
	typedef BoilerCascadeParent parent_t;
	using parent_t::input_t;
	const static input_t Target = Temperature::toInt(55);
	const static input_t MaxOut = Temperature::toInt(95);
	const static input_t MinOut = Temperature::toInt(60);
	const static input_t MaxDelta = Temperature::toInt(35);
	const static input_t MinDelta = Temperature::toInt(4);
	const static input_t TCHigh = Temperature::toInt(100);
	BoilerCascade() :  parent_t(Target), inTemp(0), outTemp(0), outAvg(0), tc(0) {}
	void processSensor(const OneWire::Addr& addr, input_t value) {
		if (boilerInSensor == addr) {
			inTemp = value;
		} else if (boilerOutSensor == addr) {
			failCount = 0;
			outTemp = value;
			outAvg = (outAvg + outTemp + 1) / 2;
		}
	}
	void processTC(input_t value) {
		tc = value;
	}
	bool step() {
		if (inTemp + Temperature::toInt(64) < outTemp) {
			// in/out temperature delta is too big, something wrong. Use only out temp.
			inTemp = outTemp;
			failCount++;
		}
		input_t deltaTarget;
		if ((tc < TCHigh
				&& outTemp < MinOut  // boiler is not too hot
				&& failCount == 0    // no errors
				&& outTemp <= outAvg // boiler is cooling
				&& inTemp + MinDelta > outTemp // boiler does not produce heat
				)
			) {
			// Boiler stops, disconnect it from pipes
			deltaTarget = max(outTemp - MinDelta, inTemp + MinDelta);
			pump.stop();
		} else {
			deltaTarget = max(outTemp - MaxDelta, Target);
			pump.start();
		}
		regul.setTarget(deltaTarget);
		current = inTemp;

		bool ret = parent_t::step();
		inTemp = Temperature::toInt(-125);
		tc = 0;
		return ret;
	}
	template <class S>
	S& log(S& s) const {
		parent_t::log(s);
		s << "\nTemp: pump=" << (pump.status() ? "1":"0");
		return s;
	}
private:
	OneWire::ConstAddr<0x10, 0x3F, 0x9D, 0x0F, 0x02, 0x08, 0x00, 0xCA> boilerOutSensor;
	OneWire::ConstAddr<0x28, 0x50, 0x05, 0xD6, 0x03, 0x00, 0x00, 0x0E> boilerInSensor;
	input_t inTemp;
	input_t outTemp;
	input_t outAvg;
	input_t tc; // termocouple
	Action<Data, 3> pump;
};

#endif /* CASCADE_H_ */
