#include <stdio.h>

#include <inttypes.h>

#include <Arduino.h>

#include "mcp23s17.h"
#include <../SPI/SPI.h>//this chip needs SPI

mcp23s17::mcp23s17(){
#if defined (SPI_HAS_TRANSACTION)
	_spiTransactionsSpeed = MAXSPISPEED;//set to max supported speed (in relation to chip and CPU)
#else
	_spiTransactionsSpeed = 0;
#endif
}


//return 255 if the choosed pin has no INT, otherwise return INT number
//if there's support for SPI transactions it will use SPI.usingInterrupt(intNum);
//to prevent problems from interrupt
/*USE:
  int intNumber = mcp.getInterruptNumber(gpio_int_pin);
  if (intNumber < 255){
    attachInterrupt(intNumber, keypress, FALLING);//attack interrupt
  } else {
    Serial.println("sorry, pin has no INT capabilities!");
  }
 */

int mcp23s17::getInterruptNumber(byte pin) {
	int intNum = digitalPinToInterrupt(pin);
	if (intNum != NOT_AN_INTERRUPT) {
		#if defined (SPI_HAS_TRANSACTION)
//			SPI.usingInterrupt(intNum);
		#endif
		return intNum;
	}
	return 255;
}

void mcp23s17::setSPIspeed(uint32_t spispeed){
	#if defined (SPI_HAS_TRANSACTION)
	if (spispeed > 0){
		if (spispeed > MAXSPISPEED) {
			_spiTransactionsSpeed = MAXSPISPEED;
		} else {
			_spiTransactionsSpeed = spispeed;
		}
	} else {
		_spiTransactionsSpeed = 0;//disable SPItransactons
	}
	#else
	_spiTransactionsSpeed = 0;
	#endif
}

mcp23s17::mcp23s17(const uint8_t csPin,const uint8_t haenAdrs){
	_spiTransactionsSpeed = 0;
	postSetup(csPin,haenAdrs);
}

mcp23s17::mcp23s17(const uint8_t csPin,const uint8_t haenAdrs,uint32_t spispeed){
	postSetup(csPin,haenAdrs,spispeed);
}


void mcp23s17::postSetup(const uint8_t csPin,const uint8_t haenAdrs,uint32_t spispeed){
	#if defined (SPI_HAS_TRANSACTION)
		if (spispeed > 0) setSPIspeed(spispeed);
	#endif
	_cs = csPin;
	if (haenAdrs > 0x19 && haenAdrs < 0x28){//HAEN works between 0x20...0x27
		_adrs = haenAdrs;
		_useHaen = 1;
	} else {
		_adrs = 0;
		_useHaen = 0;
	}
	_readCmd =  (_adrs << 1) | 1;
	_writeCmd = _adrs << 1;
	//setup register values for this chip
	IOCON = 	0x0A;
	IODIR = 	0x00;
	GPPU = 		0x0C;
	GPIO = 		0x12;
	GPINTEN = 	0x04;
	IPOL = 		0x02;
	DEFVAL = 	0x06;
	INTF = 		0x0E;
	INTCAP = 	0x10;
	OLAT = 		0x14;
	INTCON = 	0x08;
}


void mcp23s17::begin(bool protocolInitOverride) {
	if (!protocolInitOverride){
		SPI.begin();
		#if defined (SPI_HAS_TRANSACTION)
			if (_spiTransactionsSpeed == 0){//do not use SPItransactons
				SPI.setClockDivider(SPI_CLOCK_DIV4); // 4 MHz (half speed)
				SPI.setBitOrder(MSBFIRST);
				SPI.setDataMode(SPI_MODE0);
			}
		#else//do not use SPItransactons
			SPI.setClockDivider(SPI_CLOCK_DIV4); // 4 MHz (half speed)
			SPI.setBitOrder(MSBFIRST);
			SPI.setDataMode(SPI_MODE0);
		#endif
	}	
	pinMode(_cs, OUTPUT);
	digitalWrite(_cs, HIGH);
	delay(100);
	_useHaen == 1 ? writeByte(IOCON,0b00101000) : writeByte(IOCON,0b00100000);
	_gpioDirection = 0xFFFF;//all in
	_gpioState = 0xFFFF;//all low 
}



uint16_t mcp23s17::readAddress(byte addr){
	startSend(1);
	SPI.transfer(addr);
	#if !defined(__SAM3X8E__) && ((ARDUINO >= 160) || (TEENSYDUINO > 121))
		uint16_t temp = SPI.transfer16(0x0);
		endSend();
		return temp;
	#else
		byte low_byte  = SPI.transfer(0x0);
		byte high_byte = SPI.transfer(0x0);
		endSend();
		uint16_t temp = low_byte | (high_byte << 8);
		return temp;
	#endif
}


void mcp23s17::gpioPinMode(uint16_t mode){
	if (mode == INPUT){
		_gpioDirection = 0xFFFF;
	} else if (mode == OUTPUT){	
		_gpioDirection = 0x0000;
		_gpioState = 0x0000;
	} else {
		_gpioDirection = mode;
	}
	writeWord(IODIR,_gpioDirection);
}

