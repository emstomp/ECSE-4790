/***************************************************************
 * Lab 5 – DMA Tasks 3 & 4
 * Target: STM32F769I-DISCO
 *
 * TASK SELECT:
 *   Exactly ONE of these should be 1 at a time.
 ***************************************************************/
#define RUN_TASK3_IIR_DMA   1   /* ADC->DMA circular + IIR on half-buffers + DAC<-DMA (TIM2 TRGO) */
#define RUN_TASK4_UART_DMA  0   /* UART DMA file transfer with u8/u16/u32 using DMA FIFO */

/* ================== Common Includes ================== */
#include "init.h"
#include "stm32f7xx_hal.h"
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

/* ================== Clocks/Helpers ================== */
static void Error_Halt(const char* msg){ (void)msg; __disable_irq(); while(1){} }

/* =====================================================
 * ===============  TASK 3: IIR + DMA  =================
 * ===================================================== */
#if RUN_TASK3_IIR_DMA

/* ---------- Rates / Sizes ---------- */
#define FS_HZ                 100000U           /* ~100 kHz per spec suggestion */
#define ADC_SAMPLE_TIME       ADC_SAMPLETIME_3CYCLES
#define BLOCK_LEN             16               /* per half-buffer */
#define BUF_LEN               (2*BLOCK_LEN)     /* circular DMA size */

/* ---------- Pins/Periphs ---------- */
#define ADC_CH                ADC_CHANNEL_6     /* PA6 (A0) */
#define DAC_CH                DAC_CHANNEL_1     /* PA4 (A1) */

/* ---------- IIR (float, like your Lab 4) ----------
   y = 0.3125*x0 + 0.240385*x1 + 0.3125*x2 + 0.296875*y1
*/
static const float Cx0 = 0.3125f, Cx1 = 0.240385f, Cx2 = 0.3125f, Cy1 = 0.296875f;
static float fx1=0.f, fx2=0.f, fy1=0.f;

/* ---------- Handles ---------- */
static ADC_HandleTypeDef hadc1;
static DAC_HandleTypeDef hdac;
static TIM_HandleTypeDef htim2;
static DMA_HandleTypeDef hdma_adc1;
static DMA_HandleTypeDef hdma_dac1;

/* ---------- Buffers (global/static per DMA rules) ---------- */
static volatile uint16_t adc_buf[BUF_LEN];
static volatile uint16_t dac_buf[BUF_LEN];

/* ---------- Prototypes ---------- */
static void tim2_init_forFs(uint32_t fs_hz);
static void adc_init_dma_trgo(void);
static void dac_init_dma_trgo(void);
static void nvic_init_task3(void);
static void process_block(size_t idx0, size_t count);

/* ================== MAIN (Task 3) ================== */
int main(void)
{
    Sys_Init();

    /* Zero the output buffer before enabling DAC DMA */
    for (size_t i=0;i<BUF_LEN;i++) dac_buf[i]=0;

    tim2_init_forFs(FS_HZ);
    adc_init_dma_trgo();
    dac_init_dma_trgo();
    nvic_init_task3();

    /* Start DMA streams (order matters a bit so DAC has valid data first) */
    if (HAL_DAC_Start_DMA(&hdac, DAC_CH, (uint32_t*)dac_buf, BUF_LEN, DAC_ALIGN_12B_R) != HAL_OK) Error_Halt("DAC DMA start");
    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_buf, BUF_LEN) != HAL_OK) Error_Halt("ADC DMA start");

    /* Start the shared trigger (TIM2 TRGO) last */
    if (HAL_TIM_Base_Start(&htim2) != HAL_OK) Error_Halt("TIM2 start");

    /* Idle; work happens in DMA callbacks */
    while (1) { }
}

/* ============ Timer 2 -> TRGO at Fs ============ */
static void tim2_init_forFs(uint32_t fs_hz)
{
    __HAL_RCC_TIM2_CLK_ENABLE();

    uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
    uint32_t ppre1 = ((RCC->CFGR >> 10) & 7U);
    uint32_t tclk  = (ppre1 >= 4) ? (pclk1 * 2U) : pclk1;

    uint32_t psc = (tclk / 1000000U) - 1U;   /* 1 MHz tick */
    uint32_t arr = (1000000U / fs_hz) - 1U;  /* Fs */

    htim2.Instance = TIM2;
    htim2.Init.Prescaler         = psc;
    htim2.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim2.Init.Period            = arr;
    htim2.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim2) != HAL_OK) Error_Halt("TIM2 init");

    TIM_MasterConfigTypeDef m = {0};
    m.MasterOutputTrigger = TIM_TRGO_UPDATE;
    m.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &m) != HAL_OK) Error_Halt("TIM2 TRGO");
}

