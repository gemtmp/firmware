#include <stdlib.h>

#include <avr/io.h>
#include <util/delay.h>

#include <avr/interrupt.h>

#include <iopins.h>

#include "OneWire.h"
#include "serial.h"
#include "TWI.h"
#include "Cascade.h"
#include "Clock.h"
#include "spi6675.h"

template <class Led>
struct LedOn {
	LedOn() { Led::Set(); }
	~LedOn() { Led::Clear(); }
};

typedef IO::Pb5 Led;

typedef OneWire::Wire<IO::Pd2> Wire;
typedef OneWire::DS1820<Wire> DS1820;

typedef IO::Pd3 MISO;
typedef IO::Pd4 CS;
typedef IO::Pd5 CLK;

typedef SPI::max6675<SPI::SPI<CLK, CS, MISO> > max6675;

const static unsigned int cycleTime = 5000; // 5 sec per loop

uint8_t Data::data = 0;

void delay_ms(uint16_t t)
{
	for (uint16_t i = 0; i < t; ++i)
		_delay_ms(1);
}

int main(void)
{
	sei();
	Clock::start();

	TWI::init();
	TWI::write(0x40, 255);

	max6675::SPI::start();

	SerialPort<9600> com;

	Led::SetDirWrite();

	com << "Starting on 9600" << endl;

	RadiatorCascade radiatorCascade;
	BoilerCascade boilerCascade;
	const uint8_t MaxAddrs = 16;
	OneWire::Addr addrs[MaxAddrs];
	uint16_t fails = 0;
	for(;;)
	{

		Clock::clock_t startTime = Clock::millis();

		com << "Search ";
		uint8_t count = 0;
		OneWire::Search<Wire> search;
		{
			LedOn<Led> l;
			do {
				addrs[count++] = search();
			} while (!search.isDone() && count < MaxAddrs);
		}
		if (search.isFail())
		{
			com << "failed on " << int(count) << ": " << search.error();
			search.errorDetail(com) <<  endl;
			fails++;
		} else {
			com << int(count) << endl;
			fails = 0;
		}

		if (!Wire::reset())
		{
			com << "Reset failed" << endl;
			fails++;
			continue;
		}

		{
			LedOn<Led> l;
			Wire::skip();
			DS1820::convert();
			com << "Radiator " << radiatorCascade << endl;
			com << "Boiler   " << boilerCascade << endl;
			DS1820::wait();
		}

		for (int i = 0; i < count; ++i)
		{
			Led::Set();
			Temperature t = DS1820::read(addrs[i]);
			Led::Clear();

			if (t.isValid()) {
				radiatorCascade.processSensor(addrs[i], t.get());
				boilerCascade.processSensor(addrs[i], t.get());

				com << "Temp: " << addrs[i] << '=' << t << endl;
			} else {
				com << "Fail  " << addrs[i] << endl;
				fails++;
			}
		}

		{
			Temperature t = max6675::temperature();
			if (t.isValid()) {
				boilerCascade.processTC(t.get());
				com << "Temp: TC=" << t << endl;
			} else {
				com << "Fail  TC" << endl;
				fails++;
			}
		}
		com << "Temp: fails=" << fails << endl;

		if (!radiatorCascade.step())
			com << "Radiator Cascade fail" << endl;
		if (!boilerCascade.step())
			com << "Boiler Cascade fail" << endl;

		//Clock::clock_t regStart = Clock::millis();
		int16_t rDelay = radiatorCascade.getAbsOutput();
		int16_t bDelay = boilerCascade.getAbsOutput();

		TWI::write(0x40, ~Data::data);
		if (rDelay > 0 || bDelay > 0) {
			if (rDelay < bDelay) {
				delay_ms(rDelay);
				RadiatorCascade::action_t::stop();
				TWI::write(0x40, ~Data::data);
				delay_ms(bDelay - rDelay);
				BoilerCascade::action_t::stop();
				TWI::write(0x40, ~Data::data);
			} else {
				delay_ms(bDelay);
				BoilerCascade::action_t::stop();
				TWI::write(0x40, ~Data::data);
				delay_ms(rDelay - bDelay);
				RadiatorCascade::action_t::stop();
				TWI::write(0x40, ~Data::data);
			}
		}
		Clock::clock_t regStop = Clock::millis();
		com << "cycle time " << regStop - startTime << endl;
		if (cycleTime > (regStop - startTime))
			delay_ms(cycleTime - (regStop - startTime));
	}
}

extern "C" void __cxa_pure_virtual()
{
  cli();
  for (;;);
}
