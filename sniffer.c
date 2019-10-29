/*
 * XMC-Sniffer -- a basic logic-analyzer application for a
 * an XMC4500 Relax Lite board, to be called via gdb.
 *
 * usage example (record PORT1 pins 8-15 for 10,000 ms):
 * (gdb) b ready
 * (gdb) r
 * (gdb) print /x *trace@record(10000)
 *
 */

#include <XMC4500.h>
#include <system_XMC4500.h>
#include <xmc_gpio.h>

__USED
static char *version =
  " *** XMC-Sniffer -- a basic logic analyzer by Markus Kuhn *** ";

#define LED1 P1_1
#define LED2 P1_0

/* initialize the GPIO pins used on PORT1 */
void init_port() {
  XMC_GPIO_CONFIG_t config;
  uint8_t pin;

  /* enable outputs for LED1 and LED2 */
  config.mode            = XMC_GPIO_MODE_OUTPUT_PUSH_PULL;
  config.output_strength = XMC_GPIO_OUTPUT_STRENGTH_MEDIUM;
  config.output_level    = XMC_GPIO_OUTPUT_LEVEL_LOW;
  XMC_GPIO_Init(LED1, &config);
  XMC_GPIO_Init(LED2, &config);

  /* enable inputs with weak pull-up resistors for pins 8-15 on PORT1 */
  config.mode            = XMC_GPIO_MODE_INPUT_PULL_UP;
  config.output_strength = XMC_GPIO_OUTPUT_STRENGTH_WEAK;
  config.output_level    = XMC_GPIO_OUTPUT_LEVEL_LOW;
  for (pin = 8; pin < 16; pin++)
    XMC_GPIO_Init(XMC_GPIO_PORT1, pin, &config);
}

/* set up 24-bit count-down timer without enabling interrupts */
void systick_config() {
  PPB->SYST_RVR = 0xffffff;
  PPB->SYST_CVR = 0;
  PPB->SYST_CSR = 1 | 4; /* ENABLE + f_CPU */
}

/* read 24-bit count-down timer */
__STATIC_INLINE uint32_t systick() {
  return ((volatile PPB_Type *) PPB)->SYST_CVR;
}

uint32_t core_clock = 0;

/* configure required hardware resources */
void init() {
  core_clock = SystemCoreClock;
  systick_config();
  init_port();
}

/*
 * Recording format:
 *
 * Each 32-bit integer word in trace[] represents a time period during
 * which the inputs P1_8..P1_15 inputs haven't changed. There are two
 * formats:
 *
 * Short record (representing up to 2^23-1 clock ticks without change):
 *
 *  0TTTTTTT TTTTTTTT TTTTTTTT PPPPPPPP
 *
 * Long format (representing r * 2^23 ticks without change)
 *
 *  1RRRRRRR RRRRRRRR RRRRRRRR PPPPPPPP
 *
 * where
 *
 *  PPPPPPPP     are the input levels on P1_15 P1_14 ... P1_8
 *  TTTT...T     are a count of single clock ticks
 *  RRRR...R     are a count of 2^23 clock ticks
 *
 * At a clock rate of 120 MHz, the 23-bit clock-tick count in the
 * short record will overflow after ~70 ms (at which point a long
 * record will be added), and the 70 ms count in the long record will
 * overflow after ~6.787 days (at which point the next long record
 * will be added).
 *
 * Memory requirements:
 *  - one 32-bit word for each change of input bits
 *  - another 32-bit word when more than 70 ms have passed since
 *    the previous input change
 *  - another 32-bit word for every additional 6.787 day period
 *    during which no input bit has changed
 */

/* read input pins 8-15 on PORT1 */
__STATIC_INLINE uint32_t GetInput()
{
  return (((((volatile XMC_GPIO_PORT_t *) XMC_GPIO_PORT1)->IN) >> 8) & 0xff);
}

#define NMAX 10000
uint32_t trace[NMAX]; /* recording buffer */
int n;                /* number of records in buffer */

/*
 * If tmax is zero, record until the trace[] buffer is full.
 * If tmax is positive, record at least tmax milliseconds or
 * until trace[] is full.
 *
 * Return (and leave in global variable n) the number of words recorded.
 */
__USED
int record(int tmax)
{
  uint32_t last_value = GetInput();
  uint32_t last_ticks = systick() << 8;
  uint32_t new_value, new_ticks, passed_ticks;
  int64_t ticks_left = tmax * (int64_t) (core_clock / 1000);

  XMC_GPIO_SetOutputLow(LED1);  /* LED1 = edge activity indicator */
  XMC_GPIO_SetOutputHigh(LED2); /* LED2 = recording */
  n = 0;
  while (ticks_left >= 0) {
    /* wait until port bits have changed */
    while ((new_value = GetInput()) == last_value && ticks_left >= 0) {
      /* no change */
      new_ticks = systick() << 8;
      passed_ticks = last_ticks - new_ticks; /* implicit mod 2^24 */
      /* update trace record after we are half way round the 24-bit counter */
      if (passed_ticks > 0x7fffffff) {
        /* at least 2^23 ticks have passed, so it is time to record these */
        if (n == 0 ||
            (trace[n-1] & 0x800000ff) != (0x80000000 | last_value) ||
            (trace[n-1] & 0xffffff00) == 0xffffff00) {
          if (n >= NMAX) break;
          /* start a new long record */
          trace[n++] = 0x80000100 | last_value;
        } else {
          /* increment an existing long record */
          trace[n-1] += 0x100;
        }
        last_ticks -= 0x80000000;
        if (ticks_left)
          (ticks_left -= 0x800000) || ticks_left--;
        XMC_GPIO_SetOutputLow(LED1); /* timeout activity LED1 */
      }
    }
    /* we have an edge detected (or timed out) */
    new_ticks = systick() << 8;
    passed_ticks = last_ticks - new_ticks; /* implicit mod 2^24 */
    if (n >= NMAX) break;
    trace[n++] = passed_ticks | last_value;
    last_ticks = new_ticks;
    last_value = new_value;
    if (ticks_left)
      (ticks_left -= (passed_ticks >> 8)) || ticks_left--;
    XMC_GPIO_SetOutputHigh(LED1); /* indicate activity on LED1 */
  }
  XMC_GPIO_SetOutputLow(LED1);
  XMC_GPIO_SetOutputLow(LED2);
  return n;
}

/* delay a number of milliseconds (busy wait following 24-bit SysTicks) */
void delay_ms(unsigned ms) {
  uint32_t millisecond = ((core_clock + 500) / 1000) << 8;
  uint32_t last_ticks = systick() << 8;
  uint32_t passed_ticks;
  while (ms > 0) {
    passed_ticks = last_ticks - (systick() << 8); /* implicit mod 2^24 */
    if (passed_ticks > millisecond) {
      last_ticks -= millisecond;                  /* implicit mod 2^24 */
      ms--;
    }
  }
}

void ready() {
  /* blink LED1 once for 100 ms to show we are getting ready */
  XMC_GPIO_ToggleOutput(LED1);
  delay_ms(100);
  XMC_GPIO_ToggleOutput(LED1);
}

void main() {
  init();

  ready(); /* you can set a breakpoint here */

  while (1) record(0);
}
