#include "init.h" // Always need init.h, otherwise nothing will work.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

SPI_HandleTypeDef hspi2;
extern UART_HandleTypeDef USB_UART;
DMA_HandleTypeDef hdma_spi2_tx;
DMA_HandleTypeDef hdma_spi2_rx;

#define BUFFER_SIZE 128
uint8_t txBuffer[BUFFER_SIZE];
uint8_t rxBuffer[BUFFER_SIZE];
uint8_t uartChar;
uint16_t idx = 0;
uint8_t lineReady = 0;

static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_SPI2_Init(void);
void Error_Handler(void);

// SPI callback
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI2)
    {
        printf("SPI DMA transfer complete\r\n");
    }
}

int main(void)
{
	Sys_Init();

    MX_GPIO_Init();
    MX_DMA_Init();
    MX_SPI2_Init();

    HAL_NVIC_EnableIRQ(DMA1_Stream3_IRQn);
    HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);

    printf("\033[2J\033[;H"); // Erase screen & move cursor to home position
    fflush(stdout);

    while (1)
    {
		int c = getchar();
		printf("%c", c);
		if (c == '\r' || c == '\n') lineReady = 1;
		else if (idx < BUFFER_SIZE - 1) txBuffer[idx++] = c;

        if (lineReady)
        {
            printf("\033[2J\033[;H"); // Erase screen & move cursor to home position
            fflush(stdout);

            txBuffer[idx] = '\0';

            printf("\r\nTransmitting: %s\r\n\n", txBuffer);
            memset(rxBuffer, 0, sizeof(rxBuffer));

            // Start SPI DMA transfer in Normal mode
            if (HAL_SPI_TransmitReceive_DMA(&hspi2, txBuffer, rxBuffer, idx) != HAL_OK)
            {
                printf("SPI DMA start error.\r\n");
            }

            // Wait for SPI transfer to complete
            while (HAL_SPI_GetState(&hspi2) != HAL_SPI_STATE_READY);

            printf("Received: %s\r\n\r\n", rxBuffer);

            lineReady = 0;
            idx = 0;
        }
    }
}

// SPI2 Initialization
static void MX_SPI2_Init(void)
{
    hspi2.Instance = SPI2;
    hspi2.Init.Mode = SPI_MODE_MASTER;
    hspi2.Init.Direction = SPI_DIRECTION_2LINES;
    hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi2.Init.NSS = SPI_NSS_SOFT;
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
    hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi2.Init.CRCPolynomial = 7;
    hspi2.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
    if (HAL_SPI_Init(&hspi2) != HAL_OK)
    {
        Error_Handler();
    }
}

// DMA Initialization
static void MX_DMA_Init(void)
{

    // SPI2_TX DMA
    hdma_spi2_tx.Instance = DMA1_Stream4;
    hdma_spi2_tx.Init.Channel = DMA_CHANNEL_0;
    hdma_spi2_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_spi2_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_spi2_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_spi2_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_spi2_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_spi2_tx.Init.Mode = DMA_NORMAL;
    hdma_spi2_tx.Init.Priority = DMA_PRIORITY_LOW;
    hdma_spi2_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    HAL_DMA_Init(&hdma_spi2_tx);
    __HAL_LINKDMA(&hspi2, hdmatx, hdma_spi2_tx);

    // SPI2_RX DMA
    hdma_spi2_rx.Instance = DMA1_Stream3;
    hdma_spi2_rx.Init.Channel = DMA_CHANNEL_0;
    hdma_spi2_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_spi2_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_spi2_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_spi2_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_spi2_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_spi2_rx.Init.Mode = DMA_NORMAL;
    hdma_spi2_rx.Init.Priority = DMA_PRIORITY_LOW;
    hdma_spi2_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    HAL_DMA_Init(&hdma_spi2_rx);
    __HAL_LINKDMA(&hspi2, hdmarx, hdma_spi2_rx);

    HAL_NVIC_SetPriority(DMA1_Stream3_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream3_IRQn);
    HAL_NVIC_SetPriority(DMA1_Stream4_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);
}

// GPIO Initialization
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // SPI2 GPIO Configuration: PB13=SCK, PB14=MISO, PB15=MOSI
    GPIO_InitStruct.Pin = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

void DMA1_Stream3_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_spi2_rx);
}

void DMA1_Stream4_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_spi2_tx);
}

// Error Handler
void Error_Handler(void)
{
    __disable_irq();
    printf("Error");
    while (1) { }
}
