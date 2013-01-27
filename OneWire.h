#ifndef OneWire_h
#define OneWire_h

#include <inttypes.h>
#include <util/delay.h>


namespace OneWire
{
struct Addr {
	const static size_t SIZE = 8;
	uint8_t bytes[SIZE];

	uint8_t& operator[](size_t i) { return bytes[i]; }
	uint8_t operator[](size_t i) const { return bytes[i]; }
	bool operator==(const Addr x) const
	{
		for (size_t i = 0; i < Addr::SIZE; ++i)
			if (x[i] != bytes[i])
				return false;
		return true;
	}
	bool operator!=(const Addr x) const { return !operator==(x); }
};

template <class S>
S& operator<<(S& s, const Addr& x)
{
	for (size_t i = 0; i < Addr::SIZE; ++i)
		s << x[i];
	return s;
}

template <uint8_t a0, uint8_t a1, uint8_t a2, uint8_t a3,
	uint8_t a4, uint8_t a5, uint8_t a6, uint8_t a7>
struct ConstAddr {
	bool operator==(const Addr& x) const {
		if (x[0] != a0) return false;
		if (x[1] != a1) return false;
		if (x[2] != a2) return false;
		if (x[3] != a3) return false;
		if (x[4] != a4) return false;
		if (x[5] != a5) return false;
		if (x[6] != a6) return false;
		if (x[7] != a7) return false;
		return true;
	}
};

template <class _Line>
class Wire
{
  public:
	typedef _Line Line;
    static bool reset(void)
    {
    	Line::Clear();
    	Line::SetDirWrite();
    	_delay_us(480);
    	Line::SetDirRead();
    	_delay_us(10);
    	if(!Line::IsSet())
    		return false; // bus shorting
		_delay_us(50);
		bool devicePresent = !Line::IsSet();
		_delay_us(420);
    	return devicePresent;
    }

    static bool ioBit(bool bit = true)
    {
    	Line::Clear();
    	Line::SetDirWrite();
    	_delay_us(2);
    	if(bit)
    	{
    		Line::SetDirRead();
    	}
    	_delay_us(13);
    	bit = Line::IsSet();
    	_delay_us(60-15);
    	Line::SetDirRead();
    	_delay_us(1);
    	return bit;
    }

    static uint8_t read()
    {
    	uint8_t value = 0;
    	for (int i = 0; i < 8; ++i)
    	{
    		value >>= 1;
    		if (ioBit())
    			value |= 0x80;
    	}
    	return value;
    }

    static void write(uint8_t value)
    {
    	for (int i = 0; i < 8; ++i)
    	{
    		ioBit(value & 1);
    		value >>= 1;
    	}
    }

    // Issue a 1-Wire rom select command, you do the reset first.
    static void select(Addr addr)
    {
    	 write(0x55);
    	 for(int i= 0; i < 8; ++i)
    		 write(addr[i]);
    }

    // Issue a 1-Wire rom skip command, to address all on bus.
    static void skip(void) { write(0xCC); }

    static Addr readAddr()
    {
    	write(0x33);
    	Addr addr;
    	uint8_t crc = 0;
    	for (int i = 0; i < 7; ++i)
    	{
    		addr[i] = read();
    		crc = crc8(addr[i], crc);
    	}
    	addr[7] = read();
    	if (addr[7] != crc)
    		addr[0] = 0;
    	return addr;
    }

