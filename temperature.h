/*
 * temperature.h
 *
 *  Created on: Feb 23, 2016
 *      Author: gem
 */

#ifndef TEMPERATURE_H_
#define TEMPERATURE_H_


class Temperature
{
public:

	Temperature() : value(Error) {}
	Temperature(uint8_t v, uint8_t frac) : value(v*256 + frac) {}
	int16_t get() const { return value; }
	bool isValid() { return value != Error; }

	constexpr static inline int16_t toInt(int8_t v) { return v*16; }
	static const int16_t Error = -127*16;
private:
	int16_t value;
};

template <class S>
S& operator<<(S& s, const Temperature& x)
{
	static const char fracDigit[16] = {'0', '1', '1', '2', '3', '3', '4', '4',
									   '5', '6', '6', '7', '8', '8', '9', '9' };
	int16_t v = x.get();
	if (v < 0) {
		s << '-';
		v = -v;
	}
	s << (v >> 4) << '.' << fracDigit[v & 15];
	return s;
}


#endif /* TEMPERATURE_H_ */
