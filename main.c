// main.c  (FLASH SAME FILE ON BOTH BOARDS)
//
// UART between boards (Arduino header):
//   D1 = PC6 = USART6_TX
//   D0 = PC7 = USART6_RX
// Wiring: A D1->B D0, A D0<-B D1, GND<->GND
//
// Buttons (ACTIVE-LOW, PULL-UP):
//   UP    = D2 = PJ1
//   DOWN  = D3 = PF6
//   LEFT  = D4 = PJ0
//   RIGHT = D6 = PF7
//   FIRE  = D5 = PC8
//
// Audio PWM (piezo):
//   PH6 (often Arduino D9 on this board) = TIM12_CH1 PWM output
//
// Set which side THIS board is (one line only):
//   Left board:  #define BOARD_IS_LEFT 1
//   Right board: #define BOARD_IS_LEFT 0

#include "stm32f7xx_hal.h"
#include "stm32f769i_discovery.h"
#include "stm32f769i_discovery_lcd.h"
#include <stdio.h>

void SystemClock_Config(void); // defined in init.c (ONLY once)

#define BOARD_IS_LEFT 0   // <-- CHANGE TO 0 ON THE OTHER BOARD

// -------------------- Button pins --------------------
#define BTN_UP_PORT     GPIOJ
#define BTN_UP_PIN      GPIO_PIN_1   // D2

#define BTN_DOWN_PORT   GPIOF
#define BTN_DOWN_PIN    GPIO_PIN_6   // D3

#define BTN_LEFT_PORT   GPIOJ
#define BTN_LEFT_PIN    GPIO_PIN_0   // D4

#define BTN_RIGHT_PORT  GPIOF
#define BTN_RIGHT_PIN   GPIO_PIN_7   // D6

#define BTN_FIRE_PORT   GPIOC
#define BTN_FIRE_PIN    GPIO_PIN_8   // D5

static inline uint8_t pressed(GPIO_TypeDef *port, uint16_t pin)
{
  return (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_RESET); // active-low
}

// -------------------- UART6 on D0/D1 --------------------
static UART_HandleTypeDef huart6;

static void UART6_Init(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_USART6_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  // PC6 TX (D1), PC7 RX (D0)
  gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio.Alternate = GPIO_AF8_USART6;
  HAL_GPIO_Init(GPIOC, &gpio);

  huart6.Instance = USART6;
  huart6.Init.BaudRate = 115200;
  huart6.Init.WordLength = UART_WORDLENGTH_8B;
  huart6.Init.StopBits = UART_STOPBITS_1;
  huart6.Init.Parity = UART_PARITY_NONE;
  huart6.Init.Mode = UART_MODE_TX_RX;
  huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart6.Init.OverSampling = UART_OVERSAMPLING_16;

  if (HAL_UART_Init(&huart6) != HAL_OK) while (1) {}
}

static inline void UART6_ClearErrors(void)
{
  uint32_t isr = USART6->ISR;
  if (isr & USART_ISR_ORE) USART6->ICR = USART_ICR_ORECF;
  if (isr & USART_ISR_FE)  USART6->ICR = USART_ICR_FECF;
  if (isr & USART_ISR_NE)  USART6->ICR = USART_ICR_NCF;
  if (isr & USART_ISR_PE)  USART6->ICR = USART_ICR_PECF;
}

static inline int UART6_ReadByte(uint8_t *out)
{
  UART6_ClearErrors();
  if (USART6->ISR & USART_ISR_RXNE)
  {
    *out = (uint8_t)(USART6->RDR & 0xFF);
    return 1;
  }
  return 0;
}

static inline void UART6_SendByte(uint8_t b)
{
  HAL_UART_Transmit(&huart6, &b, 1, 50);
}

// -------------------- Buttons init --------------------
static void Buttons_Init(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOJ_CLK_ENABLE(); // D2 PJ1, D4 PJ0
  __HAL_RCC_GPIOF_CLK_ENABLE(); // D3 PF6, D6 PF7
  __HAL_RCC_GPIOC_CLK_ENABLE(); // D5 PC8

  gpio.Mode  = GPIO_MODE_INPUT;
  gpio.Pull  = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;

  gpio.Pin = BTN_UP_PIN | BTN_LEFT_PIN;
  HAL_GPIO_Init(GPIOJ, &gpio);

  gpio.Pin = BTN_DOWN_PIN | BTN_RIGHT_PIN;
  HAL_GPIO_Init(GPIOF, &gpio);

  gpio.Pin = BTN_FIRE_PIN;
  HAL_GPIO_Init(GPIOC, &gpio);
}

