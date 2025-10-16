//----------------------------------
// Lab 2 - Combined Parts (Modified)
//----------------------------------

#include "init.h"
#include "stm32f769xx.h"
#include "stm32f7xx_hal.h"
#include <stdio.h>

// ===========================================
// Select which part to run
// ===========================================
// 1 = Part1 (GPIO interrupts)
// 2 = Part2 Register Timer
// 3 = Part2 HAL Timer
// 4 = Depth Task
#define SELECT_PART 4

// ===========================================
// Globals used by various parts
// ===========================================
volatile uint8_t reg_button_pressed = 0;
volatile uint8_t hal_button_pressed = 0;

volatile uint32_t tenths = 0; // for Part2 Register
TIM_HandleTypeDef htim7;      // for Part2 HAL and Part 4
TIM_HandleTypeDef htim6;      // for Part 4

// ===========================================
// Prototypes
// ===========================================
void part1_main(void);
void part2reg_main(void);
void part2hal_main(void);
void depth_main(void);

// ===========================================
// Main
// ===========================================
int main(void) {
    // System Initialization
    Sys_Init();
    // Clear the console screen
    printf("\033[2J\033[;H");
    fflush(stdout);

#if SELECT_PART == 1
    part1_main();
#elif SELECT_PART == 2
    part2reg_main();
#elif SELECT_PART == 3
    part2hal_main();
#elif SELECT_PART == 4
    depth_main();
#else
    #error "SELECT_PART must be 1–4"
#endif

    // Infinite loop
    while (1) {}
}

// ===========================================
// Part 1: GPIO Interrupts
// ===========================================
#if SELECT_PART == 1

void Init_GPIO_Part1() {
    // PC7 as input with pull-down, EXTI7 (Register-based)
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    GPIOC->MODER &= ~(3U << (7*2));
    GPIOC->PUPDR &= ~(3U << (7*2));
    GPIOC->PUPDR |=  (2U << (7*2)); // pull-down

    SYSCFG->EXTICR[1] &= ~(0xF << 12);
    SYSCFG->EXTICR[1] |=  (0x2 << 12); // Port C for EXTI7
    EXTI->IMR  |= (1U << 7);
    EXTI->RTSR |= (1U << 7);
    EXTI->FTSR &= ~(1U << 7); // Rising edge only for simplicity
    NVIC_EnableIRQ(EXTI9_5_IRQn);

    // PC6 as input, HAL-managed
    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin  = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    // NVIC config for EXTI line 6
    HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
}

void part1_main(void) {
    Init_GPIO_Part1();
    while (1) {
        if (reg_button_pressed) {
            printf("Register-based EXTI interrupt!\r\n");
            fflush(stdout);
            reg_button_pressed = 0;
        }
        if (hal_button_pressed) {
            printf("HAL-based EXTI interrupt!\r\n");
            fflush(stdout);
            hal_button_pressed = 0;
        }
    }
}

void EXTI9_5_IRQHandler(void) {
    // Handle register-based EXTI7
    if (EXTI->PR & (1U << 7)) {
        EXTI->PR = (1U << 7); // Clear pending bit
        reg_button_pressed = 1;
    }
    // Forward to HAL for other EXTI lines in this range (e.g., EXTI6)
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_6);
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == GPIO_PIN_6) {
        hal_button_pressed = 1;
    }
}
#endif

// ===========================================
// Part 2: Timer (Register Implementation)
// ===========================================
#if SELECT_PART == 2

void Init_Timer_Reg(void) {
    __HAL_RCC_TIM6_CLK_ENABLE();
    NVIC_EnableIRQ(TIM6_DAC_IRQn);
    TIM6->PSC = 10800 - 1;   // 108MHz / 10800 = 10 kHz
    TIM6->ARR = 1000 - 1;    // 1000 counts / 10 kHz = 0.1s
    TIM6->DIER |= TIM_DIER_UIE; // Enable update interrupt
    TIM6->CR1  |= TIM_CR1_CEN;  // Enable timer
}

void part2reg_main(void) {
    Init_Timer_Reg();
    while (1) {}
}

void TIM6_DAC_IRQHandler(void) {
    if (TIM6->SR & TIM_SR_UIF) {
        TIM6->SR &= ~TIM_SR_UIF;
        tenths++;
        printf("Elapsed time: %lu tenths of a second\r\n", tenths);
        fflush(stdout);
    }
}
#endif

// ===========================================
// Part 2: Timer (HAL Implementation)
// ===========================================
#if SELECT_PART == 3

uint32_t period = 1;

void Init_Timer_HAL(void) {
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	__HAL_RCC_GPIOJ_CLK_ENABLE();
	GPIO_InitStruct.Pin = GPIO_PIN_13;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOJ, &GPIO_InitStruct);

	__HAL_RCC_TIM7_CLK_ENABLE();
    htim7.Instance = TIM7;
    htim7.Init.Prescaler = 108 - 1;   // 108MHz / 108 = 1 MHz
    htim7.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim7.Init.Period = 1000 - 1;     // 1000 counts / 1 MHz = 1 ms
    htim7.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_Base_Init(&htim7);

    HAL_NVIC_SetPriority(TIM7_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(TIM7_IRQn);
}

void part2hal_main(void) {
    Init_Timer_HAL();
    HAL_TIM_Base_Start_IT(&htim7);
    while (1) {}
}

void TIM7_IRQHandler(void) {
    HAL_TIM_IRQHandler(&htim7);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM7) {
        HAL_GPIO_TogglePin(GPIOJ, GPIO_PIN_13);
        printf("HAL Timer tick: %lu ms\r\n", period);
        period++;
        if (period > 100) {
            period = 1;
        }
        __HAL_TIM_SET_AUTORELOAD(htim, period * 1000 - 1);
	}
}
#endif

