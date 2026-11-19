/*
 * Licensed under the GNU General Public License version 2 with exceptions. See
 * LICENSE file in the project root for full license information
 */

 /** \file
 * \brief
 * ESC hardware layer functions.
 *
 * Function to read and write commands to the ESC. Used to read/write ESC
 * registers and memory.
 */

#include "../SOES/soes/esc.h"
#include "soes_pin_mapping_def.h"
#include "app_runtime.h"
#include <string.h>
#include "stm32h7xx_hal.h"
#include "main.h"


#define MAX_READ_SIZE   128

#define ESC_CMD_READ    0x02
#define ESC_CMD_READWS  0x03
#define ESC_CMD_WRITE   0x04
#define ESC_CMD_NOP     0x00
#define ESC_TERM        0xff
#define ESC_NEXT        0x00



static uint8_t read_termination[MAX_READ_SIZE] = { 0 };

static pin_mapping_typedef * soes_pin_mapping;
static uint32_t startup_ms;

#define GPIO_ECAT_RESET    1 /* specific function to hold ESC reset on startup
                              * when emulating EEPROM
                              */


static void esc_address (uint16_t address, uint8_t command)
{
   /* Device is selected already.
    * We use 2 bytes addressing.
    */
   uint8_t data[2];

   /* address 12:5 */
   data[0] = (address >> 5);
   /* address 4:0 and cmd 2:0 */
   data[1] = ((address & 0x1F) << 3) | command;

   /* Write (and read AL interrupt register) */
   
   uint8_t event_bytes[2];
   HAL_StatusTypeDef err = App_SpiTransfer(soes_pin_mapping->hspi,
      data, event_bytes, sizeof(data), 2, 0);
   if(err != HAL_OK)
   {
      Error_Handler();
   }
   
   const uint16_t event = (uint16_t)event_bytes[0] | ((uint16_t)event_bytes[1] << 8);
   ESCvar.ALevent = etohs(event);
}

/** ESC read function used by the Slave stack.
 *
 * @param[in]   address     = address of ESC register to read
 * @param[out]  buf         = pointer to buffer to read in
 * @param[in]   len         = number of bytes to read
 */
void ESC_read (uint16_t address, void *buf, uint16_t len)
{
   // Bound SOES's internal DL-status startup wait without changing the stack.
   if (!App_IsReady() && HAL_GetTick() - startup_ms >= 2000) Error_Handler();
   if (len == 0) return;
   HAL_GPIO_WritePin(soes_pin_mapping->NCS_port,soes_pin_mapping->NCS_pin, GPIO_PIN_RESET);
   esc_address(address, ESC_CMD_READ);
   static const uint8_t zeros[MAX_READ_SIZE] = {0};
   uint8_t *destination = buf;
   while (len) {
      const uint16_t chunk = len > MAX_READ_SIZE ? MAX_READ_SIZE : len;
      // Only the final byte of the entire ESC transaction gets termination.
      const uint8_t *tx = len > chunk ? zeros : read_termination + MAX_READ_SIZE - chunk;
      if (App_SpiTransfer(soes_pin_mapping->hspi, tx, destination, chunk, 2, 0) != HAL_OK)
         Error_Handler();
      destination += chunk;
      len -= chunk;
   }
   HAL_GPIO_WritePin(soes_pin_mapping->NCS_port,soes_pin_mapping->NCS_pin, GPIO_PIN_SET);
}

/** ESC write function used by the Slave stack.
 *
 * @param[in]   address     = address of ESC register to write
 * @param[out]  buf         = pointer to buffer to write from
 * @param[in]   len         = number of bytes to write
 */
void ESC_write (uint16_t address, void *buf, uint16_t len)
{
   if (len == 0) return;
   uint8_t dummy[MAX_READ_SIZE];
   const uint8_t *source = buf;
   HAL_GPIO_WritePin(soes_pin_mapping->NCS_port,soes_pin_mapping->NCS_pin, GPIO_PIN_RESET);
   esc_address(address, ESC_CMD_WRITE);
   while (len) {
      const uint16_t chunk = len > MAX_READ_SIZE ? MAX_READ_SIZE : len;
      if (App_SpiTransfer(soes_pin_mapping->hspi, source, dummy, chunk, 2, 0) != HAL_OK)
         Error_Handler();
      source += chunk;
      len -= chunk;
   }
   HAL_GPIO_WritePin(soes_pin_mapping->NCS_port,soes_pin_mapping->NCS_pin, GPIO_PIN_SET);
}

void ESC_reset (void)
{
   const uint32_t start = HAL_GetTick();
   while (!HAL_GPIO_ReadPin(soes_pin_mapping->EEPROM_loaded_port,soes_pin_mapping->EEPROM_loaded_pin)) {
      if (HAL_GetTick() - start >= 2000) Error_Handler();
      App_Delay(1);
   }
}

void ESC_init (const esc_cfg_t * config)
{
   startup_ms = HAL_GetTick();
   // read the mapping of the physical ports from the user.
   soes_pin_mapping = (pin_mapping_typedef *) config->user_arg;
   read_termination[MAX_READ_SIZE - 1] = ESC_TERM;
}