void mcp23s17::gpioPinMode(uint8_t pin, bool mode){
	if (pin < 16){//0...15
		mode == INPUT ? _gpioDirection |= (1 << pin) :_gpioDirection &= ~(1 << pin);
		writeWord(IODIR,_gpioDirection);
	}
}


void mcp23s17::gpioPort(uint16_t value){
	if (value == HIGH){
		_gpioState = 0xFFFF;
	} else if (value == LOW){	
		_gpioState = 0x0000;
	} else {
		_gpioState = value;
	}
	writeWord(GPIO,_gpioState);
}

void mcp23s17::gpioPort(byte lowByte, byte highByte){
	//_gpioState = byte2word(highByte,lowByte);
	//_gpioState = lowByte | (highByte << 8);
	_gpioState = highByte | (lowByte << 8);
	writeWord(GPIO,_gpioState);
}


uint16_t mcp23s17::readGpioPort(){
	return readAddress(GPIO);
}

uint16_t mcp23s17::readGpioPortFast(){
	return _gpioState;
}

void mcp23s17::portPullup(uint16_t data) {
	if (data == HIGH){
		_gpioState = 0xFFFF;
	} else if (data == LOW){	
		_gpioState = 0x0000;
	} else {
		_gpioState = data;
	}
	writeWord(GPPU, _gpioState);
}


void mcp23s17::gpioDigitalWrite(uint8_t pin, bool value){
	if (pin < 16){//0...15
		value == HIGH ? _gpioState |= (1 << pin) : _gpioState &= ~(1 << pin);
		writeWord(GPIO,_gpioState);
	}
}

void mcp23s17::gpioDigitalWriteFast(uint8_t pin, bool value){
	if (pin < 16){//0...15
		value == HIGH ? _gpioState |= (1 << pin) : _gpioState &= ~(1 << pin);
	}
}

void mcp23s17::gpioPortUpdate(){
	writeWord(GPIO,_gpioState);
}

int mcp23s17::gpioDigitalRead(uint8_t pin){
	if (pin < 16) return (int)(readAddress(GPIO) & 1 << pin);
	return 0;
}


int mcp23s17::gpioDigitalReadFast(uint8_t pin){
	int temp = 0;
	if (pin < 16) temp = bitRead(_gpioState,pin);
	return temp;
}

uint8_t mcp23s17::gpioRegisterReadByte(byte reg){
  uint8_t data = 0;
    startSend(1);
    SPI.transfer(reg);
    data = SPI.transfer(0);
    endSend();
  return data;
}

uint16_t mcp23s17::gpioRegisterReadWord(byte reg){
  uint16_t data = 0;
    startSend(1);
    SPI.transfer(reg);
	#if !defined(__SAM3X8E__) && ((ARDUINO >= 160) || (TEENSYDUINO > 121))
		data = SPI.transfer16(0);
	#else
		data = SPI.transfer(0);
		data = SPI.transfer(0) << 8;
	#endif
    endSend();
  return data;
}

void mcp23s17::gpioRegisterWriteByte(byte reg,byte data,bool both){
	if (!both){
		writeByte(reg,(byte)data);
	} else {
		startSend(0);
		SPI.transfer(reg);
			SPI.transfer(data);
			SPI.transfer(data);
		endSend();
	}
}

void mcp23s17::gpioRegisterWriteWord(byte reg,word data){
	writeWord(reg,(word)data);
}

/* ------------------------------ Low Level ----------------*/
void mcp23s17::writeByte(byte addr, byte data){
	startSend(0);
	SPI.transfer(addr);
	SPI.transfer(data);
	endSend();
}

void mcp23s17::writeWord(byte addr, uint16_t data){
	startSend(0);
	SPI.transfer(addr);
	#if !defined(__SAM3X8E__) && ((ARDUINO >= 160) || (TEENSYDUINO > 121))
		SPI.transfer16(data);
	#else
		//SPI.transfer(word2lowByte(data));
		//SPI.transfer(word2highByte(data));
			SPI.transfer(data >> 8);
			SPI.transfer(data & 0xFF);
	#endif
	endSend();
}

void mcp23s17::startSend(bool mode){
#if defined (SPI_HAS_TRANSACTION)
	if (_spiTransactionsSpeed > 0) SPI.beginTransaction(SPISettings(_spiTransactionsSpeed, MSBFIRST, SPI_MODE0));
#endif

#if defined(__FASTWRITE)
	digitalWriteFast(_cs, LOW);
#else
	digitalWrite(_cs, LOW);
#endif
	mode == 1 ? SPI.transfer(_readCmd) : SPI.transfer(_writeCmd);
}

void mcp23s17::endSend(){
#if defined(__FASTWRITE)
	digitalWriteFast(_cs, HIGH);
#else
	digitalWrite(_cs, HIGH);
#endif

#if defined (SPI_HAS_TRANSACTION)
	if (_spiTransactionsSpeed > 0) SPI.endTransaction();
#endif
}




