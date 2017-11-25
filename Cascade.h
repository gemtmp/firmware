/*
 * Cascade.h
 *
 *  Created on: Jan 27, 2013
 *      Author: gem
 */

#ifndef CASCADE_H_
#define CASCADE_H_


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
	Cascade(input_t target, uint8_t p) : failCount(0), current(0), regul(target, p) {}
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
	output_t getTarget() const {
		return regul.getTarget();
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
	static const input_t Zero = Temperature::toInt(40);
	static const input_t k = 2;
	static const input_t IndoorTarget = Temperature::toInt(22);
	static const input_t OutdoorAvg = Temperature::toInt(-5); // winter avg
	// target temperature is (Zero - outdoor) / k + (IndoorTarget - indoor) * 4,
	// limited by Min and Max

	static const input_t Fail = Temperature::toInt(85);

	RadiatorCascade() :  parent_t(Zero, 2), indoor(IndoorTarget), outdoor(OutdoorAvg) {}
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
			outdoor = value;
		}
	}
	bool step() {
		input_t target = Zero - outdoor / k + (IndoorTarget - indoor) * 4;
		if (target < Min) target = Min;
		if (target > Max) target = Max;
		regul.setTarget(target);

		indoor = IndoorTarget;
		outdoor = OutdoorAvg;

		bool ret = parent_t::step();
		return ret;
	}
	template <class S>
	S& log(S& s) const {
		parent_t::log(s);
		s << "\nTemp: radiatorValve=" << regul.getOutput();
		return s;
	}

private:
	input_t indoor;
	input_t outdoor;
	OneWire::ConstAddr<0x28, 0xD9, 0xF8, 0xD5, 0x03, 0x00, 0x00, 0xB0> radiatorSensor;
	OneWire::ConstAddr<0x28, 0x0A, 0xFB, 0xD5, 0x03, 0x00, 0x00, 0x63> outdoorSensor;
	OneWire::ConstAddr<0x28, 0xC3, 0xE0, 0xD5, 0x03, 0x00, 0x00, 0x66> indoorSensor;
};

typedef Cascade<Regul<int16_t, 4000, -4000>, Action<Data, 4, 5>> BoilerCascadeParent;
class BoilerCascade: public BoilerCascadeParent {
public:
	typedef BoilerCascadeParent parent_t;
	using parent_t::input_t;
	const static input_t Target = Temperature::toInt(50);
	const static input_t MaxOut = Temperature::toInt(95);
	const static input_t MinOut = Temperature::toInt(50);
	const static input_t MaxDelta = Temperature::toInt(35);
	const static input_t MinDelta = Temperature::toInt(4);
	const static input_t TCHigh = Temperature::toInt(60);
	BoilerCascade() :  parent_t(Target, 4), inTemp(0), outTemp(0), outAvg(0), tc(0),
			readInTemp(false), readOutTemp(false) {}
	void processSensor(const OneWire::Addr& addr, input_t value) {
		if (boilerInSensor == addr) {
			inTemp = value;
			readInTemp = true;
		} else if (boilerOutSensor == addr) {
			outTemp = value;
			readOutTemp = true;
			outAvg = (outAvg + outTemp + 1) / 2;
		}
	}
	void processTC(input_t value) {
		tc = value;
	}
	bool step() {
		if (readInTemp && readOutTemp)
			failCount = 0;
		if (!readInTemp) {
			failCount++;
		}
		readInTemp = false;
		if (!readOutTemp) {
			failCount++;
		}
		readOutTemp = false;
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
		return ret;
	}
	template <class S>
	S& log(S& s) const {
		parent_t::log(s);
		s << "\nTemp: pump=" << pump.status();
		s << "\nTemp: boilerValve=" << regul.getOutput();
		s << "\nTemp: boilerDelta=" << Temperature(outTemp - inTemp);
		return s;
	}
private:
	input_t max( input_t a, input_t b)
	{
		return a > b ? a:b;
	}
	OneWire::ConstAddr<0x28, 0x8D, 0x2E, 0x8E, 0x05, 0x00, 0x00, 0x1D> boilerOutSensor;
	OneWire::ConstAddr<0x28, 0x50, 0x05, 0xD6, 0x03, 0x00, 0x00, 0x0E> boilerInSensor;
	input_t inTemp;
	input_t outTemp;
	input_t outAvg;
	input_t tc; // termocouple
	Action<Data, 3> pump;
	bool readInTemp;
	bool readOutTemp;
};

#endif /* CASCADE_H_ */
