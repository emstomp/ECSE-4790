//--------------------------------
// Lab 4 - Part 4: high-Fs + Q15 IIR (+optional x2 upsampler)
// STM32F769I-DISCO  |  ADC: PA6 (ADC1_IN6)  |  DAC: PA4 (DAC1_OUT1)
//--------------------------------
#include "init.h"
#include "stm32f7xx_hal.h"
#include <stdint.h>

/* ===== knobs ===== */
#define FS_HZ                 192000U                 // ADC sample rate (push high to reduce steps)
#define ADC_SAMPLE_TIME       ADC_SAMPLETIME_3CYCLES  // short sample; raise if your source is high-Z
#define ENABLE_DAC_X2_UPSAMP  0                       // 1 = drive DAC at 2*Fs with linear interp

/* pins/channels */
#define ADC_CH    ADC_CHANNEL_6   // A0 = PA6
#define DAC_CH    DAC_CHANNEL_1   // A1 = PA4

/* Q15 coefficients for Part-4 IIR:
   y = 0.3125*x0 + 0.240385*x1 + 0.3125*x2 + 0.296875*y1
   Q15 scale = 32768
*/
#define Q15(x)    ((int32_t)((x) * 32768.0f + 0.5f))
static const int32_t C0 = Q15(0.3125f);    // 10240
static const int32_t C1 = Q15(0.240385f);  //  7865
static const int32_t C2 = Q15(0.3125f);    // 10240
static const int32_t A1 = Q15(0.296875f);  //  9728

/* globals */
static ADC_HandleTypeDef hadc1;
static DAC_HandleTypeDef hdac;
static TIM_HandleTypeDef htim2;   // ADC trigger timer
#if ENABLE_DAC_X2_UPSAMP
static TIM_HandleTypeDef htim6;   // DAC trigger timer (2*Fs)
#endif

/* IIR state (Q15 domain on 12-bit samples) */
static int32_t x1=0, x2=0, y1=0;   // Q0 inputs promoted to Q15 in MACs

/* latest/next outputs (for optional x2 interpolation) */
static volatile uint16_t y_curr = 0, y_next = 0;

/* protos */
static void nvic_enable(void);
static void tim2_init_forFs(uint32_t fs_hz);
static void adc_init_trgo(void);
static void dac_init(void);
#if ENABLE_DAC_X2_UPSAMP
static void tim6_init_for2Fs(uint32_t fs_hz);
#endif
static inline uint16_t sat12_q15(int32_t acc);

/* ===== MAIN ===== */
int main(void)
{
    Sys_Init();

    nvic_enable();
    tim2_init_forFs(FS_HZ);
    adc_init_trgo();
    dac_init();
#if ENABLE_DAC_X2_UPSAMP
    tim6_init_for2Fs(FS_HZ);
#endif

    HAL_ADC_Start_IT(&hadc1);
    HAL_TIM_Base_Start(&htim2);
#if ENABLE_DAC_X2_UPSAMP
    HAL_TIM_Base_Start(&htim6);
#endif

    while (1) {
        // work happens in callbacks
    }
}

/* ===== NVIC ===== */
static void nvic_enable(void)
{
    HAL_NVIC_SetPriority(ADC_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(ADC_IRQn);
#if ENABLE_DAC_X2_UPSAMP
    HAL_NVIC_SetPriority(TIM6_DAC_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
#endif
}

/* ===== TIM2 -> TRGO at Fs ===== */
static void tim2_init_forFs(uint32_t fs_hz)
{
    __HAL_RCC_TIM2_CLK_ENABLE();

    uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
    uint32_t ppre1 = ((RCC->CFGR >> 10) & 7U);
    uint32_t tclk  = (ppre1 >= 4) ? (pclk1 * 2U) : pclk1;

    uint32_t psc = (tclk / 1000000U) - 1U;       // 1 MHz tick
    uint32_t arr = (1000000U / fs_hz) - 1U;      // Fs

    htim2.Instance = TIM2;
    htim2.Init.Prescaler         = psc;
    htim2.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim2.Init.Period            = arr;
    htim2.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_Base_Init(&htim2);

    TIM_MasterConfigTypeDef m = {0};
    m.MasterOutputTrigger = TIM_TRGO_UPDATE;
    m.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
    HAL_TIMEx_MasterConfigSynchronization(&htim2, &m);
}

/* ===== ADC (ext-triggered by TIM2 TRGO) ===== */
static void adc_init_trgo(void)
{
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_6; g.Mode = GPIO_MODE_ANALOG; g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &g);

    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4;   // ~27 MHz ADC clk (<=36 MHz)
    hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.ScanConvMode          = ADC_SCAN_DISABLE;
    hadc1.Init.ContinuousConvMode    = DISABLE;                    // driven by timer
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.NbrOfConversion       = 1;
    hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_RISING;
    hadc1.Init.ExternalTrigConv      = ADC_EXTERNALTRIGCONV_T2_TRGO;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    HAL_ADC_Init(&hadc1);

    ADC_ChannelConfTypeDef s = {0};
    s.Channel      = ADC_CH;
    s.Rank         = 1;
    s.SamplingTime = ADC_SAMPLE_TIME;  // 3 cycles → total ~15.5 cycles
    HAL_ADC_ConfigChannel(&hadc1, &s);
}

/* ===== DAC direct-write (buffer ON) ===== */
static void dac_init(void)
{
    __HAL_RCC_DAC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_4; g.Mode = GPIO_MODE_ANALOG; g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &g);

    hdac.Instance = DAC;
    HAL_DAC_Init(&hdac);

    DAC_ChannelConfTypeDef c = {0};
    c.DAC_Trigger      = DAC_TRIGGER_NONE;        // we push each sample
    c.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
    HAL_DAC_ConfigChannel(&hdac, &c, DAC_CH);
    HAL_DAC_Start(&hdac, DAC_CH);
}

