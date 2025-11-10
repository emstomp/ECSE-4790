#include "init.h" // Always need init.h, otherwise nothing will work.
#include<stdint.h>
#include<stdlib.h>


DMA_HandleTypeDef hdma_memtomem_dma2_stream0;

static void MX_DMA_Init(void);
void DmaXferCompleteCallback(DMA_HandleTypeDef *hdma);        // Data Transfer Complete callback

volatile uint8_t dma_done = 0;

void MemToMem(int buflen);

int main(void)
{
	Sys_Init();

	// Enable the DWT_CYCCNT register
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->LAR = 0xC5ACCE55;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

	MX_DMA_Init(); // initialize DMA
	hdma_memtomem_dma2_stream0.XferCpltCallback = &DmaXferCompleteCallback;

	MemToMem(10);
	MemToMem(100);
	MemToMem(1000);

	while(1);
}

void MemToMem(int buflen) {

	printf("\n\n======= buffer length: %d =======\r\n", buflen);

	/********** uint8_t **********/
	uint8_t *src8 = (uint8_t *)malloc(buflen * sizeof(uint8_t));
	uint8_t *dest8 = (uint8_t *)malloc(buflen * sizeof(uint8_t));

	DWT->CYCCNT = 0; // Clear the cycle counter

	for (int i = 0; i < buflen; i++) { dest8[i] = src8[i]; }

	uint32_t cycles = DWT->CYCCNT; // Store the cycle counter
	printf("copying uint8_t buffer without DMA took %lu CPU cycles\r\n", cycles);

    hdma_memtomem_dma2_stream0.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_memtomem_dma2_stream0.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    HAL_DMA_Init(&hdma_memtomem_dma2_stream0);
	dma_done = 0;
	DWT->CYCCNT = 0; // Clear the cycle counter
	HAL_DMA_Start_IT(&hdma_memtomem_dma2_stream0,(uint32_t)src8,(uint32_t)dest8,buflen);
	while (!dma_done);  // wait until callback fires

	cycles = DWT->CYCCNT; // Store the cycle counter
	printf("copying uint8_t buffer with DMA took %lu CPU cycles\r\n", cycles);

	/********** uint16_t **********/
	uint16_t *src16 = (uint16_t *)malloc(buflen * sizeof(uint16_t));
	uint16_t *dest16 = (uint16_t *)malloc(buflen * sizeof(uint16_t));

	DWT->CYCCNT = 0; // Clear the cycle counter

	for (int i = 0; i < buflen; i++) { dest16[i] = src16[i]; }

	cycles = DWT->CYCCNT; // Store the cycle counter
	printf("copying uint16_t buffer without DMA took %lu CPU cycles\r\n", cycles);

    hdma_memtomem_dma2_stream0.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_memtomem_dma2_stream0.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
    HAL_DMA_Init(&hdma_memtomem_dma2_stream0);
    dma_done = 0;
	DWT->CYCCNT = 0; // Clear the cycle counter
	HAL_DMA_Start_IT(&hdma_memtomem_dma2_stream0,(uint32_t)src16,(uint32_t)dest16,buflen);
	while (!dma_done);  // wait until callback fires

	cycles = DWT->CYCCNT; // Store the cycle counter
	printf("copying uint16_t buffer with DMA took %lu CPU cycles\r\n", cycles);

	/********** uint32_t **********/
	uint32_t *src32 = (uint32_t *)malloc(buflen * sizeof(uint32_t));
	uint32_t *dest32 = (uint32_t *)malloc(buflen * sizeof(uint32_t));

	DWT->CYCCNT = 0; // Clear the cycle counter

	for (int i = 0; i < buflen; i++) { dest32[i] = src32[i]; }

	cycles = DWT->CYCCNT; // Store the cycle counter
	printf("copying uint32_t buffer without DMA took %lu CPU cycles\r\n", cycles);

    hdma_memtomem_dma2_stream0.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
    hdma_memtomem_dma2_stream0.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
    HAL_DMA_Init(&hdma_memtomem_dma2_stream0);
	dma_done = 0;
	DWT->CYCCNT = 0; // Clear the cycle counter
	HAL_DMA_Start_IT(&hdma_memtomem_dma2_stream0,(uint32_t)src32,(uint32_t)dest32,buflen);
	while (!dma_done);  // wait until callback fires

	cycles = DWT->CYCCNT; // Store the cycle counter
	printf("copying uint32_t buffer with DMA took %lu CPU cycles\r\n", cycles);

}

static void MX_DMA_Init(void) {

  /* Configure DMA request hdma_memtomem_dma2_stream0 on DMA2_Stream0 */
  hdma_memtomem_dma2_stream0.Instance = DMA2_Stream0;
  hdma_memtomem_dma2_stream0.Init.Channel = DMA_CHANNEL_0;
  hdma_memtomem_dma2_stream0.Init.Direction = DMA_MEMORY_TO_MEMORY;
  hdma_memtomem_dma2_stream0.Init.PeriphInc = DMA_PINC_ENABLE;
  hdma_memtomem_dma2_stream0.Init.MemInc = DMA_MINC_ENABLE;
  hdma_memtomem_dma2_stream0.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  hdma_memtomem_dma2_stream0.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
  hdma_memtomem_dma2_stream0.Init.Mode = DMA_NORMAL;
  hdma_memtomem_dma2_stream0.Init.Priority = DMA_PRIORITY_LOW;
  hdma_memtomem_dma2_stream0.Init.FIFOMode = DMA_FIFOMODE_ENABLE;
  hdma_memtomem_dma2_stream0.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
  hdma_memtomem_dma2_stream0.Init.MemBurst = DMA_MBURST_SINGLE;
  hdma_memtomem_dma2_stream0.Init.PeriphBurst = DMA_PBURST_SINGLE;
  HAL_DMA_Init(&hdma_memtomem_dma2_stream0);

  /* DMA interrupt init */
  /* DMA2_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
}

void DmaXferCompleteCallback(DMA_HandleTypeDef *hdma) {
	dma_done = 1;
}

void DMA2_Stream0_IRQHandler(void) {
    HAL_DMA_IRQHandler(&hdma_memtomem_dma2_stream0);
}


