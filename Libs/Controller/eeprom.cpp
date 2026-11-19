#include "eeprom.h"
#include "app_runtime.h"
#include <cmath>

void EEPROM_Write (uint16_t page, uint16_t offset, uint8_t *data, uint16_t size)
{

	// Find out the number of bit, where the page addressing starts
	int paddrposition = log2(EEPROM_PAGE_SIZE);

	// calculate the start page and the end page
	uint16_t startPage = page;
	uint16_t endPage = page + ((size+offset)/EEPROM_PAGE_SIZE);

	// number of pages to be written
	uint16_t numofpages = (endPage-startPage) + 1;
	uint16_t pos=0;

	// write the data
	for (int i=0; i<numofpages; i++)
	{
		/* calculate the address of the memory location
		 * Here we add the page address with the byte address
		 */
		uint16_t MemAddress = startPage<<paddrposition | offset;
        uint16_t space_in_page = EEPROM_PAGE_SIZE - offset;
        uint16_t bytesremaining = (size < space_in_page) ? size : space_in_page;
        if (bytesremaining == 0) break;

		App_I2cTransfer(EEPROM_I2C_HANDLE, EEPROM_I2C_ADDRESS << 1, MemAddress, I2C_MEMADD_SIZE_16BIT, data + pos, nullptr, bytesremaining, 10, 0);  // write the data to the EEPROM

		startPage += 1;  // increment the page, so that a new page address can be selected for further write
		offset=0;   // since we will be writing to a new page, so offset will be 0
		size = size-bytesremaining;  // reduce the size of the bytes
		pos += bytesremaining;  // update the position for the data buffer

		App_Delay (4);  // Write cycle delay (5ms)
	}
}



void EEPROM_Read (uint16_t page, uint16_t offset, uint8_t *data, uint16_t size)
{
	int paddrposition = log2(EEPROM_PAGE_SIZE); // Calculate the number of bits for page addressing

	uint16_t startPage = page;
	uint16_t endPage = page + ((size+offset)/EEPROM_PAGE_SIZE);

	uint16_t numofpages = (endPage-startPage) + 1;
	uint16_t pos=0;

	for (int i=0; i<numofpages; i++)
	{
		uint16_t MemAddress = startPage<<paddrposition | offset;
        uint16_t space_in_page = EEPROM_PAGE_SIZE - offset;
        uint16_t bytesremaining = (size < space_in_page) ? size : space_in_page;
        if (bytesremaining == 0) break;
		App_I2cTransfer(EEPROM_I2C_HANDLE, EEPROM_I2C_ADDRESS << 1, MemAddress, I2C_MEMADD_SIZE_16BIT, nullptr, data + pos, bytesremaining, 10, 0);
		startPage += 1;
		offset=0;
		size = size-bytesremaining;
		pos += bytesremaining;
	}
}
