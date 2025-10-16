/***************************************************************
 * Lab 3 - Part 3: SPI loopback with live single-line panes
 * Shows each full sentence as a JSON-style array of strings (words)
 * Target: STM32F769I-DISCO (F7 HAL)
 ***************************************************************/
#include "init.h"
#include "stm32f7xx_hal.h"
#include "uart.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

/* ---- extern from your uart.c ---- */
extern UART_HandleTypeDef USB_UART;

/* ---- SPI2 handle ---- */
SPI_HandleTypeDef hspi2;

/* ==== Prototypes ==== */
static void SPI2_MspInit_Pins(void);
static void MX_SPI2_Init(void);
static void UI_Init(void);
static void UI_DrawHeader(void);
static void UI_UpdateTopLive(const char* s);
static void UI_UpdateBotLive(const char* s);
static void UI_PrintTopArray(const char* s);
static void UI_PrintBotArray(const char* s);
static int  uart_getchar_nb(uint8_t* ch);
static void tokenize_to_array(const char* line, char* out, size_t outsz);
void Error_Handler(void);

/* ====== Simple split terminal layout (ANSI) ====== */
#define TERM_COLS  100
#define TOP_ROW    4           /* live typing row */
#define SEP_ROW    6
#define BOT_ROW    8           /* live RX row */

static inline void ansi_move(int r, int c){ printf("\033[%d;%dH", r, c); }
static inline void ansi_clear(void){ printf("\033[2J\033[H"); }
static inline void ansi_clear_line(void){ printf("\033[2K"); }

/* ====== Line buffers ====== */
#define LINE_MAX 256
static char tx_live[LINE_MAX];  static size_t tx_len = 0;
static char rx_live[LINE_MAX];  static size_t rx_len = 0;

/* ============================================================
 * MSP: map SPI2 to PB13/PB14/PB15 (AF5)
 * ============================================================ */
static void SPI2_MspInit_Pins(void)
{
  __HAL_RCC_SPI2_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  GPIO_InitTypeDef g = {0};
  g.Mode      = GPIO_MODE_AF_PP;
  g.Pull      = GPIO_NOPULL;
  g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
  g.Alternate = GPIO_AF5_SPI2;

  /* PB13 = SCK, PB14 = MISO, PB15 = MOSI */
  g.Pin = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
  HAL_GPIO_Init(GPIOB, &g);
}

/* ============================================================
 * SPI2 init: 8-bit, Mode 0, ~1 MHz, NSS=SOFT
 * ============================================================ */
static void MX_SPI2_Init(void)
{
  hspi2.Instance               = SPI2;
  hspi2.Init.Mode              = SPI_MODE_MASTER;
  hspi2.Init.Direction         = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize          = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity       = SPI_POLARITY_LOW;      // Mode 0
  hspi2.Init.CLKPhase          = SPI_PHASE_1EDGE;       // Mode 0
  hspi2.Init.NSS               = SPI_NSS_SOFT;          // no CS for loopback
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64; // ~1 MHz if kernel ~64 MHz
  hspi2.Init.FirstBit          = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode            = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.NSSPMode          = SPI_NSS_PULSE_DISABLE;

  if (HAL_SPI_Init(&hspi2) != HAL_OK) Error_Handler();
}

/* ============================================================
 * UI helpers
 * ============================================================ */
static void UI_DrawHeader(void)
{
  ansi_clear();
  ansi_move(1,1);
  printf("=== Lab 3 Part 3: SPI2 Loopback (~1 MHz) — Live Panes & Word Arrays ===\r\n");
  printf("Pins: D13=PB13(SCK), D12=PB14(MISO), D11=PB15(MOSI)  |  Short D11 <-> D12\r\n");
  printf("Top live (TX) — type your sentence, hit ENTER to freeze as an array\r\n");
  ansi_move(SEP_ROW,1);
  for (int i=0;i<TERM_COLS;i++) putchar('-');
  ansi_move(SEP_ROW+1,1);
  printf("Bottom live (RX) — echoed back from SPI; ENTER prints received array\r\n");
  fflush(stdout);
}

static void UI_Init(void)
{
  tx_len = rx_len = 0;
  tx_live[0] = rx_live[0] = 0;
  UI_DrawHeader();
  UI_UpdateTopLive("");
  UI_UpdateBotLive("");
}

static void UI_UpdateTopLive(const char* s)
{
  ansi_move(TOP_ROW,1); ansi_clear_line();
  printf("TX live: %s", s);
  fflush(stdout);
}
static void UI_UpdateBotLive(const char* s)
{
  ansi_move(BOT_ROW,1); ansi_clear_line();
  printf("RX live: %s", s);
  fflush(stdout);
}

