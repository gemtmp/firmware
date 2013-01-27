#include <stdlib.h>

#include <avr/io.h>
#include <util/delay.h>

#include <avr/interrupt.h>

#include <iopins.h>

#include "OneWire.h"
#include "serial.h"
#include "TWI.h"
#include "Cascade.h"

template <class Led>
struct LedOn {
	LedOn() { Led::Set(); }
	~LedOn() { Led::Clear(); }
};

typedef IO::Pb5 Led;

typedef OneWire::Wire<IO::Pd2> Wire;
typedef OneWire::DS1820<Wire> DS1820;

uint8_t Data::data = 0;

int main(void)
{
	sei();
	TWI::init();

	SerialPort<9600> com;

	Led::SetDirWrite();

	com << "Starting on 9600" << endl;

	RadiatorCascade radiatorCascade;
	BoilerCascade boilerCascade;
	for(;;)
	{
		TWI::write(0x40, ~Data::data);

		_delay_ms(3962); // delay to get 5 sec loop

		com << "Search ";
		const uint8_t MaxAddrs = 8;
		OneWire::Addr addrs[MaxAddrs];
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
			com << "failed on " << count << endl;
			continue;
		} else {
			com << count << endl;
		}

		if (!Wire::reset())
		{
			com << "Reset failed" << endl;
			continue;
		}

		{
			LedOn<Led> l;
			Wire::skip();
			DS1820::convert();
			DS1820::wait();
		}

		for (int i = 0; i < count; ++i)
		{
			Led::Set();
			OneWire::Temperature t = DS1820::read(addrs[i]);
			Led::Clear();

			if (t.isValid()) {
				radiatorCascade.processSensor(addrs[i], t.get());
				boilerCascade.processSensor(addrs[i], t.get());

				com << "Temp: " << addrs[i] << '=' << t << endl;
			} else {
				com << "Fail " << addrs[i] << endl;
			}
		}
		if (!radiatorCascade.step())
			com << "Radiator Cascade fail" << endl;
		com << "Radiator " << radiatorCascade << endl;
		if (!boilerCascade.step())
			com << "Boiler Cascade fail" << endl;
		com << "Boiler " << boilerCascade << endl;
	}
}

extern "C" void __cxa_pure_virtual()
{
  cli();
  for (;;);
}