// -------------------- PWM Audio on PH6 / TIM12_CH1 --------------------
static TIM_HandleTypeDef htim12;

typedef enum {
  SFX_NONE = 0,
  SFX_FIRE,
  SFX_TX,
  SFX_RX,
  SFX_HIT,
  SFX_WIN,
  SFX_LOSE
} SfxId;

static const uint16_t sfx_fire_f[] = {1800};
static const uint16_t sfx_fire_d[] = {60};

static const uint16_t sfx_tx_f[]   = {1200};
static const uint16_t sfx_tx_d[]   = {20};

static const uint16_t sfx_rx_f[]   = {900};
static const uint16_t sfx_rx_d[]   = {20};

static const uint16_t sfx_hit_f[]  = {350, 0, 250};
static const uint16_t sfx_hit_d[]  = {120, 40, 140};

static const uint16_t sfx_win_f[]  = {900, 1400, 2000};
static const uint16_t sfx_win_d[]  = {80, 80, 140};

static const uint16_t sfx_lose_f[] = {300, 200};
static const uint16_t sfx_lose_d[] = {180, 220};

static const uint16_t *g_sfx_f = 0;
static const uint16_t *g_sfx_d = 0;
static uint8_t  g_sfx_len = 0;
static uint8_t  g_sfx_idx = 0;
static uint32_t g_sfx_step_end = 0;
static uint8_t  g_sfx_active = 0;

static uint32_t tim12_get_clk_hz(void)
{
  uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
  // If APB1 prescaler != 1, timer clock is doubled.
  uint32_t ppre1 = (RCC->CFGR & RCC_CFGR_PPRE1);
  if (ppre1 != RCC_CFGR_PPRE1_DIV1) return pclk1 * 2U;
  return pclk1;
}

static void Audio_SetTone(uint16_t freq_hz)
{
  if (freq_hz == 0)
  {
    __HAL_TIM_SET_COMPARE(&htim12, TIM_CHANNEL_1, 0);
    return;
  }

  // Use ~1 MHz timer tick for easy ARR math
  uint32_t timclk = tim12_get_clk_hz();
  uint32_t psc = (timclk / 1000000U);
  if (psc == 0) psc = 1;
  psc -= 1;

  __HAL_TIM_SET_PRESCALER(&htim12, psc);

  uint32_t arr = (1000000U / (uint32_t)freq_hz);
  if (arr < 2) arr = 2;
  arr -= 1;

  __HAL_TIM_SET_AUTORELOAD(&htim12, arr);
  __HAL_TIM_SET_COMPARE(&htim12, TIM_CHANNEL_1, (arr + 1U) / 2U);
}

static void Audio_Stop(void)
{
  __HAL_TIM_SET_COMPARE(&htim12, TIM_CHANNEL_1, 0);
}

static void Audio_Start(void)
{
  HAL_TIM_PWM_Start(&htim12, TIM_CHANNEL_1);
}

static void Audio_Init(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_TIM12_CLK_ENABLE();

  // PH6 = TIM12_CH1, AF9 on STM32F769
  gpio.Pin = GPIO_PIN_6;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  gpio.Alternate = GPIO_AF9_TIM12;
  HAL_GPIO_Init(GPIOH, &gpio);

  htim12.Instance = TIM12;
  htim12.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim12.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim12.Init.RepetitionCounter = 0;
  htim12.Init.Prescaler = 0;
  htim12.Init.Period = 999;
  htim12.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim12) != HAL_OK) while (1) {}

  TIM_OC_InitTypeDef oc = {0};
  oc.OCMode = TIM_OCMODE_PWM1;
  oc.Pulse = 0;
  oc.OCPolarity = TIM_OCPOLARITY_HIGH;
  oc.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim12, &oc, TIM_CHANNEL_1) != HAL_OK) while (1) {}

  Audio_Start();
  Audio_Stop();
}

