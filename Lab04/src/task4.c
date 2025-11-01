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
/* ----- Part 4 IIR (float, FPU-ASM) -----
   y = 0.3125*x0 + 0.240385*x1 + 0.3125*x2 + 0.296875*y1
*/
static const float Cx0 = 0.3125f;     // x0
static const float Cx1 = 0.240385f;   // x1
static const float Cx2 = 0.3125f;     // x2
static const float Cy1 = 0.296875f;   // y1

static float fx1 = 0.f, fx2 = 0.f, fy1 = 0.f;   // filter state


/* globals */
static ADC_HandleTypeDef hadc1;
static DAC_HandleTypeDef hdac;
static TIM_HandleTypeDef htim2;   // ADC trigger timer
#if ENABLE_DAC_X2_UPSAMP
static TIM_HandleTypeDef htim6;   // DAC trigger timer (2*Fs)
#endif

/* IIR state (Q15 domain on 12-bit samples) */


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

    /* current input sample (0..4095) as float */
    float x0 = (float)(HAL_ADC_GetValue(&hadc1) & 0x0FFF);
    float y;  /* output accumulator */

    /* Pointers for inline asm loads */
    register const float *p_y1 = &fy1;
    register const float *p_x2 = &fx2;
    register const float *p_x1 = &fx1;
    register const float *p_x0 = &x0;

    register const float *pc_y1 = &Cy1;
    register const float *pc_x2 = &Cx2;
    register const float *pc_x1 = &Cx1;
    register const float *pc_x0 = &Cx0;

    /*  y = Cy1*y1
        y += Cx2*x2
        y += Cx1*x1
        y += Cx0*x0
        (All in FPU using VMLA.F32)  */
    __asm volatile(
        "vldr    s0, [%[py1]]      \n"  /* s0 = y1        */
        "vldr    s1, [%[c_y1]]     \n"  /* s1 = Cy1       */
        "vmul.f32 s0, s0, s1       \n"  /* s0 = Cy1*y1    */

        "vldr    s2, [%[px2]]      \n"  /* s2 = x2        */
        "vldr    s3, [%[c_x2]]     \n"  /* s3 = Cx2       */
        "vmla.f32 s0, s2, s3       \n"  /* s0 += Cx2*x2   */

        "vldr    s4, [%[px1]]      \n"  /* s4 = x1        */
        "vldr    s5, [%[c_x1]]     \n"  /* s5 = Cx1       */
        "vmla.f32 s0, s4, s5       \n"  /* s0 += Cx1*x1   */

        "vldr    s6, [%[px0]]      \n"  /* s6 = x0        */
        "vldr    s7, [%[c_x0]]     \n"  /* s7 = Cx0       */
        "vmla.f32 s0, s6, s7       \n"  /* s0 += Cx0*x0   */

        "vstr    s0, [%[py]]       \n"  /* store y        */
        :
        : [py1]  "r"(p_y1),
          [px2]  "r"(p_x2),
          [px1]  "r"(p_x1),
          [px0]  "r"(p_x0),
          [c_y1] "r"(pc_y1),
          [c_x2] "r"(pc_x2),
          [c_x1] "r"(pc_x1),
          [c_x0] "r"(pc_x0),
          [py]   "r"(&y)
        : "s0","s1","s2","s3","s4","s5","s6","s7","memory","cc"
    );

    /* shift state and output */
    fx2 = fx1; fx1 = x0; fy1 = y;

    /* clamp to 12-bit and send to DAC */
    uint16_t out = (y <= 0.f) ? 0 : (y >= 4095.f ? 4095 : (uint16_t)(y + 0.5f));
    HAL_DAC_SetValue(&hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, out);
}


/* required IRQ wrapper */
void ADC_IRQHandler(void)
{
    HAL_ADC_IRQHandler(&hadc1);
}
