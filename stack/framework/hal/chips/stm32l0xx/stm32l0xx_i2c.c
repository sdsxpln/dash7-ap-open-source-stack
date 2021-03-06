#include <debug.h>
#include <stdio.h>


#include "hwgpio.h"
#include "hwi2c.h"

#include "platform.h"
#include "ports.h"
#include "stm32l0xx_gpio.h"
#include "stm32l0xx_hal.h"
#include "stm32l0xx_hal_i2c.h"

// TODO use other ways to avoid long polling
#define I2C_POLLING  100000
#define I2C_DEFAULT_TIMEOUT	1000

#ifndef IC2_BAUDRATE
#define IC2_BAUDRATE 100000
#endif

//HAL_MAX_DELAY


typedef struct {
  pin_id_t sda;
  pin_id_t scl;
} i2c_pins_t;

// TODO to be completed with all documented locations

/*
const PinMap PinMap_I2C_SDA[] = {
{PA_10, I2C_1, STM_PIN_DATA(STM_MODE_AF_OD, GPIO_NOPULL, GPIO_AF6_I2C1)}, // ARDUINO D2
{PB_4,  I2C_3, STM_PIN_DATA(STM_MODE_AF_OD, GPIO_NOPULL, GPIO_AF7_I2C3)}, // Pin not available on any connector
{PB_7,  I2C_1, STM_PIN_DATA(STM_MODE_AF_OD, GPIO_NOPULL, GPIO_AF1_I2C1)}, // ARDUINO D5 - Used also to drive LED4
{PB_9,  I2C_1, STM_PIN_DATA(STM_MODE_AF_OD, GPIO_NOPULL, GPIO_AF4_I2C1)}, // ARDUINO D14
{PB_11, I2C_2, STM_PIN_DATA(STM_MODE_AF_OD, GPIO_NOPULL, GPIO_AF6_I2C2)}, // Pin not available on any connector
{PB_14, I2C_2, STM_PIN_DATA(STM_MODE_AF_OD, GPIO_NOPULL, GPIO_AF5_I2C2)}, // ARDUINO D12
{PC_1,  I2C_3, STM_PIN_DATA(STM_MODE_AF_OD, GPIO_NOPULL, GPIO_AF7_I2C3)}, // Pin not available on any connector
{NC,    NC,    0}
};

const PinMap PinMap_I2C_SCL[] = {
{PA_8,  I2C_3, STM_PIN_DATA(STM_MODE_AF_OD, GPIO_NOPULL, GPIO_AF7_I2C3)}, // ARDUINO D7
{PA_9,  I2C_1, STM_PIN_DATA(STM_MODE_AF_OD, GPIO_NOPULL, GPIO_AF6_I2C1)}, // ARDUINO D8
{PB_6,  I2C_1, STM_PIN_DATA(STM_MODE_AF_OD, GPIO_NOPULL, GPIO_AF1_I2C1)}, // ARDUINO D10 - Used also to drive LED3
{PB_8,  I2C_1, STM_PIN_DATA(STM_MODE_AF_OD, GPIO_NOPULL, GPIO_AF4_I2C1)}, // ARDUINO D15
{PB_10, I2C_2, STM_PIN_DATA(STM_MODE_AF_OD, GPIO_NOPULL, GPIO_AF6_I2C2)}, // Pin not available on any connector
{PB_13, I2C_2, STM_PIN_DATA(STM_MODE_AF_OD, GPIO_NOPULL, GPIO_AF5_I2C2)}, // ARDUINO D13
{PC_0,  I2C_3, STM_PIN_DATA(STM_MODE_AF_OD, GPIO_NOPULL, GPIO_AF7_I2C3)}, // Pin not available on any connector
{NC,    NC,    0}
};
*/


typedef struct i2c_handle {
  I2C_HandleTypeDef hal_handle;
} i2c_handle_t;

static i2c_handle_t handle[I2C_COUNT] = {
  {
    .hal_handle.Instance = NULL,
  }
};

static inline uint32_t get_i2c_timing(int hz)
{
    uint32_t tim = 0;

    switch (hz) {
        case 100000:
            tim = 0x10805E89; // Standard mode with Rise Time = 400ns and Fall Time = 100ns
            break;
        case 400000:
            tim = 0x00901850; // Fast mode with Rise Time = 250ns and Fall Time = 100ns
            break;
        case 1000000:
            tim = 0x00700818; // Fast mode Plus with Rise Time = 60ns and Fall Time = 100ns
            break;
        default:
            break;
    }
    return tim;
}