static void Audio_Trigger(SfxId id, uint8_t force)
{
  if (!force && g_sfx_active) return;

  switch (id)
  {
    case SFX_FIRE: g_sfx_f = sfx_fire_f; g_sfx_d = sfx_fire_d; g_sfx_len = 1; break;
    case SFX_TX:   g_sfx_f = sfx_tx_f;   g_sfx_d = sfx_tx_d;   g_sfx_len = 1; break;
    case SFX_RX:   g_sfx_f = sfx_rx_f;   g_sfx_d = sfx_rx_d;   g_sfx_len = 1; break;
    case SFX_HIT:  g_sfx_f = sfx_hit_f;  g_sfx_d = sfx_hit_d;  g_sfx_len = 3; break;
    case SFX_WIN:  g_sfx_f = sfx_win_f;  g_sfx_d = sfx_win_d;  g_sfx_len = 3; break;
    case SFX_LOSE: g_sfx_f = sfx_lose_f; g_sfx_d = sfx_lose_d; g_sfx_len = 2; break;
    default: return;
  }

  g_sfx_idx = 0;
  g_sfx_active = 1;

  Audio_SetTone(g_sfx_f[g_sfx_idx]);
  g_sfx_step_end = HAL_GetTick() + (uint32_t)g_sfx_d[g_sfx_idx];
}

static void Audio_Update(void)
{
  if (!g_sfx_active) return;

  uint32_t now = HAL_GetTick();
  if ((int32_t)(now - g_sfx_step_end) < 0) return;

  g_sfx_idx++;
  if (g_sfx_idx >= g_sfx_len)
  {
    g_sfx_active = 0;
    Audio_Stop();
    return;
  }

  Audio_SetTone(g_sfx_f[g_sfx_idx]);
  g_sfx_step_end = now + (uint32_t)g_sfx_d[g_sfx_idx];
}

// -------------------- Game --------------------
#define TICK_MS       20
#define SHIP_W        80
#define SHIP_H        56
#define BULLET_W      4
#define BULLET_H      8
#define MAX_BULLETS   10

#define COL_BG        LCD_COLOR_BLACK
#define COL_SHIP      LCD_COLOR_GREEN
#define COL_BULLET    LCD_COLOR_WHITE
#define COL_IN_BULLET LCD_COLOR_YELLOW
#define COL_HUD       LCD_COLOR_LIGHTGRAY

// 1-byte protocol (WORKING STYLE):
//   0..253  = bullet Y encoded
//   254     = "I DIED"   (loser -> winner)
//   255     = "RESET"    (winner -> loser / sync reset)
#define UART_CTRL_DIED   254
#define UART_CTRL_RESET  255
#define UART_Y_MAX       253

static int W, H, MID_X;

typedef struct { int x, y; } Ship;

typedef struct {
  uint8_t active;
  int x, y;
  int vx;
} Bullet;

static Ship   g_ship;
static Bullet g_out[MAX_BULLETS];
static Bullet g_in[MAX_BULLETS];

static int score_me = 0;
static int score_them = 0;

static void bullets_clear(Bullet *b)
{
  for (int i = 0; i < MAX_BULLETS; i++) b[i].active = 0;
}

static void game_respawn_and_clear(void)
{
  bullets_clear(g_out);
  bullets_clear(g_in);

  g_ship.y = (H / 2) - (SHIP_H / 2);
  g_ship.x = BOARD_IS_LEFT ? 20 : (W - SHIP_W - 20);

  BSP_LCD_Clear(COL_BG);
}

static void flash_screen(uint32_t color, int ms)
{
  BSP_LCD_Clear(color);
  HAL_Delay(ms);
  BSP_LCD_Clear(COL_BG);
}

static void bullet_spawn(Bullet *pool, int x, int y, int vx)
{
  for (int i = 0; i < MAX_BULLETS; i++)
  {
    if (!pool[i].active)
    {
      pool[i].active = 1;
      pool[i].x = x;
      pool[i].y = y;
      pool[i].vx = vx;
      return;
    }
  }
}

static void draw_bullet(int x, int y, uint32_t c)
{
  if (x < 0 || x >= W || y < 0 || y >= H) return;

  int w = BULLET_W;
  int h = BULLET_H;
  if (x + w > W) w = W - x;
  if (y + h > H) h = H - y;
  if (w <= 0 || h <= 0) return;

  BSP_LCD_SetTextColor(c);
  BSP_LCD_FillRect(x, y, w, h);
}

static void draw_ship_right(int x, int y, uint32_t c)
{
  BSP_LCD_SetTextColor(c);
  BSP_LCD_FillRect(x + 18, y + 22, 38, 12);
  BSP_LCD_FillRect(x + 56, y + 18, 18, 20);
  BSP_LCD_FillRect(x + 6,  y + 24, 10, 8);
  BSP_LCD_FillRect(x + 24, y + 10, 18, 10);
  BSP_LCD_FillRect(x + 24, y + 36, 18, 10);
}