#if ENABLE_DAC_X2_UPSAMP
/* ===== TIM6 -> optional 2*Fs DAC tick (linear interp) ===== */
static void tim6_init_for2Fs(uint32_t fs_hz)
{
    __HAL_RCC_TIM6_CLK_ENABLE();

    uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
    uint32_t ppre1 = ((RCC->CFGR >> 10) & 7U);
    uint32_t tclk  = (ppre1 >= 4) ? (pclk1 * 2U) : pclk1;

    uint32_t psc = (tclk / 1000000U) - 1U;                 // 1 MHz
    uint32_t arr = (1000000U / (2U*fs_hz)) - 1U;           // 2*Fs

    htim6.Instance = TIM6;
    htim6.Init.Prescaler         = psc;
    htim6.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim6.Init.Period            = arr;
    htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_Base_Init(&htim6);

    __HAL_TIM_CLEAR_FLAG(&htim6, TIM_FLAG_UPDATE);
    __HAL_TIM_ENABLE_IT(&htim6, TIM_IT_UPDATE);
}

/* IRQ: push y_curr / mid-point between y_curr and y_next */
void TIM6_DAC_IRQHandler(void)
{
    if (__HAL_TIM_GET_FLAG(&htim6, TIM_FLAG_UPDATE) != RESET) {
        if (__HAL_TIM_GET_IT_SOURCE(&htim6, TIM_IT_UPDATE) != RESET) {
            __HAL_TIM_CLEAR_IT(&htim6, TIM_IT_UPDATE);
            static uint8_t phase = 0;
            uint16_t out = (!phase) ? y_curr : (uint16_t)((y_curr + y_next) >> 1);
            phase ^= 1;
            HAL_DAC_SetValue(&hdac, DAC_CH, DAC_ALIGN_12B_R, out);
        }
    }
}
#endif

/* ===== ADC EOC ISR: Q15 IIR, update DAC (or y_next for x2 mode) ===== */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance != ADC1) return;

    int32_t x0 = (int32_t)(HAL_ADC_GetValue(&hadc1) & 0x0FFF); // 0..4095

    // Q15 MAC: acc = C0*x0 + C1*x1 + C2*x2 + A1*y1
    // Inputs are Q0; coefficients Q15 => acc is Q15.
    int32_t acc = 0;
    acc += C0 * x0;
    acc += C1 * x1;
    acc += C2 * x2;
    acc += A1 * y1;
    acc >>= 15;                 // back to Q0 domain

    if (acc < 0)       acc = 0;
    else if (acc > 4095) acc = 4095;

    // shift state
    x2 = x1; x1 = x0; y1 = acc;  // store y1 in Q0, ok (we only multiply by Q15 next time)

#if ENABLE_DAC_X2_UPSAMP
    // produce next pair for the DAC ISR
    y_next = (uint16_t)acc;
    y_curr = y_next;   // ensures continuity; DAC ISR interpolates mid-points
#else
    // direct write at Fs
    HAL_DAC_SetValue(&hdac, DAC_CH, DAC_ALIGN_12B_R, (uint16_t)acc);
#endif
}

/* required IRQ wrapper */
void ADC_IRQHandler(void)
{
    HAL_ADC_IRQHandler(&hadc1);
}
