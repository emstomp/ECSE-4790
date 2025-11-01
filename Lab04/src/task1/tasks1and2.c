//--------------------------------
// Lab 4 - Sample - Lab04_sample.c
//--------------------------------

#include "init.h"
#include "stm32f7xx_hal.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

/* =========================
   SELECT WHAT TO RUN
   ========================= */
#define RUN_TASK1_VOLT_METER        0   // Prints ADC hex, voltage, 10s running avg/min/max (100 ms sample)
#define RUN_TASK2_SAWTOOTH          0   // Outputs 12-bit sawtooth on DAC1 (PA4)
#define RUN_TASK2_ADC_TO_DAC        1   // Pass-through: ADC1 (PA6) -> DAC1 (PA4)

/* ===== Pins / Channels =====
   ADC input : PA6  (Arduino A0) -> ADC1_IN6
   DAC output: PA4  (Arduino A1) -> DAC1_OUT1
*/
#define VREF_MV                     3300U
#define ADC_MAX                     4095U

/* ===== Task 1 timing ===== */
#define SAMPLE_PERIOD_MS            1U
#define WINDOW_MS                   1000000U
#define RING_LEN                    (WINDOW_MS / SAMPLE_PERIOD_MS)

/* ====== Globals ====== */
static ADC_HandleTypeDef hadc1;
static DAC_HandleTypeDef hdac;

/* Prototypes */
static void configureADC_single(void);
static void configureADC_continuous(void);
static void configureDAC(void);
static void Error_Handler(const char *msg);

/* MSP hooks */
void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc);
void HAL_DAC_MspInit(DAC_HandleTypeDef *hdac);

/* Helpers */
static inline uint32_t adc_to_mV(uint32_t raw)
{
    return (raw * VREF_MV) / ADC_MAX;
}

int main(void)
{
    Sys_Init();

#if RUN_TASK1_VOLT_METER
    configureADC_single();

    static uint16_t ring[RING_LEN] = {0};
    uint32_t sum_mV = 0, count = 0, idx = 0;
    uint16_t cur_min = 0xFFFF, cur_max = 0;

    printf("\r\n[Task 1] Simple Voltmeter on PA6 (A0). Sampling every %lu ms.\r\n",
           (unsigned long)SAMPLE_PERIOD_MS);

    while (1)
    {
        if (HAL_ADC_Start(&hadc1) != HAL_OK) Error_Handler("ADC_Start");
        if (HAL_ADC_PollForConversion(&hadc1, 10) != HAL_OK) Error_Handler("ADC_Poll");
        uint32_t raw = HAL_ADC_GetValue(&hadc1);
        HAL_ADC_Stop(&hadc1);

        uint16_t mV = (uint16_t)adc_to_mV(raw);

        if (count < RING_LEN) {
            ring[idx] = mV; sum_mV += mV; count++;
        } else {
            uint16_t oldest = ring[idx];
            sum_mV = sum_mV - oldest + mV;
            ring[idx] = mV;
            if (oldest == cur_min || oldest == cur_max) {
                uint16_t tmin = 0xFFFF, tmax = 0;
                for (uint32_t i = 0; i < RING_LEN; i++) {
                    if (ring[i] < tmin) tmin = ring[i];
                    if (ring[i] > tmax) tmax = ring[i];
                }
                cur_min = tmin; cur_max = tmax;
            }
        }
        if (mV < cur_min) cur_min = mV;
        if (mV > cur_max) cur_max = mV;

        uint32_t avg_mV = (count == 0) ? 0 : (sum_mV / count);
        printf("ADC=0x%03lX  V=%.3f  avg(10s)=%.3f  min=%.3f  max=%.3f\r\n",
               (unsigned long)raw,
               (float)mV/1000.0f, (float)avg_mV/1000.0f,
               (float)cur_min/1000.0f, (float)cur_max/1000.0f);

        HAL_Delay(SAMPLE_PERIOD_MS);
        idx = (idx + 1) % RING_LEN;
    }
#endif

#if RUN_TASK2_SAWTOOTH
    configureDAC();
    printf("\r\n[Task 2A] DAC sawtooth on PA4 (A1). 12-bit wraparound.\r\n");
    uint32_t v = 0;
    while (1) {
        HAL_DAC_SetValue(&hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, (v & 0x0FFF));
        v++;
        // HAL_Delay(1);
    }
#endif
#if RUN_TASK2_ADC_TO_DAC
    // Use single-conversion config instead of continuous
    configureADC_single();
    configureDAC();

    //printf("\r\n[Task 2B] ADC1 (PA6) -> DAC1 (PA4) pass-through. Feed a sine into A0.\r\n");

    while (1) {
        if (HAL_ADC_Start(&hadc1) != HAL_OK) Error_Handler("ADC_Start");
        if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
            uint32_t raw = HAL_ADC_GetValue(&hadc1) & 0x0FFF;  // 12-bit sample
            HAL_DAC_SetValue(&hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, raw);
        }
        HAL_ADC_Stop(&hadc1);
        // Optional tiny delay to reduce CPU hammering; comment out for max rate
        // HAL_Delay(1);
    }