/* ============ ADC1 + DMA (circular), ext trig = TIM2 TRGO ============ */
void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1) {
        __HAL_RCC_ADC1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_DMA2_CLK_ENABLE();

        GPIO_InitTypeDef g = {0};
        g.Pin = GPIO_PIN_6; g.Mode = GPIO_MODE_ANALOG; g.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &g);

        /* ADC1 -> DMA2 Stream0 Channel0 (regular conv) */
        hdma_adc1.Instance                 = DMA2_Stream0;
        hdma_adc1.Init.Channel             = DMA_CHANNEL_0;
        hdma_adc1.Init.Direction           = DMA_PERIPH_TO_MEMORY;
        hdma_adc1.Init.PeriphInc           = DMA_PINC_DISABLE;
        hdma_adc1.Init.MemInc              = DMA_MINC_ENABLE;
        hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
        hdma_adc1.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
        hdma_adc1.Init.Mode                = DMA_CIRCULAR;
        hdma_adc1.Init.Priority            = DMA_PRIORITY_HIGH;
        hdma_adc1.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
        if (HAL_DMA_Init(&hdma_adc1) != HAL_OK) Error_Halt("DMA2_Stream0 ADC");

        __HAL_LINKDMA(hadc, DMA_Handle, hdma_adc1);

        /* NVIC for DMA2 Stream0 */
        HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 1, 0);
        HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
    }
}

static void adc_init_dma_trgo(void)
{
    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.ScanConvMode          = ADC_SCAN_DISABLE;
    hadc1.Init.ContinuousConvMode    = DISABLE;  /* external trigger */
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.NbrOfConversion       = 1;
    hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_RISING;
    hadc1.Init.ExternalTrigConv      = ADC_EXTERNALTRIGCONV_T2_TRGO;
    hadc1.Init.DMAContinuousRequests = ENABLE;   /* allow DMA circular */
    hadc1.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) Error_Halt("ADC init");

    ADC_ChannelConfTypeDef s = {0};
    s.Channel      = ADC_CH;
    s.Rank         = 1;
    s.SamplingTime = ADC_SAMPLE_TIME;
    if (HAL_ADC_ConfigChannel(&hadc1, &s) != HAL_OK) Error_Halt("ADC ch cfg");
}

/* ============ DAC + DMA (circular), trig = TIM2 TRGO ============ */
void HAL_DAC_MspInit(DAC_HandleTypeDef *hd)
{
    if (hd->Instance == DAC) {
        __HAL_RCC_DAC_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_DMA1_CLK_ENABLE();

        GPIO_InitTypeDef g = {0};
        g.Pin = GPIO_PIN_4; g.Mode = GPIO_MODE_ANALOG; g.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &g);

        /* DAC CH1 -> DMA1 Stream5 Channel7 (standard mapping) */
        hdma_dac1.Instance                 = DMA1_Stream5;
        hdma_dac1.Init.Channel             = DMA_CHANNEL_7;
        hdma_dac1.Init.Direction           = DMA_MEMORY_TO_PERIPH;
        hdma_dac1.Init.PeriphInc           = DMA_PINC_DISABLE;
        hdma_dac1.Init.MemInc              = DMA_MINC_ENABLE;
        hdma_dac1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD; /* DAC is 12-bit -> 16-bit */
        hdma_dac1.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
        hdma_dac1.Init.Mode                = DMA_CIRCULAR;
        hdma_dac1.Init.Priority            = DMA_PRIORITY_HIGH;
        hdma_dac1.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
        if (HAL_DMA_Init(&hdma_dac1) != HAL_OK) Error_Halt("DMA1_Stream5 DAC");

        __HAL_LINKDMA(hd, DMA_Handle1, hdma_dac1);

        /* NVIC for DMA1 Stream5 */
        HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 1, 0);
        HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
    }
}

