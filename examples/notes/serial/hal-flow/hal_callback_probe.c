/* STM32F407 board fragment. See README.md for integration requirements. */
#include "hal_callback_probe.h"
#include "usart.h"

static uint8_t probe_rx[4];
volatile uint32_t callback_probe_hits;

static void probe_done(UART_HandleTypeDef *uart)
{
    if (uart == &huart2) ++callback_probe_hits;
}

#if (USE_HAL_UART_REGISTER_CALLBACKS == 0)
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
    probe_done(uart);
}
#endif

HAL_StatusTypeDef callback_probe_start(void)
{
#if (USE_HAL_UART_REGISTER_CALLBACKS == 1)
    HAL_StatusTypeDef status = HAL_UART_RegisterCallback(
        &huart2, HAL_UART_RX_COMPLETE_CB_ID, probe_done);
    if (status != HAL_OK) return status;
#endif
    return HAL_UART_Receive_IT(&huart2, probe_rx, sizeof probe_rx);
}
