/* lmx2594.h — TI LMX2594 driver (bit-bang SPI). Register map + math from reverse. */
#ifndef LMX2594_H
#define LMX2594_H

#include <stdint.h>
#include <stdbool.h>

/* Two outputs at the same frequency: OUTA=RFa=Down-conv, OUTB=RFb=Up-conv. */
typedef enum { LMX_OUTA = 0, LMX_OUTB = 1 } lmx_output_t;

void     lmx_init(void);                 /* CE pulse + full R112..R0 burst + FCAL */
void     lmx_write(uint32_t frame24);    /* raw 24-bit {addr[23:16],data[15:0]} */
bool     lmx_set_freq_khz(uint32_t f_khz);   /* program LO; false if out of range */
void     lmx_set_power(lmx_output_t out, uint8_t pwr);   /* 0..63 */
void     lmx_output_enable(lmx_output_t out, bool on);   /* per-output PD bit */
void     lmx_powerdown(bool pd);         /* R0 POWERDOWN — whole chip low power */
void     lmx_ce(bool high);              /* hardware CE pin */
bool     lmx_locked(void);               /* read MUXout lock-detect */

#endif /* LMX2594_H */
