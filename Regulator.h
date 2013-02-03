/*
 * Regulator.h
 *
 *  Created on: Jan 27, 2013
 *      Author: gem
 */

#ifndef REGULATOR_H_
#define REGULATOR_H_

typedef int16_t output_t;
template <typename InputType = int16_t, output_t Max = 32000, output_t Min = -32000>
class Regul {
public:
	typedef InputType input_t;

	Regul(input_t target, uint8_t p = 16)
		: target(target), p(p), output(0) {}
	output_t step(input_t current) {
		input_t err = current - target;
		output = err * p; // TODO check overflow
		if (output > Max)
			output = Max;
		else if (output < Min)
			output = Min;
		return output;
	}
	void setTarget(input_t t) {
		target = t;
	}
	void reset() { output = 0; }
	input_t getTarget() const { return target; }
	output_t getOutput() const { return output; }
	template <class S>
	S& log(S& s) const {
		s << "Target: " << getTarget() << ", Output: " << getOutput();
		return s;
	}

private:
	input_t target;
	uint8_t p;
	output_t output;
};


#endif /* REGULATOR_H_ */