static void dac_init_dma_trgo(void)
{
    hdac.Instance = DAC;
    if (HAL_DAC_Init(&hdac) != HAL_OK) Error_Halt("DAC init");

    DAC_ChannelConfTypeDef c = {0};
    c.DAC_Trigger      = DAC_TRIGGER_T2_TRGO;      /* same timer trigger as ADC */
    c.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
    if (HAL_DAC_ConfigChannel(&hdac, &c, DAC_CH) != HAL_OK) Error_Halt("DAC ch cfg");
}

/* ============ NVIC (ADC callbacks use DMA IRQs) ============ */
static void nvic_init_task3(void)
{
    /* DMA IRQs already enabled in MSP; nothing else needed here */
    (void)0;
}

/* ============ DMA IRQ wrappers ============ */
void DMA2_Stream0_IRQn_Handler_Alias(void) __attribute__((alias("DMA2_Stream0_IRQHandler")));
void DMA1_Stream5_IRQn_Handler_Alias(void) __attribute__((alias("DMA1_Stream5_IRQHandler")));

void DMA2_Stream0_IRQHandler(void){ HAL_DMA_IRQHandler(&hdma_adc1); }
void DMA1_Stream5_IRQHandler(void){ HAL_DMA_IRQHandler(&hdma_dac1); }

/* ============ ADC DMA half/complete callbacks ============ */
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance != ADC1) return;
    process_block(/*start*/0, /*count*/BLOCK_LEN);
}
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance != ADC1) return;
    process_block(/*start*/BLOCK_LEN, /*count*/BLOCK_LEN);
}

/* ============ Block IIR (in-place into dac_buf) ============ */
static void process_block(size_t idx0, size_t count)
{
    /* carry state across calls */
    float x1 = fx1, x2 = fx2, y1 = fy1;

    for (size_t i=0;i<count;i++){
        /* current input sample (0..4095) as float */
        float x0 = (float)(adc_buf[idx0 + i] & 0x0FFF);
        float y  = (Cy1*y1) + (Cx2*x2) + (Cx1*x1) + (Cx0*x0);

        /* shift state */
        x2 = x1; x1 = x0; y1 = y;

        /* clamp to 12-bit and store into DAC buffer */
        uint16_t out = (y <= 0.f) ? 0 : (y >= 4095.f ? 4095U : (uint16_t)(y + 0.5f));
        dac_buf[idx0 + i] = out;
    }

    /* commit state */
    fx1 = x1; fx2 = x2; fy1 = y1;
}

#endif /* RUN_TASK3_IIR_DMA */

/* =====================================================
 * ============  TASK 4: UART DMA “files”  =============
 * ===================================================== */
#if RUN_TASK4_UART_DMA

/* -------- UART we’ll use between boards --------
 * USART6: TX=PG14 (AF8), RX=PG9 (AF8) on Disco F769I
 */
static UART_HandleTypeDef huart6;
static DMA_HandleTypeDef  hdma_us6_tx;
static DMA_HandleTypeDef  hdma_us6_rx;

/* -------- Simple header -------- */
typedef struct __attribute__((packed)) {
    uint32_t magic;   /* 0xA5A55A5A */
    uint8_t  elem_sz; /* 1,2,4 */
    uint32_t length;  /* number of elements */
    uint32_t cksum;   /* simple 32-bit sum over payload bytes */
} file_hdr_t;

static uint32_t sum_bytes(const void* p, uint32_t n)
{
    const uint8_t* b=(const uint8_t*)p; uint32_t s=0;
    for (uint32_t i=0;i<n;i++) s += b[i];
    return s;
}

/* -------- Demo buffers (distinct types) -------- */
#define N_ELEMS  800
static uint8_t  buf_u8 [N_ELEMS];
static uint16_t buf_u16[N_ELEMS];
static uint32_t buf_u32[N_ELEMS];

/* -------- Protos -------- */
static void uart6_init_pins(void);
static void uart6_init_115200(void);
static void uart6_dma_init(void);
static HAL_StatusTypeDef uart_send_file_DMA(const void* buf, uint32_t elem_sz, uint32_t length);
static HAL_StatusTypeDef uart_recv_file_DMA(void* buf, uint32_t elem_sz, uint32_t length);

