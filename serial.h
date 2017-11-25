/*
 * serial.h
 *
 *  Created on: 27.02.2011
 *      Author: gem
 */

#ifndef SERIAL_H_
#define SERIAL_H_

#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif



template <unsigned long baud,
	uint8_t _rxen = RXEN0, uint8_t _txen = TXEN0,
	uint8_t _rxcie = RXCIE0, uint8_t _udre = UDRE0,
    uint8_t _u2x = U2X0>
class SerialPort
{
	volatile uint8_t *ubrrh() { return &UBRR0H;}
    volatile uint8_t *ubrrl() { return &UBRR0L;}
    volatile uint8_t *ucsra() { return &UCSR0A;}
    volatile uint8_t *ucsrb() { return &UCSR0B;}
    volatile uint8_t *udr() { return &UDR0;}

    const char* digits;
public:
    SerialPort() : digits("0123456789ABCDEF")
	{

		uint16_t baud_setting;
		bool use_u2x = true;

#if F_CPU == 16000000UL
		// hardcoded exception for compatibility with the bootloader shipped
		// with the Duemilanove and previous boards and the firmware on the 8U2
		// on the Uno and Mega 2560.
		if (baud == 57600)
		{
			use_u2x = false;
		}
#endif

		if (use_u2x)
		{
			*ucsra() = 1 << _u2x;
			baud_setting = (F_CPU / 4 / baud - 1) / 2;
		}
		else
		{
			*ucsra() = 0;
			baud_setting = (F_CPU / 8 / baud - 1) / 2;
		}

		// assign the baud_setting, a.k.a. ubbr (USART Baud Rate Register)
		*ubrrh() = baud_setting >> 8;
		*ubrrl() = baud_setting;

		//sbi(*ucsrb(), _rxen);
		sbi(*ucsrb(), _txen);
		//sbi(*ucsrb(), _rxcie);
	}

	void write(char c)
	{
		while (!((*ucsra()) & (1 << _udre)))
			;
		*udr() = c;
	}

	SerialPort& operator<< (char c)
	{
		write(c);
		return *this;
	}
	SerialPort& operator<< (bool b)
	{
		write(b ? '1' : '0');
		return *this;
	}
	SerialPort& operator<< (const char* s)
	{
		while(*s) write(*(s++));
		return *this;
	}
	SerialPort& operator<< (uint8_t i)
	{
		write(digits[i>>4]);
		write(digits[i&15]);
		return *this;
	}
	SerialPort& operator<< (int n)
	{
		if (n < 0) {
			write('-');
			n = -n;
		}

		return *this << (unsigned int)n;
	}
	SerialPort& operator<< (unsigned int n)
	{
		if (n == 0)
		{
			write('0');
			return *this;
		}
		uint8_t buf[6];
		long i = 0;
		while(n > 0)
		{
		    buf[i++] = n % 10;
		    n /= 10;
		}
		for (--i; i >= 0; --i)
			write(digits[buf[i]]);

		return *this;
	}
	SerialPort& operator<< (SerialPort& (*pf)(SerialPort&))
	{
		return pf(*this);
	}
};

template <unsigned long b, uint8_t A6,uint8_t A7,uint8_t A8,uint8_t A9,uint8_t A10>
inline SerialPort<b,A6,A7,A8,A9,A10>& endl(SerialPort<b,A6,A7,A8,A9,A10>& p)
{
	p.write('\r'); p.write('\n');
	return p;
}

#endif /* SERIAL_H_ */