static void draw_ship_left(int x, int y, uint32_t c)
{
  BSP_LCD_SetTextColor(c);
  BSP_LCD_FillRect(x + 24, y + 22, 38, 12);
  BSP_LCD_FillRect(x + 6,  y + 18, 18, 20);
  BSP_LCD_FillRect(x + 64, y + 24, 10, 8);
  BSP_LCD_FillRect(x + 38, y + 10, 18, 10);
  BSP_LCD_FillRect(x + 38, y + 36, 18, 10);
}

static void draw_ship(uint32_t c)
{
  if (BOARD_IS_LEFT) draw_ship_right(g_ship.x, g_ship.y, c);
  else              draw_ship_left (g_ship.x, g_ship.y, c);
}

static inline int rect_overlap(int ax, int ay, int aw, int ah,
                               int bx, int by, int bw, int bh)
{
  return (ax < bx + bw) && (ax + aw > bx) && (ay < by + bh) && (ay + ah > by);
}

static inline uint8_t y_to_u8_safe(int y)
{
  int maxY = H - BULLET_H - 1;
  if (maxY <= 0) return 0;
  if (y < 0) y = 0;
  if (y > maxY) y = maxY;

  int v = (y * UART_Y_MAX) / maxY; // 0..253
  if (v < 0) v = 0;
  if (v > UART_Y_MAX) v = UART_Y_MAX;
  return (uint8_t)v;
}

static inline int u8_to_y_safe(uint8_t b)
{
  int maxY = H - BULLET_H - 1;
  if (maxY <= 0) return 0;
  if (b > UART_Y_MAX) b = UART_Y_MAX;
  return (int)((b * maxY) / UART_Y_MAX);
}

// -------------------- UART RX: 1-byte protocol --------------------
static void uart_poll_rx(void)
{
  uint8_t b;
  while (UART6_ReadByte(&b))
  {
    BSP_LED_Toggle(LED1); // RX proof

    if (b == UART_CTRL_DIED)
    {
      // opponent died => you score, flash green, then command reset
      score_me++;
      Audio_Trigger(SFX_WIN, 1);
      flash_screen(LCD_COLOR_GREEN, 250);

      UART6_SendByte(UART_CTRL_RESET);
      Audio_Trigger(SFX_TX, 0);
      BSP_LED_Toggle(LED2); // TX proof

      game_respawn_and_clear();
      continue;
    }

    if (b == UART_CTRL_RESET)
    {
      game_respawn_and_clear();
      continue;
    }

    // Bullet byte 0..253
    int y = u8_to_y_safe(b);

    // Spawn at your SCREEN EDGE (not at the player)
    int spawnX, vx;
    if (BOARD_IS_LEFT)
    {
      spawnX = W - BULLET_W - 1; // comes from right edge
      vx = -10;
    }
    else
    {
      spawnX = 0; // comes from left edge
      vx = +10;
    }

    bullet_spawn(g_in, spawnX, y, vx);
    Audio_Trigger(SFX_RX, 0);
  }
}