/* ================== MAIN (Task 4) ================== */
int main(void)
{
    Sys_Init();

    /* Fill identifiable patterns */
    for (uint32_t i=0;i<N_ELEMS;i++){
        buf_u8[i]  = (uint8_t)(i & 0xFF);
        buf_u16[i] = (uint16_t)(0x1234u + i);
        buf_u32[i] = 0xA5A50000u + i;
    }

    uart6_init_pins();
    uart6_dma_init();
    uart6_init_115200();

    /* Example: send all three when we boot — or wire this to keypresses */
    (void)uart_send_file_DMA(buf_u8,  1, N_ELEMS);
    HAL_Delay(10);
    (void)uart_send_file_DMA(buf_u16, 2, N_ELEMS);
    HAL_Delay(10);
    (void)uart_send_file_DMA(buf_u32, 4, N_ELEMS);

    while(1){ }
}

/* ---------- GPIO for USART6 ---------- */
static void uart6_init_pins(void)
{
    __HAL_RCC_GPIOG_CLK_ENABLE();
    GPIO_InitTypeDef g = {0};
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_PULLUP;
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF8_USART6;

    /* PG14 = TX, PG9 = RX */
    g.Pin = GPIO_PIN_14; HAL_GPIO_Init(GPIOG, &g);
    g.Pin = GPIO_PIN_9;  HAL_GPIO_Init(GPIOG, &g);
}

/* ---------- DMA for USART6 RX/TX ---------- */
static void uart6_dma_init(void)
{
    __HAL_RCC_DMA2_CLK_ENABLE();

    /* TX: DMA2 Stream6 Channel5 (USART6_TX) */
    hdma_us6_tx.Instance                 = DMA2_Stream6;
    hdma_us6_tx.Init.Channel             = DMA_CHANNEL_5;
    hdma_us6_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
    hdma_us6_tx.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_us6_tx.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_us6_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;    /* UART DR is byte */
    hdma_us6_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;    /* will override per transfer */
    hdma_us6_tx.Init.Mode                = DMA_NORMAL;
    hdma_us6_tx.Init.Priority            = DMA_PRIORITY_HIGH;
    hdma_us6_tx.Init.FIFOMode            = DMA_FIFOMODE_ENABLE;    /* needed for 16/32-bit packing */
    hdma_us6_tx.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
    hdma_us6_tx.Init.MemBurst            = DMA_MBURST_SINGLE;
    hdma_us6_tx.Init.PeriphBurst         = DMA_PBURST_SINGLE;
    if (HAL_DMA_Init(&hdma_us6_tx) != HAL_OK) Error_Halt("DMA2_Stream6");
    HAL_NVIC_SetPriority(DMA2_Stream6_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream6_IRQn);

    /* RX: DMA2 Stream1 Channel5 (USART6_RX) — optional */
    hdma_us6_rx.Instance                 = DMA2_Stream1;
    hdma_us6_rx.Init.Channel             = DMA_CHANNEL_5;
    hdma_us6_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    hdma_us6_rx.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_us6_rx.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_us6_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_us6_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_us6_rx.Init.Mode                = DMA_NORMAL;
    hdma_us6_rx.Init.Priority            = DMA_PRIORITY_HIGH;
    hdma_us6_rx.Init.FIFOMode            = DMA_FIFOMODE_ENABLE;
    hdma_us6_rx.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
    if (HAL_DMA_Init(&hdma_us6_rx) != HAL_OK) Error_Halt("DMA2_Stream1");
    HAL_NVIC_SetPriority(DMA2_Stream1_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream1_IRQn);
}

/* ---------- USART6 itself ---------- */
static void uart6_init_115200(void)
{
    __HAL_RCC_USART6_CLK_ENABLE();

    huart6.Instance        = USART6;
    huart6.Init.BaudRate   = 115200;
    huart6.Init.WordLength = UART_WORDLENGTH_8B;
    huart6.Init.StopBits   = UART_STOPBITS_1;
    huart6.Init.Parity     = UART_PARITY_NONE;
    huart6.Init.Mode       = UART_MODE_TX_RX;
    huart6.Init.HwFlowCtl  = UART_HWCONTROL_NONE;
    huart6.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart6) != HAL_OK) Error_Halt("USART6 init");

    /* Link DMA to UART (TX/RX) */
    __HAL_LINKDMA(&huart6, hdmatx, hdma_us6_tx);
    __HAL_LINKDMA(&huart6, hdmarx, hdma_us6_rx);

    /* NVIC: USART6 IRQ (for errors, etc.) */
    HAL_NVIC_SetPriority(USART6_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART6_IRQn);
}

