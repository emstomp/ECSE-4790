/***************************************************************
 * Lab 3 - Task 4: STaTS SPI controller (per STaTS register protocol)
 * Target : STM32F769I-DISCO (F7 HAL)
 * Depends: init.h (Sys_Init) + uart.c/uart.h (USB_UART, printf)
 *
 * SPI2 pins: PB13=SCK, PB14=MISO, PB15=MOSI (AF5)
 * CS pin   : PA11 (D10) active-low
 *
 * STaTS protocol:
 *  - 2 bytes per transaction
 *  - LSB-first, Mode 0, <= 100 kbit/s
 *  - TX[0] header (LSB-first): bit0=WE (1=write), bits1..4 = REG#, bits5..7 don't care
 *  - TX[1] = write data (or dummy for read)
 *  - RX[0] = STS_REG, RX[1] = REG value (read) or previous value (write)
 ***************************************************************/
#include "init.h"
#include "stm32f7xx_hal.h"
#include "uart.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* extern from your uart.c */
extern UART_HandleTypeDef USB_UART;

/* ===== SPI2 + CS ===== */
static SPI_HandleTypeDef hspi2;
#define CS_GPIO_Port   GPIOA              /* D10 -> A2 on STaTS */
#define CS_Pin         GPIO_PIN_11
#define CS_LOW()       HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET)
#define CS_HIGH()      HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET)

/* tiny CS setup/hold delays (μs-ish; conservative) */
static inline void delay_cycles(volatile uint32_t n){ while(n--) __NOP(); }
#define CS_SETUP_DELAY()  delay_cycles(150)
#define CS_HOLD_DELAY()   delay_cycles(150)

/* ===== STaTS register numbers ===== */
enum {
  REG_CTL     = 0,
  REG_STS     = 1,
  REG_DIG     = 2,
  REG_TMP_AVG = 3,
  REG_TMP_LO  = 4,
  REG_TMP_HI  = 5,
  REG_CH_BUF  = 6,
  REG_TXT_ATTR= 7,
  REG_VERSION = 8,
  REG_DEVID   = 9
};

/* CTL_REG bits (write one-shot control) */
#define CTL_RST      (1u<<0)   /* soft reset */
#define CTL_RDTMP    (1u<<1)   /* trigger temperature conversion */
#define CTL_TRMCLR   (1u<<2)   /* clear terminal */
#define CTL_TRMRST   (1u<<3)   /* reset terminal + attributes */
#define CTL_CHBCLR   (1u<<4)   /* clear char buffer */
#define CTL_LGT_TGL  (1u<<5)   /* toggle LD3 */
#define CTL_ULKDID   (1u<<7)   /* unlock next DEVID write */

/* STS_REG bits (read status) */
#define STS_RDY      (1u<<0)
#define STS_DIG      (1u<<1)
#define STS_LGT_ON   (1u<<2)
#define STS_TBUSY    (1u<<3)
#define STS_TRDY     (1u<<4)   /* temperature ready */
#define STS_NCHBF0   (1u<<5)
#define STS_NCHBF1   (1u<<6)
#define STS_CHBOV    (1u<<7)

/* Choose which DPx line to mirror to LD3 (bitmap read from DIG_REG) */
#ifndef STATS_DPX_MASK
#define STATS_DPX_MASK (1u<<2)   /* mirror DP2 by default; change if needed */
#endif

/* Temperature conversion per datasheet:
   Temp [°C] = 357.6 − 0.187 * (TMP_HI:TMP_LO, right-justified 12-bit) */
static inline float stats_raw_to_degC(uint16_t v){ return 357.6f - 0.187f * (float)v; }

/* ===== UART non-blocking getchar ===== */
static int uart_getchar_nb(uint8_t* ch)
{
  return (HAL_UART_Receive(&USB_UART, ch, 1, 0) == HAL_OK) ? 1 : 0;
}

/* ===== GPIO + SPI init ===== */
static void GPIO_SPI2_Msp(void)
{
  __HAL_RCC_SPI2_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  GPIO_InitTypeDef g = {0};
  g.Mode      = GPIO_MODE_AF_PP;
  g.Pull      = GPIO_NOPULL;
  g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
  g.Alternate = GPIO_AF5_SPI2;
  g.Pin       = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;  /* PB13=SCK, PB14=MISO, PB15=MOSI */
  HAL_GPIO_Init(GPIOB, &g);

  g.Pin   = CS_Pin;  g.Mode  = GPIO_MODE_OUTPUT_PP;  g.Pull  = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(CS_GPIO_Port, &g);
  CS_HIGH();
}

