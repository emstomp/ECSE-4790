//----------------------------------
// Lab 2 - Combined Parts
//----------------------------------

#include "init.h"
#include "stm32f769xx.h"
#include "stm32f7xx_hal.h"

// ===========================================
// Select which part to run
// ===========================================
// 1 = Part1 (GPIO interrupts)
// 2 = Part2 Register Timer
// 3 = Part2 HAL Timer
// 4 = Depth Task
#define SELECT_PART 1

// ===========================================
// Globals used by various parts
// ===========================================
volatile uint8_t reg_button_pressed = 0;
volatile uint8_t hal_button_pressed = 0;

volatile uint32_t tenths = 0; // for Part2 Register
TIM_HandleTypeDef htim7;      // for Part2 HAL

volatile uint32_t numbers[100];
volatile uint8_t num_count = 0;
volatile uint32_t current_num = 0;
volatile uint8_t digit_pos = 0;

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
    Sys_Init();
    printf("\033[2J\033[;H"); // Clear the screen and move the cursor to the home position
    fflush(stdout); // Ensure the output buffer is flushed
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

    while (1) {}
}

// ===========================================
// Part 1: GPIO Interrupts
// ===========================================
#if SELECT_PART == 1

void Init_GPIO() {
    // PC7 as input with pull-down, EXTI7
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    GPIOC->MODER &= ~(3U << (7*2));
    GPIOC->PUPDR &= ~(3U << (7*2));
    GPIOC->PUPDR |=  (2U << (7*2)); // pull-down

    SYSCFG->EXTICR[1] &= ~(0xF << 12);
    SYSCFG->EXTICR[1] |=  (0x2 << 12); // Port C
    EXTI->IMR  |= (1U << 7);
    EXTI->RTSR |= (1U << 7);
    EXTI->FTSR |= (1U << 7);
    NVIC_EnableIRQ(EXTI9_5_IRQn);

    // PA0 as input, HAL-managed
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI0_IRQn);
}

void part1_main(void) {
    Init_GPIO();
    while (1) {
        if (reg_button_pressed) {
            printf("Register-based EXTI interrupt!\r\n");fflush(stdout); // Ensure the output buffer is flushed
            reg_button_pressed = 0;

        }
        if (hal_button_pressed) {
            printf("HAL-based EXTI interrupt!\r\n");fflush(stdout); // Ensure the output buffer is flushed
            hal_button_pressed = 0;

        }

    }
}

void EXTI9_5_IRQHandler(void) {
    if (EXTI->PR & (1U << 7)) {
        EXTI->PR |= (1U << 7);
        reg_button_pressed = 1;
    }
}

void EXTI0_IRQHandler(void) {
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
}
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == GPIO_PIN_0) {
        hal_button_pressed = 1;
    }
}
#endif

// ===========================================
// Part 2: Timer (Register Implementation)
// ===========================================
#if SELECT_PART == 2

void Init_Timer_Reg(void) {
    NVIC_EnableIRQ(TIM6_DAC_IRQn);
    TIM6->PSC = 10800 - 1;   // 10 kHz
    TIM6->ARR = 1000 - 1;    // 0.1s
    TIM6->DIER |= TIM_DIER_UIE;
    TIM6->CR1  |= TIM_CR1_CEN;
}

void part2reg_main(void) {
    Init_Timer_Reg();
    while (1) {}
}

void TIM6_DAC_IRQHandler(void) {
    if (TIM6->SR & TIM_SR_UIF) {
        TIM6->SR &= ~TIM_SR_UIF;
        tenths++;
        printf("Elapsed time: %lu tenths\r\n", tenths);fflush(stdout); // Ensure the output buffer is flushed
    }
}
#endif

// ===========================================
// Part 2: Timer (HAL Implementation)
// ===========================================
#if SELECT_PART == 3

uint32_t period = 1;

void Init_Timer_HAL(void) {
    __HAL_RCC_TIM7_CLK_ENABLE();
    htim7.Instance = TIM7;
    htim7.Init.Prescaler = 108 - 1;   // 1 MHz
    htim7.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim7.Init.Period = 1000 - 1;     // 1 ms
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
        printf("HAL Timer tick: %lu ms\r\n", period);
        period++;
        if (period > 100) period = 1;
        __HAL_TIM_SET_AUTORELOAD(htim, period * 1000 - 1);
    }
}
#endif

// ===========================================
// Part 4: Depth Task (Button Number Entry)
// ===========================================
#if SELECT_PART == 4

void Init_GPIO_Button(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    GPIOA->MODER &= ~(3U << (0*2));
    GPIOA->PUPDR &= ~(3U << (0*2));
    GPIOA->PUPDR |=  (2U << (0*2)); // pull-down

    SYSCFG->EXTICR[0] &= ~(0xF << 0);
    SYSCFG->EXTICR[0] |=  (0x0 << 0);

    EXTI->IMR  |= (1U << 0);
    EXTI->RTSR |= (1U << 0);
    EXTI->FTSR |= (1U << 0);

    NVIC_EnableIRQ(EXTI0_IRQn);
}

void Init_Timer_Depth(void) {
    NVIC_EnableIRQ(TIM6_DAC_IRQn);
    TIM6->PSC = 10800 - 1;   // 10 kHz
    TIM6->ARR = 10000 - 1;   // 1s
    TIM6->DIER |= TIM_DIER_UIE;
    TIM6->CR1  |= TIM_CR1_CEN;
}

void part4_accept_number(void) {
    numbers[num_count++] = current_num;
    printf("Number entered: %lu\r\n", current_num);
    current_num = 0;
    digit_pos = 0;
}

void depth_main(void) {
    Init_GPIO_Button();
    Init_Timer_Depth();
    while (1) {}
}

void EXTI0_IRQHandler(void) {
    if (EXTI->PR & (1U << 0)) {
        EXTI->PR |= (1U << 0);
        current_num = current_num * 10 + (digit_pos % 10);
        digit_pos++;
    }
}

void TIM6_DAC_IRQHandler(void) {
    if (TIM6->SR & TIM_SR_UIF) {
        TIM6->SR &= ~TIM_SR_UIF;
        part4_accept_number();
    }
}
#endif