i2c_handle_t* i2c_init(uint8_t idx, uint8_t pins, uint32_t baudrate)
{
  assert(pins==0);
  assert(idx < I2C_COUNT);

  GPIO_InitTypeDef gpio_init_options;

  /*RCC_PeriphCLKInitTypeDef PeriphClkInit;
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C1;
	PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_PCLK1;
	if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
	{
		return NULL;
	}*/

  switch ((uint32_t)(i2c_ports[idx].i2c))
  {
    case I2C1_BASE:
      __HAL_RCC_I2C1_CLK_ENABLE();
      break;
    case I2C2_BASE:
      __HAL_RCC_I2C2_CLK_ENABLE();
      break;
    case I2C3_BASE:
      __HAL_RCC_I2C3_CLK_ENABLE();
      break;
    default:
      assert(false);
  }

  gpio_init_options.Alternate = i2c_ports[idx].alternate;
  gpio_init_options.Mode = GPIO_MODE_AF_OD;
  gpio_init_options.Pull = GPIO_PULLUP;
  gpio_init_options.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  hw_gpio_configure_pin_stm(i2c_ports[idx].scl_pin, &gpio_init_options);
  hw_gpio_configure_pin_stm(i2c_ports[idx].sda_pin, &gpio_init_options);

  I2C_InitTypeDef i2c_init_options;
  i2c_init_options.Timing = get_i2c_timing(baudrate);
  i2c_init_options.OwnAddress1 = 0;
  i2c_init_options.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  i2c_init_options.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  i2c_init_options.OwnAddress2 = 0;
  i2c_init_options.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  i2c_init_options.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  handle[idx].hal_handle.Init = i2c_init_options;
  handle[idx].hal_handle.Instance = i2c_ports[idx].i2c;
  if (HAL_I2C_Init(&handle[idx].hal_handle) != HAL_OK)
  {
    return NULL;
  }

  // TODO only disable clock until actually using I2C

  return &handle[idx];
}

int8_t i2c_write(i2c_handle_t* i2c, uint8_t to, uint8_t* payload, int length) {
  HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(&i2c->hal_handle, (uint16_t) to << 1, payload, length, I2C_DEFAULT_TIMEOUT);
  return status == HAL_OK;
}

int8_t i2c_read(i2c_handle_t* i2c, uint8_t to, uint8_t* payload, int length) {

  HAL_StatusTypeDef status = HAL_I2C_Master_Receive(&i2c->hal_handle, (uint16_t) to << 1, payload, length, I2C_DEFAULT_TIMEOUT);
  return status == HAL_OK;
}

int8_t i2c_read_memory(i2c_handle_t* i2c, uint8_t to, uint16_t register_address,  uint8_t register_address_size, uint8_t* payload, int length)
{
  uint8_t mem_addr_size = register_address_size == 8 ? I2C_MEMADD_SIZE_8BIT : I2C_MEMADD_SIZE_16BIT;
  int status = HAL_I2C_Mem_Read(&i2c->hal_handle, (uint16_t) to << 1, register_address, mem_addr_size, payload, length, I2C_DEFAULT_TIMEOUT);
  return status == HAL_OK;
}

int8_t i2c_write_memory(i2c_handle_t* i2c, uint8_t to, uint16_t register_address,  uint8_t register_address_size, uint8_t* payload, int length)
{
  uint8_t mem_addr_size = register_address_size == 8 ? I2C_MEMADD_SIZE_8BIT : I2C_MEMADD_SIZE_16BIT;
  int status = HAL_I2C_Mem_Write(&i2c->hal_handle, (uint16_t) to << 1, register_address, mem_addr_size, payload, length, I2C_DEFAULT_TIMEOUT);
  return status == HAL_OK;
}

int8_t i2c_write_read(i2c_handle_t* i2c, uint8_t to, uint8_t* payload, int length, uint8_t* receive, int receive_length)
{
  HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(&i2c->hal_handle, (uint16_t) to << 1, payload, length, I2C_DEFAULT_TIMEOUT);
  if (status != HAL_OK) return 0;

  status = HAL_I2C_Master_Receive(&i2c->hal_handle, (uint16_t) to << 1, receive, receive_length, I2C_DEFAULT_TIMEOUT);
  return status == HAL_OK;
}
