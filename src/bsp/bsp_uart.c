#include "usart.h"
#include "stm32h5xx_hal.h"

void bsp_uart_send(uint8_t* buff, uint16_t size)
{
    uint8_t err = HAL_UART_Transmit(&huart1, buff, size, HAL_MAX_DELAY);
    if ( err != HAL_OK)
        while (1);
}


