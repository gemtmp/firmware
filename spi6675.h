#ifndef SPI6675_H_
#define SPI6675_H_


#include <inttypes.h>
#include <util/delay.h>

#include "temperature.h"

namespace SPI
{

template <class CLK, class CS, class MISO>
class SPI
{
  public:

  static void start()
  {
	CLK::SetDirWrite();
	CS::SetDirWrite();
	MISO::SetDirRead();

	disable();
  }

  static void stop()
  {
	CLK::SetDirRead();
	CS::SetDirRead();
	MISO::SetDirRead();
  }

  static void enable()
  {
	CS::Clear();
  }

  static void disable()
  {
	CS::Set();
  }

  static uint8_t read()
  {
	  uint8_t d = 0;

	  for (int i=7; i>=0; i--)
	  {
	    CLK::Clear();
	    _delay_ms(1);
	    if (MISO::IsSet()) {
	      d |= (1 << i);
	    }

	    CLK::Set();
	    _delay_ms(1);
	  }

	  return d;
	}
};

template <class Spi>
class max6675
{
public:
  typedef Spi SPI;
  static Temperature temperature()
  {
	  Spi::enable();
	  _delay_ms(1);
	  int16_t t = Spi::read();
	  t <<= 8;
	  t |= Spi::read();
	  Spi::disable();

	  if (t & 0x4)
		  return Temperature();
	  t >>= 3;
	  t <<= 2;
	  return Temperature(t);
  }
};

} //namespace SPI

#endif // SERIAL_H_
