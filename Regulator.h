/*
 * Regulator.h
 *
 *  Created on: Jan 27, 2013
 *      Author: gem
 */

#ifndef REGULATOR_H_
#define REGULATOR_H_

typedef int16_t output_t;
template <typename InputType = int16_t, output_t Max = 40000, output_t Min = -40000, InputType Large = 400>
class Regul {
public:
	typedef InputType input_t;

	Regul(input_t target, uint8_t p = 8, uint8_t i = 0, uint8_t d = 48)
		: target(target), previos(0), integral(0), dValue(0)
			, p(p), i(i), d(d), output(0) {}
	output_t step(input_t current) {
		input_t err = current - target;
		if (err > Large)
			return output=Max;
		if (err < -Large)
			return output=Min;
		integral += (i * err)/16;
		integral = limit<8>(integral);
		dValue = (d*(err - previos))/2;
		output = err * p + integral + dValue; // TODO check overflow
		output = limit(output);
		previos = err;
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
				<< ", i= " << integral
				<< ", d= " << dValue;
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
	input_t previos;
	input_t integral;
	input_t dValue;
	uint8_t p;
	uint8_t i;
	uint8_t d;
	output_t output;
};


#endif /* REGULATOR_H_ */