static void MX_SPI2_Init(void)
{
  hspi2.Instance               = SPI2;
  hspi2.Init.Mode              = SPI_MODE_MASTER;
  hspi2.Init.Direction         = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize          = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity       = SPI_POLARITY_LOW;     /* Mode 0 */
  hspi2.Init.CLKPhase          = SPI_PHASE_1EDGE;      /* Mode 0 */
  hspi2.Init.NSS               = SPI_NSS_SOFT;         /* manual CS */
  hspi2.Init.FirstBit          = SPI_FIRSTBIT_LSB;     /* **LSB-first** required by STaTS */
  hspi2.Init.TIMode            = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.NSSPMode          = SPI_NSS_PULSE_DISABLE;

  /* Keep <= 100 kbit/s. With your clock tree this gives ~53 kbit/s. */
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;

  if (HAL_SPI_Init(&hspi2) != HAL_OK) { __disable_irq(); while(1){} }
}

/* ===== One STaTS transfer (2 bytes): header + data, returns STS + value =====
   Header (LSB-first): b0=WE, b1..b4=REG#, b5..b7=don't care
*/
static HAL_StatusTypeDef stats_xfer(uint8_t we, uint8_t reg, uint8_t din,
                                    uint8_t* out_sts, uint8_t* out_data)
{
  uint8_t tx[2], rx[2];
  tx[0] = (uint8_t)((we ? 1u : 0u) | ((reg & 0x0Fu) << 1));
  tx[1] = din;

  CS_LOW(); CS_SETUP_DELAY();
  HAL_StatusTypeDef s = HAL_SPI_TransmitReceive(&hspi2, tx, rx, 2, 30);
  CS_HOLD_DELAY(); CS_HIGH();

  if (out_sts)  *out_sts  = rx[0];
  if (out_data) *out_data = rx[1];
  return s;
}

static uint8_t stats_read_reg(uint8_t reg, uint8_t* val, uint8_t* sts)
{
  uint8_t s=0, d=0;
  if (stats_xfer(0, reg, 0x00, &s, &d) != HAL_OK) return 0;
  if (val) *val = d;
  if (sts) *sts = s;
  return 1;
}
static uint8_t stats_write_reg(uint8_t reg, uint8_t val, uint8_t* prev, uint8_t* sts)
{
  uint8_t s=0, d=0;
  if (stats_xfer(1, reg, val, &s, &d) != HAL_OK) return 0;
  if (prev) *prev = d;
  if (sts)  *sts  = s;
  return 1;
}

/* ===== High-level ops per Task 4 ===== */

/* (1) send terminal char: write CH_BUF */
static void stats_tx_char(uint8_t ch) { (void)stats_write_reg(REG_CH_BUF, ch, NULL, NULL); }

/* (2) try receive a char: read CH_BUF (returns 0x00 if empty) */
static int stats_try_rx_char(uint8_t* out)
{
  uint8_t v=0;
  if (!stats_read_reg(REG_CH_BUF, &v, NULL)) return 0;
  if (v == 0x00) return 0;
  *out = v; return 1;
}

/* (3) mirror DPx -> LD3 using status (toggle only if needed) */
static void stats_mirror_ld3(void)
{
  uint8_t dpx=0, sts=0;
  if (!stats_read_reg(REG_DIG, &dpx, &sts)) return;
  uint8_t want_on = (dpx & STATS_DPX_MASK) ? 1u : 0u;
  uint8_t is_on   = (sts & STS_LGT_ON)     ? 1u : 0u;
  if (want_on != is_on) (void)stats_write_reg(REG_CTL, CTL_LGT_TGL, NULL, NULL);
}

/* (4) read version (REG_VERSION: major in high nibble, minor in low) */
static uint8_t stats_read_version(uint8_t* major, uint8_t* minor)
{
  uint8_t v=0;
  if (!stats_read_reg(REG_VERSION, &v, NULL)) return 0;
  if (major) *major = (uint8_t)((v>>4)&0x0F);
  if (minor) *minor = (uint8_t)(v & 0x0F);
  return 1;
}

/* (5) temperature: trigger, then when STS.TRDY=1 read TMP_LO/TMP_HI */
static void stats_trigger_temp(void) { (void)stats_write_reg(REG_CTL, CTL_RDTMP, NULL, NULL); }
static int stats_try_read_temp(float* outC)
{
  uint8_t sts=0;
  if (!stats_read_reg(REG_STS, &sts, &sts)) return 0;
  if (!(sts & STS_TRDY)) return 0;
  uint8_t lo=0, hi=0;
  if (!stats_read_reg(REG_TMP_LO, &lo, NULL)) return 0;
  if (!stats_read_reg(REG_TMP_HI, &hi, NULL)) return 0; /* reading HI clears TRDY */
  uint16_t raw = (uint16_t)(((uint16_t)hi<<8) | lo);
  if (outC) *outC = stats_raw_to_degC(raw);
  return 1;
}