    static uint8_t crc8(uint8_t crc, uint8_t data)
    {
		for (int i = 8; i; --i) {
			bool mix = (crc ^ data) & 0x01;
			crc >>= 1;
			if (mix) crc ^= 0x8C;
			data >>= 1;
		}
    	return crc;
    }

};

/**
 * Realization of search algorithm
 * Counter is type of holder of last search results.
 * Maximum number of devices is max value of Counter.
 *
 * Example
 *
 * 		OneWire::Search<Wire> search;
		do {
			com << "Address = " << search() << endl;
		} while (!search.isDone());
 *
 */
template <class Wire, typename Counter = uint8_t>
class Search
{
	bool fail;
	Counter visited;
public:
	bool isDone() { return visited == 0 || fail; }
	bool isFail() { return fail; }
	Search() : fail(true), visited(0) { }
    Addr operator()()
    {
    	Addr addr;
    	if (!Wire::reset())
    	{
    		fail = true;
    		return addr;
    	}
    	Wire::write(0xF0);
    	uint8_t crc = 0;
    	Counter curMask = 1;
    	Counter next = 0;
    	for (int bytePos = 0; bytePos < 8; ++bytePos)
    	{
    		for (uint8_t curBit = 1; curBit != 0; curBit <<= 1)
    		{
				bool bit = Wire::ioBit();
				bool notBit = Wire::ioBit();

				if (bit && notBit)
				{
					// bus failure
					fail = true;
					return addr;
				}
				if (!bit && !notBit)
				{
					bit = visited & curMask;
					if (!bit)
						next = (visited & (curMask-1)) | curMask;
					curMask <<= 1;
				}
				if (bit)
					addr[bytePos] |= curBit;
				else
					addr[bytePos] &= ~curBit;
				Wire::ioBit(bit);
    		}
    		if (bytePos != 7)
    			crc = Wire::crc8(addr[bytePos], crc);
    	}
    	fail = addr[7] != crc;
    	visited = next;

    	return addr;
    }
};

struct Temperature
{
	int8_t num;
	uint8_t frac;
public:
	static const int8_t Error = -128;

	Temperature() : num(Error), frac(0) {}
	Temperature(int8_t v, uint8_t frac) : num(v), frac(frac) {}
	int8_t get() { return num; }
	bool isValid() { return num != Error; }
};

template <class S>
S& operator<<(S& s, const Temperature& x)
{
	static const char fracDigit[16] = {'0', '1', '1', '2', '3', '3', '4', '4',
									   '5', '6', '6', '7', '8', '8', '9', '9' };
	s << (x.num) << '.' << fracDigit[x.frac >> 4];
	return s;
}

template <class Wire, bool parasitePower = false>
class DS1820
{
public:

	static void convert()
	{
		Wire::write(0x44);
	}
	static void wait()
	{
		if (parasitePower)
		{
			_delay_ms(750);
		} else {
			while (!Wire::ioBit()) ;
		}
	}
	template <bool ds18s20>
	static Temperature read()
	{
		RawTemperature t = readRaw();
		return t.toTemperature(ds18s20);
	}
	template <int retryCount = 5>
	static Temperature read(const Addr& addr) {
		for (int i = 0; i < retryCount; ++i) {
			if (!Wire::reset())
				continue;
			Wire::select(addr);
			OneWire::Temperature t = readRaw().toTemperature(addr[0] == 0x10);
			if (t.isValid())
				return t;
		}
		return Temperature();
	}
private:
	struct RawTemperature {
		uint8_t h,l;
		Temperature toTemperature(bool ds18s20) const {
			if (h == 255 && l == 255)
				return Temperature();

			if (ds18s20)
				return Temperature((h & 0x80) | (l >> 1) , l << 7 );
			return Temperature((h << 4) | (l >> 4), l << 4);
		}
	};
	static RawTemperature readRaw() {
		Wire::write(0xbe);

		uint8_t templ, temph, conf;
		uint8_t crc = 0;
		templ = Wire::read();
		crc = Wire::crc8(crc, templ);
		temph = Wire::read();
		crc = Wire::crc8(crc, temph);
		crc = Wire::crc8(crc, Wire::read());
		crc = Wire::crc8(crc, Wire::read());
		conf = Wire::read();
		crc = Wire::crc8(crc, conf);
		for(int i = 5; i < 8; ++i)
			crc = Wire::crc8(crc, Wire::read());

		if (Wire::read() != crc)
			return {255, 255};

		return {temph, templ};
	}
};

} // namespace OneWire
#endif