static void UI_PrintTopArray(const char* s)
{
  char arr[LINE_MAX*2]; tokenize_to_array(s, arr, sizeof(arr));
  ansi_move(TOP_ROW-1,1); ansi_clear_line();
  printf("TX array: %s", arr);
  fflush(stdout);
}
static void UI_PrintBotArray(const char* s)
{
  char arr[LINE_MAX*2]; tokenize_to_array(s, arr, sizeof(arr));
  ansi_move(BOT_ROW+1,1); ansi_clear_line();
  printf("RX array: %s", arr);
  fflush(stdout);
}

/* Convert "hello world 123" -> ["hello","world","123"] */
static void tokenize_to_array(const char* line, char* out, size_t outsz)
{
  out[0] = 0;
  strncat(out, "[", outsz-1);
  const char* p=line;
  while (*p && isspace((unsigned char)*p)) p++;

  int first=1;
  while (*p)
  {
    /* extract token */
    char tok[LINE_MAX]; size_t tlen=0;
    while (*p && !isspace((unsigned char)*p) && tlen < LINE_MAX-1)
      tok[tlen++] = *p++;
    tok[tlen] = 0;

    if (tlen>0) {
      if (!first) strncat(out, ",", outsz-1);
      strncat(out, "\"", outsz-1);
      /* escape quotes/backslashes if needed */
      for (size_t i=0;i<tlen;i++){
        if (tok[i]=='\"' || tok[i]=='\\') strncat(out, "\\", outsz-1);
        char tmp[2]={tok[i],0}; strncat(out, tmp, outsz-1);
      }
      strncat(out, "\"", outsz-1);
      first=0;
    }
    while (*p && isspace((unsigned char)*p)) p++;
  }
  strncat(out, "]", outsz-1);
}

/* ============================================================
 * Non-blocking getchar (uses USB_UART)
 * ============================================================ */
static int uart_getchar_nb(uint8_t* ch)
{
  return (HAL_UART_Receive(&USB_UART, ch, 1, 0) == HAL_OK) ? 1 : 0;
}

/* ============================================================
 * MAIN
 * ============================================================ */
int main(void)
{
  Sys_Init();              // clocks + USB_UART for printf
  SPI2_MspInit_Pins();     // PB13/PB14/PB15 -> AF5
  MX_SPI2_Init();          // SPI ready
  UI_Init();

  for (;;)
  {
    /* 1) Read from keyboard (non-blocking) */
    uint8_t c;
    if (uart_getchar_nb(&c))
    {
      /* Handle backspace */
      if ((c==0x08 || c==0x7F) && tx_len>0) {
        tx_live[--tx_len]=0;
        UI_UpdateTopLive(tx_live);
        /* still send the raw byte over SPI to keep SPI busy, optional: skip */
        continue;
      }

      /* End-of-line? Treat CR or LF as "send line" */
      if (c=='\r' || c=='\n') {
        /* freeze & print TX word array */
        UI_PrintTopArray(tx_live);

        /* also terminate TX line for SPI peer */
        uint8_t eol = '\n';
        uint8_t dummy;
        HAL_SPI_TransmitReceive(&hspi2, &eol, &dummy, 1, 5);

        /* if we already have RX line pending, print its array too */
        if (rx_len>0) { UI_PrintBotArray(rx_live); rx_len=0; rx_live[0]=0; UI_UpdateBotLive(rx_live); }

        /* reset TX line for next sentence */
        tx_len=0; tx_live[0]=0; UI_UpdateTopLive(tx_live);
        continue;
      }

      /* Append printable char to TX live line */
      if (tx_len < LINE_MAX-1 && (c>=32 && c<=126)) {
        tx_live[tx_len++] = (char)c; tx_live[tx_len]=0;
        UI_UpdateTopLive(tx_live);
      }

      /* 2) Send on SPI and receive simultaneously (loopback returns same byte) */
      uint8_t rx=0;
      (void)HAL_SPI_TransmitReceive(&hspi2, &c, &rx, 1, 5);

      /* Append received printable to RX live line; on EOL, print array */
      if (rx=='\r') { /* ignore */ }
      else if (rx=='\n') {
        UI_PrintBotArray(rx_live);
        rx_len=0; rx_live[0]=0;
        UI_UpdateBotLive(rx_live);
      } else if (rx>=32 && rx<=126) {
        if (rx_len < LINE_MAX-1) {
          rx_live[rx_len++] = (char)rx; rx_live[rx_len]=0;
          UI_UpdateBotLive(rx_live);
        }
      }
    }

    /* small idle so USB CDC doesn't starve lower-priority IRQs */
    // HAL_Delay(0);
  }
}

/* ============================================================
 * Error handler
 * ============================================================ */
void Error_Handler(void)
{
  __disable_irq();
  while (1) { /* add LED blink if desired */ }
}