// ===========================================
// Part 4: Depth Task (Button Number Entry) - REVISED
// ===========================================
#if SELECT_PART == 4

// --- Globals for Part 4 ---
volatile uint32_t recorded_numbers[100];
volatile uint8_t  num_count = 0;
volatile uint32_t current_num = 0;
volatile uint32_t place_value = 1;
volatile uint8_t  is_button_pressed = 0;

// --- Timer and GPIO Initializations for Part 4 ---

// TIM6 is used for the 1-second INACTIVITY timer.
void Init_Inactivity_Timer(void) {
    __HAL_RCC_TIM6_CLK_ENABLE();
    TIM6->PSC = 10800 - 1;   // Prescaler for 10kHz from 108MHz clock
    TIM6->ARR = 10000 - 1;   // Auto-reload for 1 second (10000 counts @ 10kHz)
    TIM6->DIER |= TIM_DIER_UIE;
    TIM6->CR1 &= ~TIM_CR1_CEN;  // Ensure timer is disabled initially
    NVIC_EnableIRQ(TIM6_DAC_IRQn);
}

// TIM7 is used to measure the DURATION of a button press.
void Init_Hold_Timer(void) {
    __HAL_RCC_TIM7_CLK_ENABLE();
    TIM7->PSC = 108000 - 1;  // Prescaler for 1kHz (1ms ticks) from 108MHz clock
    TIM7->ARR = 0xFFFF;      // Max period, we'll read the counter directly
    TIM7->CR1 &= ~TIM_CR1_CEN; // Ensure timer is disabled initially
}

void Init_GPIO_Button(void) {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_SYSCFG_CLK_ENABLE();

    // Configure PA0 as an input with pull-down
    GPIOA->MODER &= ~(GPIO_MODER_MODER0);
    GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPDR0);
    GPIOA->PUPDR |=  (GPIO_PUPDR_PUPDR0_1);

    // Connect EXTI Line 0 to GPIOA
    SYSCFG->EXTICR[0] &= ~(SYSCFG_EXTICR1_EXTI0);
    SYSCFG->EXTICR[0] |=  SYSCFG_EXTICR1_EXTI0_PA;

    // Configure EXTI Line 0 for both rising and falling edges
    EXTI->IMR  |= EXTI_IMR_IM0;
    EXTI->RTSR |= EXTI_RTSR_TR0;
    EXTI->FTSR |= EXTI_FTSR_TR0;

    HAL_NVIC_SetPriority(EXTI0_IRQn, 2, 0);
    NVIC_EnableIRQ(EXTI0_IRQn);
}

// --- Main Function for Part 4 ---
void depth_main(void) {
    printf("Part 4: Depth Task - Button Number Entry\r\n");
    printf("Instructions:\r\n");
    printf("- Quick press (<1s): Increment current digit.\r\n");
    printf("- Hold (1-3s): Move to the next digit (tens, hundreds...).\r\n");
    printf("- Long hold (>3s): Print all recorded numbers.\r\n");
    printf("- Inactivity (>1s): Saves the current number and resets.\r\n");
    fflush(stdout);

    Init_GPIO_Button();
    Init_Inactivity_Timer();
    Init_Hold_Timer();
}

// --- Interrupt Handlers for Part 4 ---

void EXTI0_IRQHandler(void) {
    if (EXTI->PR & EXTI_PR_PR0) {
        EXTI->PR = EXTI_PR_PR0; // Clear the interrupt flag

        uint8_t button_state = (GPIOA->IDR & GPIO_PIN_0);

        if (button_state && !is_button_pressed) {
            is_button_pressed = 1;
            TIM6->CR1 &= ~TIM_CR1_CEN; // Stop inactivity timer
            TIM7->CNT = 0;
            TIM7->CR1 |= TIM_CR1_CEN; // Start hold timer
        }
        else if (!button_state && is_button_pressed) {
            is_button_pressed = 0;
            TIM7->CR1 &= ~TIM_CR1_CEN; // Stop hold timer
            uint32_t hold_duration_ms = TIM7->CNT;

            if (hold_duration_ms >= 3000) {
                printf("\r\n--- Recorded Numbers ---\r\n");
                if (num_count == 0) printf("No numbers recorded yet.\r\n");
                else {
                    for (uint8_t i = 0; i < num_count; i++) {
                        printf("Entry %d: %lu\r\n", i, recorded_numbers[i]);
                    }
                }
                printf("------------------------\r\n");
            }
            else if (hold_duration_ms >= 1000) {
                place_value *= 10;
                printf("-> Moved to next digit. Current Number: %lu\r\n", current_num);
            }
            else {
                uint32_t current_digit = (current_num / place_value) % 10;
                current_num -= current_digit * place_value;
                current_digit = (current_digit + 1) % 10;
                current_num += current_digit * place_value;
                printf("Current Number: %lu\r\n", current_num);
            }
            TIM6->CNT = 0;
            TIM6->CR1 |= TIM_CR1_CEN; // Restart inactivity timer
            fflush(stdout);
        }
    }
}

void TIM6_DAC_IRQHandler(void) {
    if (TIM6->SR & TIM_SR_UIF) {
        TIM6->SR &= ~TIM_SR_UIF; // Clear flag
        TIM6->CR1 &= ~TIM_CR1_CEN; // Stop timer

        if (current_num > 0 || place_value > 1) {
            if (num_count < 100) {
                recorded_numbers[num_count++] = current_num;
                printf("\r\n(Inactivity) Number saved: %lu. Ready for next number.\r\n", current_num);
            } else {
                printf("\r\nNumber array full. Cannot save %lu.\r\n", current_num);
            }
            current_num = 0;
            place_value = 1;
            fflush(stdout);
        }
    }
}
#endif

