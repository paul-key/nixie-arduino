/****************************************************************************/	
//	Function: Cpp file for DS1307
//	Hardware: Grove - RTC
//	Arduino IDE: Arduino-1.0
//	Author:	 FrankieChu		
//	Date: 	 Jan 19,2013
//	Version: v1.0
//	by www.seeedstudio.com
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
//
/****************************************************************************/

#include <Wire.h>
#include "DS1307.h"

uint8_t DS1307::decToBcd(uint8_t val)
{
	return ( (val/10*16) + (val%10) );
}

//Convert binary coded decimal to normal decimal numbers
uint8_t DS1307::bcdToDec(uint8_t val)
{
	return ( (val/16*10) + (val%16) );
}

void DS1307::begin()
{
	Wire.begin();
}
/*Function: Read time and date from RTC	*/
void DS1307::getTime()
{
    // Reset the register pointer
	Wire.beginTransmission(DS1307_I2C_ADDRESS);
	Wire.write((uint8_t)0x00);
	Wire.endTransmission();  
	Wire.requestFrom(DS1307_I2C_ADDRESS, 7);
	// A few of these need masks because certain bits are control bits
	second	   = bcdToDec(Wire.read() & 0x7f);
	minute	   = bcdToDec(Wire.read());
	hour	   = bcdToDec(Wire.read() & 0x3f);// Need to change this if 12 hour am/pm
	Wire.read();    //day of week
	dayOfMonth = bcdToDec(Wire.read());
	month      = bcdToDec(Wire.read());
	year	   = bcdToDec(Wire.read());
}
/*******************************************************************/
/*Frunction: Write the time that includes the date to the RTC chip */
void DS1307::setTime()
{
	Wire.beginTransmission(DS1307_I2C_ADDRESS);
	Wire.write((uint8_t)0x00);
	Wire.write(decToBcd(second));// 0 to bit 7 starts the clock
	Wire.write(decToBcd(minute));
	Wire.write(decToBcd(hour));  // If you want 12 hour am/pm you need to set bit 6 
	Wire.write(decToBcd(1));
	Wire.write(decToBcd(dayOfMonth));
	Wire.write(decToBcd(month));
	Wire.write(decToBcd(year));
	Wire.endTransmission();
}

float DS1307::readTemperature(void)
{
    uint8_t msb, lsb;

    Wire.beginTransmission(DS1307_I2C_ADDRESS);
    Wire.write(DS3231_REG_TEMPERATURE);
    Wire.endTransmission();
    Wire.requestFrom(DS1307_I2C_ADDRESS, 2);
    while(!Wire.available()) {};
    msb = Wire.read();
    lsb = Wire.read();
    
    return ((((short)msb << 8) | (short)lsb) >> 6) / 4.0f;
}

void DS1307::fillByHMS(uint8_t _hour, uint8_t _minute, uint8_t _second)
{
	// assign variables
	hour = _hour;
	minute = _minute;
	second = _second;
}
void DS1307::fillByYMD(uint8_t _year, uint8_t _month, uint8_t _day)
{
	year = _year;
	month = _month;
	dayOfMonth = _day;
}