/* ---------- IRQ wrappers ---------- */
void DMA2_Stream6_IRQHandler(void){ HAL_DMA_IRQHandler(&hdma_us6_tx); }
void DMA2_Stream1_IRQHandler(void){ HAL_DMA_IRQHandler(&hdma_us6_rx); }
void USART6_IRQHandler(void){ HAL_UART_IRQHandler(&huart6); }

/* ---------- Send “file”: header then payload via DMA ---------- */
static HAL_StatusTypeDef uart_send_file_DMA(const void* buf, uint32_t elem_sz, uint32_t length)
{
    if (!(elem_sz==1 || elem_sz==2 || elem_sz==4)) return HAL_ERROR;

    /* Build and send header (blocking OK) */
    file_hdr_t hdr;
    hdr.magic   = 0xA5A55A5Au;
    hdr.elem_sz = (uint8_t)elem_sz;
    hdr.length  = length;
    hdr.cksum   = sum_bytes(buf, length*elem_sz);

    if (HAL_UART_Transmit(&huart6, (uint8_t*)&hdr, sizeof(hdr), 100) != HAL_OK)
        return HAL_ERROR;

    /* Configure DMA memory width for this transfer */
    /* NOTE: UART DR is byte; FIFO packs halfword/word without us building a byte-copy */
    huart6.hdmatx->Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    huart6.hdmatx->Init.MemDataAlignment    = (elem_sz==1)? DMA_MDATAALIGN_BYTE :
                                              (elem_sz==2)? DMA_MDATAALIGN_HALFWORD : DMA_MDATAALIGN_WORD;
    huart6.hdmatx->Init.FIFOMode            = DMA_FIFOMODE_ENABLE;
    huart6.hdmatx->Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
    if (HAL_DMA_Init(huart6.hdmatx) != HAL_OK) return HAL_ERROR;

    /* Start UART DMA: HAL expects byte count; DMA packs from larger words via FIFO */
    return HAL_UART_Transmit_DMA(&huart6, (uint8_t*)buf, length*elem_sz);
}

/* Optionally implement uart_recv_file_DMA(...) similarly (receive header blocking; then HAL_UART_Receive_DMA) */
static HAL_StatusTypeDef uart_recv_file_DMA(void* buf, uint32_t elem_sz, uint32_t length)
{
    /* Receive header blocking */
    file_hdr_t hdr;
    if (HAL_UART_Receive(&huart6, (uint8_t*)&hdr, sizeof(hdr), 5000) != HAL_OK) return HAL_TIMEOUT;
    if (hdr.magic != 0xA5A55A5Au || hdr.elem_sz != elem_sz || hdr.length != length) return HAL_ERROR;

    /* Configure RX DMA to write into typed buffer using FIFO packing */
    huart6.hdmarx->Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    huart6.hdmarx->Init.MemDataAlignment    = (elem_sz==1)? DMA_MDATAALIGN_BYTE :
                                              (elem_sz==2)? DMA_MDATAALIGN_HALFWORD : DMA_MDATAALIGN_WORD;
    huart6.hdmarx->Init.FIFOMode            = DMA_FIFOMODE_ENABLE;
    huart6.hdmarx->Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
    if (HAL_DMA_Init(huart6.hdmarx) != HAL_OK) return HAL_ERROR;

    if (HAL_UART_Receive_DMA(&huart6, (uint8_t*)buf, length*elem_sz) != HAL_OK) return HAL_ERROR;

    /* wait (poll) for completion; in practice hook into DMA complete IRQ */
    while (HAL_UART_GetState(&huart6) != HAL_UART_STATE_READY) { /* spin */ }

    /* verify checksum */
    uint32_t s = sum_bytes(buf, length*elem_sz);
    if (s != hdr.cksum) return HAL_ERROR;
    return HAL_OK;
}

#endif /* RUN_TASK4_UART_DMA */