// -------------------- MAIN --------------------
int main(void)
{
  HAL_Init();
  SystemClock_Config();

  BSP_LED_Init(LED1);
  BSP_LED_Init(LED2);

  Buttons_Init();
  UART6_Init();
  Audio_Init();

  BSP_LED_On(LED2);

  if (BSP_LCD_Init() != LCD_OK)
  {
    while (1) { BSP_LED_Toggle(LED2); HAL_Delay(150); }
  }

  BSP_LCD_LayerDefaultInit(LTDC_ACTIVE_LAYER_FOREGROUND, LCD_FB_START_ADDRESS);
  BSP_LCD_SelectLayer(LTDC_ACTIVE_LAYER_FOREGROUND);
  BSP_LCD_DisplayOn();
  BSP_LCD_SetBrightness(100);

  W = (int)BSP_LCD_GetXSize();
  H = (int)BSP_LCD_GetYSize();
  MID_X = W / 2;

  BSP_LCD_Clear(COL_BG);
  BSP_LCD_SetBackColor(COL_BG);

  game_respawn_and_clear();

  uint8_t fireLatch = 0;

  while (1)
  {
    Audio_Update();
    uart_poll_rx();

    // ---- erase previous frame ----
    draw_ship(COL_BG);
    for (int i = 0; i < MAX_BULLETS; i++)
    {
      if (g_out[i].active) draw_bullet(g_out[i].x, g_out[i].y, COL_BG);
      if (g_in[i].active)  draw_bullet(g_in[i].x,  g_in[i].y,  COL_BG);
    }

    // ---- movement (keep in your half) ----
    if (pressed(BTN_UP_PORT, BTN_UP_PIN))        g_ship.y -= 4;
    if (pressed(BTN_DOWN_PORT, BTN_DOWN_PIN))    g_ship.y += 4;
    if (pressed(BTN_LEFT_PORT, BTN_LEFT_PIN))    g_ship.x -= 4;
    if (pressed(BTN_RIGHT_PORT, BTN_RIGHT_PIN))  g_ship.x += 4;

    // clamp Y
    if (g_ship.y < 24) g_ship.y = 24;
    if (g_ship.y > (H - SHIP_H - 1)) g_ship.y = (H - SHIP_H - 1);

    // clamp X to your half
    if (BOARD_IS_LEFT)
    {
      if (g_ship.x < 0) g_ship.x = 0;
      int maxX = MID_X - SHIP_W - 2;
      if (g_ship.x > maxX) g_ship.x = maxX;
    }
    else
    {
      int minX = MID_X + 2;
      if (g_ship.x < minX) g_ship.x = minX;
      int maxX = W - SHIP_W - 1;
      if (g_ship.x > maxX) g_ship.x = maxX;
    }

    // ---- fire (edge detect) ----
    uint8_t fireNow = pressed(BTN_FIRE_PORT, BTN_FIRE_PIN);
    if (fireNow && !fireLatch)
    {
      fireLatch = 1;

      int bx = BOARD_IS_LEFT ? (g_ship.x + SHIP_W - 8) : (g_ship.x + 4);
      int by = g_ship.y + (SHIP_H / 2);
      int vx = BOARD_IS_LEFT ? 10 : -10;

      bullet_spawn(g_out, bx, by, vx);
      Audio_Trigger(SFX_FIRE, 0);
    }
    if (!fireNow) fireLatch = 0;

    // ---- update OUT bullets: send ONLY when fully leaving your screen ----
    for (int i = 0; i < MAX_BULLETS; i++)
    {
      if (!g_out[i].active) continue;

      g_out[i].x += g_out[i].vx;

      // When it exits the screen edge, transmit the Y and delete local bullet.
      if (g_out[i].x >= W || (g_out[i].x + BULLET_W) <= 0)
      {
        uint8_t yb = y_to_u8_safe(g_out[i].y);
        UART6_SendByte(yb);
        Audio_Trigger(SFX_TX, 0);
        BSP_LED_Toggle(LED2); // TX proof
        g_out[i].active = 0;
      }
    }

    // ---- update IN bullets ----
    for (int i = 0; i < MAX_BULLETS; i++)
    {
      if (!g_in[i].active) continue;
      g_in[i].x += g_in[i].vx;
      if (g_in[i].x < -20 || g_in[i].x > (W + 20)) g_in[i].active = 0;
    }

    // ---- collision: incoming bullets hit ship ----
    int shipDead = 0;
    for (int i = 0; i < MAX_BULLETS; i++)
    {
      if (!g_in[i].active) continue;

      if (rect_overlap(g_ship.x, g_ship.y, SHIP_W, SHIP_H,
                       g_in[i].x, g_in[i].y, BULLET_W, BULLET_H))
      {
        g_in[i].active = 0;
        shipDead = 1;
      }
    }

    if (shipDead)
    {
      // you died => they score; tell them; then reset yourself
      score_them++;
      Audio_Trigger(SFX_HIT, 1);
      flash_screen(LCD_COLOR_RED, 250);

      UART6_SendByte(UART_CTRL_DIED);
      Audio_Trigger(SFX_TX, 0);
      BSP_LED_Toggle(LED2); // TX proof

      Audio_Trigger(SFX_LOSE, 1);
      game_respawn_and_clear();
    }

    // ---- draw ----
    draw_ship(COL_SHIP);
    for (int i = 0; i < MAX_BULLETS; i++)
    {
      if (g_out[i].active) draw_bullet(g_out[i].x, g_out[i].y, COL_BULLET);
      if (g_in[i].active)  draw_bullet(g_in[i].x,  g_in[i].y,  COL_IN_BULLET);
    }

    // HUD (score)
    BSP_LCD_SetTextColor(COL_BG);
    BSP_LCD_FillRect(0, 0, W, 24);
    BSP_LCD_SetTextColor(COL_HUD);
    char s[64];
    snprintf(s, sizeof(s), "%s  ME:%d  THEM:%d",
             BOARD_IS_LEFT ? "LEFT" : "RIGHT",
             score_me, score_them);
    BSP_LCD_DisplayStringAt(0, 0, (uint8_t*)s, LEFT_MODE);

    HAL_Delay(TICK_MS);
  }
}
