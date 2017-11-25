#include <stdlib.h>

#include <avr/io.h>
#include <util/delay.h>

#include <avr/interrupt.h>

#include <iopins.h>
using namespace Mcucpp;

#include "OneWire.h"
#include "serial.h"
#include "TWI.h"
#include "Cascade.h"
#include "Clock.h"
#include "spi6675.h"

template <class Led>
struct LedOn {
	LedOn(bool v = true) { Led::Set(v); }
	~LedOn() { Led::Clear(); }
};

typedef IO::Pb5 Led;

typedef OneWire::Wire<IO::Pd2> Wire;
typedef OneWire::DS1820<Wire> DS1820;

typedef IO::Pd3 MISO;
typedef IO::Pd4 CS;
typedef IO::Pd5 CLK;
typedef IO::Pd6 BOILER_ON;

typedef SPI::max6675<SPI::SPI<CLK, CS, MISO> > max6675;

const static unsigned int cycleTime = 5000; // 5 sec per loop
const static uint8_t bolierDelay = 600/(cycleTime/1000); // 10 minute
const static uint8_t bolierDelayOff = 60*15/(cycleTime/1000); // 30 minute
const static uint8_t bolierRetryDelay = 900/(cycleTime/1000); // 15 minute
const static int16_t MinFeedTemp = Temperature::toInt(35);

OneWire::ConstAddr<0x10, 0xA1, 0x7B, 0x0F, 0x02, 0x08, 0x00, 0x2E> heatOuputSensor;

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

	BOILER_ON::Clear();
	BOILER_ON::SetDirWrite();
	BOILER_ON::Clear();

	com << "Starting on 9600" << endl;

	RadiatorCascade radiatorCascade;
	BoilerCascade boilerCascade;
	const uint8_t MaxAddrs = 16;
	OneWire::Addr addrs[MaxAddrs];
	uint16_t fails = 0;
	uint8_t boilerCircles = bolierDelay;
	uint8_t boilerCirclesOn = 0;
	bool boilerOn = false;
	bool boilerStatus = false;
	bool boilerRealStatus = false;

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

		Temperature heatOutput;
		for (int i = 0; i < count; ++i)
		{
			Led::Set();
			Temperature t = DS1820::read(addrs[i]);
			Led::Clear();

			if (t.isValid()) {
				if (heatOuputSensor == addrs[i])
					heatOutput = t;
				radiatorCascade.processSensor(addrs[i], t.get());
				boilerCascade.processSensor(addrs[i], t.get());

				com << "Temp: " << addrs[i] << '=' << t << endl;
			} else {
				com << "Fail  " << addrs[i] << endl;
				fails++;
			}
		}

		Temperature tc = max6675::temperature();
		if (tc.isValid()) {
			boilerCascade.processTC(tc.get());
			com << "Temp: TC=" << tc << endl;
		} else {
			com << "Fail  TC" << endl;
			fails++;
		}

		com << "Temp: fails=" << fails << endl;

		if (!radiatorCascade.step())
			com << "Radiator Cascade fail" << endl;
		if (!boilerCascade.step())
			com << "Boiler Cascade fail" << endl;


		boilerOn = radiatorCascade.getOutput() < (boilerRealStatus ? 50 : -100)
				|| (heatOutput.get() < radiatorCascade.getTarget() +(boilerRealStatus ? 5 : 0)
						&& heatOutput.isValid())
				|| (heatOutput.get() < MinFeedTemp && heatOutput.isValid());
		if (boilerOn != boilerStatus) {
			boilerCircles = boilerOn ? bolierDelay : bolierDelayOff;
			boilerCirclesOn = 0;
			boilerStatus = boilerOn;
		}
		if (boilerCircles != 0)
			boilerCircles--;
		else {
			if (boilerStatus && boilerCirclesOn != 255)
				boilerCirclesOn++;
			if (tc.get() < boilerCascade.TCHigh && boilerCirclesOn > bolierRetryDelay)
				boilerStatus = false; // turn boiler off temporary to trigger burning
			BOILER_ON::Set(boilerStatus);
			boilerRealStatus = boilerStatus;
		}
		LedOn<Led> l(boilerRealStatus);

		com << "Temp: boiler=" << boilerRealStatus << endl;

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