#endif


    while (1) { }
}

/* ===== ADC Configs ===== */
static void configureADC_single(void)
{
    __HAL_RCC_ADC1_CLK_ENABLE();

    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.ScanConvMode          = ADC_SCAN_DISABLE;
    hadc1.Init.ContinuousConvMode    = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.NbrOfConversion       = 1;
    hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) Error_Handler("HAL_ADC_Init (single)");

    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel      = ADC_CHANNEL_6;           // PA6
    sConfig.Rank         = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_56CYCLES;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) Error_Handler("HAL_ADC_ConfigChannel");
}

static void configureADC_continuous(void)
{
    __HAL_RCC_ADC1_CLK_ENABLE();

    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.ScanConvMode          = ADC_SCAN_DISABLE;
    hadc1.Init.ContinuousConvMode    = ENABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.NbrOfConversion       = 1;
    hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) Error_Handler("HAL_ADC_Init (cont)");

    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel      = ADC_CHANNEL_6;           // PA6
    sConfig.Rank         = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_56CYCLES;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) Error_Handler("HAL_ADC_ConfigChannel");
}

/* ===== DAC Config ===== */
static void configureDAC(void)
{
    __HAL_RCC_DAC_CLK_ENABLE();

    hdac.Instance = DAC;
    if (HAL_DAC_Init(&hdac) != HAL_OK) Error_Handler("HAL_DAC_Init");

    DAC_ChannelConfTypeDef sConf = {0};
    sConf.DAC_Trigger      = DAC_TRIGGER_NONE;
    sConf.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
    if (HAL_DAC_ConfigChannel(&hdac, &sConf, DAC_CHANNEL_1) != HAL_OK)
        Error_Handler("HAL_DAC_ConfigChannel");

    if (HAL_DAC_Start(&hdac, DAC_CHANNEL_1) != HAL_OK)
        Error_Handler("HAL_DAC_Start");
}

/* ===== MSP INIT: GPIO + Clocks ===== */
void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1) {
        __HAL_RCC_GPIOA_CLK_ENABLE();
        GPIO_InitTypeDef GPIO_InitStruct = {0};
        // PA6 -> ADC1_IN6 (A0)
        GPIO_InitStruct.Pin  = GPIO_PIN_6;
        GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }
}

void HAL_DAC_MspInit(DAC_HandleTypeDef *hdac)
{
    if (hdac->Instance == DAC) {
        __HAL_RCC_GPIOA_CLK_ENABLE();
        GPIO_InitTypeDef GPIO_InitStruct = {0};
        // PA4 -> DAC_OUT1 (A1)
        GPIO_InitStruct.Pin  = GPIO_PIN_4;
        GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }
}

/* ===== Error Handler ===== */
static void Error_Handler(const char *msg)
{
   // printf("ERROR: %s\r\n", msg);
    while (1) { }
}