/* (6) clear/reset terminal */
static void stats_clear_terminal(uint8_t reset_attrs)
{
  (void)stats_write_reg(REG_CTL, reset_attrs ? CTL_TRMRST : CTL_TRMCLR, NULL, NULL);
}

/* (7) set/read device ID (unlock then write) */
static void stats_set_devid(uint8_t id)
{
  (void)stats_write_reg(REG_CTL, CTL_ULKDID, NULL, NULL); /* unlock for next write */
  (void)stats_write_reg(REG_DEVID, id, NULL, NULL);
}
static uint8_t stats_get_devid(void)
{
  uint8_t v=0; (void)stats_read_reg(REG_DEVID, &v, NULL); return v;
}

/* ===== Menu ===== */
static void show_menu(void)
{
  printf("\r\n--- STaTS Menu ---\r\n");
  printf(" v: read version\r\n");
  printf(" t: trigger temperature\r\n");
  printf(" r: read temperature\r\n");
  printf(" x: clear terminal (keep attrs)\r\n");
  printf(" X: reset terminal (clear attrs)\r\n");
  printf(" i: set ID=0x42 then read back\r\n");
  printf(" q: quit menu\r\n> ");
}
static void handle_menu(uint8_t k)
{
  if (k=='v'){
    uint8_t maj=0, min=0;
    if (stats_read_version(&maj,&min)) printf("FW version %u.%u\r\n> ", maj, min);
    else                                printf("FW read failed\r\n> ");
  } else if (k=='t'){
    stats_trigger_temp(); printf("Temp conversion started\r\n> ");
  } else if (k=='r'){
    float c;
    if (stats_try_read_temp(&c)) printf("Temp = %.2f C\r\n> ", (double)c);
    else                         printf("Temp not ready\r\n> ");
  } else if (k=='x'){
    stats_clear_terminal(0); printf("Cleared\r\n> ");
  } else if (k=='X'){
    stats_clear_terminal(1); printf("Reset\r\n> ");
  } else if (k=='i'){
    stats_set_devid(0x42);
    uint8_t id = stats_get_devid();
    printf("Device ID now 0x%02X\r\n> ", id);
  } else {
    printf("(unknown)\r\n> ");
  }
}

/* ===== MAIN ===== */
int main(void)
{
  Sys_Init();          /* clocks + USB_UART for printf */
  GPIO_SPI2_Msp();     /* PB13/14/15 AF5 + PA11 output */
  MX_SPI2_Init();      /* LSB-first, Mode0, ~53 kbit/s */

  printf("\r\n=== Task 4: STaTS controller (LSB-first, Mode0) ===\r\n");
  printf("Wiring: D13->A3(SCK) D11->A4(MOSI) D12->A5(MISO) D10->A2(CS) 3V3/GND\r\n");
  printf("Type to send; ESC shows menu. DPx->LD3 mirroring is automatic.\r\n");

  /* (4) read FW on startup */
  uint8_t maj=0, min=0;
  if (stats_read_version(&maj,&min)) printf("FW version: %u.%u\r\n", maj, min);
  else                                printf("FW read failed\r\n");

  uint32_t last_poll_ms = 0, last_rx_ms = 0;
  int in_menu = 0;

  for (;;)
  {
    /* keyboard -> STaTS (1) */
    uint8_t c;
    if (uart_getchar_nb(&c))
    {
      if (c == 27){ in_menu=1; show_menu(); continue; }

      if (in_menu){
        if (c=='q' || c=='Q'){ in_menu=0; printf("\r\n(resume)\r\n"); }
        else handle_menu(c);
        continue;
      }

      if (c=='\r' || c=='\n'){ stats_tx_char('\n'); printf("[TX] \\n\r\n"); }
      else if (c>=32 && c<=126){ stats_tx_char(c); printf("[TX] 0x%02X '%c'\r\n", c, c); }
      /* ignore other control chars */
    }

    uint32_t now = HAL_GetTick();

    /* poll RX (2) every few ms */
    if (!in_menu && (now - last_rx_ms > 3)){
      uint8_t r;
      if (stats_try_rx_char(&r)){
        if (r=='\r' || r=='\n') printf("[RX] \\n\r\n");
        else if (r>=32 && r<=126) printf("[RX] 0x%02X '%c'\r\n", r, r);
        else printf("[RX] 0x%02X\r\n", r);
      }
      last_rx_ms = now;
    }

    /* DPx -> LD3 mirror (3) ~50 Hz */
    if (now - last_poll_ms > 20){
      stats_mirror_ld3();
      last_poll_ms = now;
    }
  }
}
