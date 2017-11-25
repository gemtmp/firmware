/*
 * Regulator.h
 *
 *  Created on: Jan 27, 2013
 *      Author: gem
 */

#ifndef REGULATOR_H_
#define REGULATOR_H_

typedef int16_t output_t;
template <typename InputType = int16_t, output_t Max = 40000, output_t Min = -40000, InputType Large = 1000>
class Regul {
public:
	typedef InputType input_t;

	Regul(input_t target, uint8_t p = 2, uint8_t i = 0, uint8_t d = 65)
		: target(target), p(p), i(i), d(d),
		  previos(0), integral(0), dValue(0), output(0), isFirstCircle(true) {}
	output_t step(input_t current) {
		input_t err = current - target;
		if (err > Large)
			return output=Max;
		if (err < -Large)
			return output=Min;
		integral += (i * err)/16;
		integral = limit<25>(integral);

		if (isFirstCircle) {
			previos = err;
			isFirstCircle = false;
		}
		dValue = (3*dValue + d*(err - previos)) / 4;
		previos = err;

		if (dValue > Max / 2)
			return output=Max;
		if (dValue < Min / 2)
			return output=Min;

		output_t cur = err * p + integral + dValue; // TODO check overflow
		output = (output + cur) / 2;
		output = limit(output);
		return output;
	}
	void setTarget(input_t t) {
		target = t;
	}
	void reset() { output = 0; integral = 0; previos = 0; }
	input_t getTarget() const { return target; }
	output_t getOutput() const { return output; }
	template <class S>
	S& log(S& s) const {
		s << "Target: " << getTarget() << ", Output: " << getOutput()
				<< ", p: " << previos * p
				<< ", i: " << integral
				<< ", d: " << dValue;
		return s;
	}
	template <uint8_t div =1>
	output_t limit(output_t v) {
		if (v > Max/div)
			v = Max/div;
		else if (v < Min/div)
			v = Min/div;
		return v;
	}

private:
	input_t target;
	uint8_t p;
	uint8_t i;
	uint8_t d;
	input_t previos;
	output_t integral;
	output_t dValue;
	output_t output;
	bool isFirstCircle;
};


#endif /* REGULATOR_H_ */
