/* lmx2594.c — LMX2594 driver. Bit-bang SPI, reverse-derived register map + math.
 * Refs: research/lmx2594-registers.md, thermal-and-powerdown.md, system-architecture.md */
#include "board.h"
#include "lmx2594.h"

/* ---- low-level GPIO helpers ---- */
static inline void pin_hi(GPIO_TypeDef *p, uint32_t pin){ p->BSRR = (1u << pin); }
static inline void pin_lo(GPIO_TypeDef *p, uint32_t pin){ p->BSRR = (1u << (pin + 16)); }
static void short_delay(void){ for (volatile int i = 0; i < 4; i++) __NOP(); }

/* ---- init register burst, R112 -> R0 (verified dump). R0 last triggers FCAL. ---- */
static const uint32_t lmx_init_regs[] = {
#include "lmx2594_regs.inc"
};
#define LMX_NREGS (sizeof(lmx_init_regs)/sizeof(lmx_init_regs[0]))

/* Register bases carrying non-power config bits — must be preserved (reverse). */
#define R44_BASE 0x2C0023u   /* MASH_ORDER=3,MASH_RESET_N=1,OUTA_PD=0,OUTB_PD=0 */
#define R45_BASE 0x2DC0DFu   /* OUTA_MUX etc + OUTB_PWR[5:0] */
#define R0_ON    0x00251Cu   /* POWERDOWN=0, FCAL_EN=1 (bit3) */
#define R0_PD    0x00251Du   /* POWERDOWN=1 */

/* Toggle FCAL_EN (R0 bit3): clear -> delay -> set. Restarts VCO calibration
 * reliably after a register change (ref toggle_FCAL_EN + datasheet). */
static void lmx_fcal(void)
{
    lmx_write(R0_ON & ~0x8u);                 /* FCAL_EN = 0 */
    for (volatile int i = 0; i < 800; i++) __NOP();  /* ~40us @72MHz */
    lmx_write(R0_ON);                         /* FCAL_EN = 1 -> trigger */
}

/* shadow of the live R44/R45 so per-output toggles don't clobber each other */
static uint32_t r44_shadow = R44_BASE;
static uint32_t r45_shadow = R45_BASE;

void lmx_ce(bool high){ if (high) pin_hi(LMX_CE_PORT, LMX_CE_PIN); else pin_lo(LMX_CE_PORT, LMX_CE_PIN); }

/* 24-bit MSB-first, LMX latches on SCK rising, then CS/LE pulse. Reverse FUN_080069F4. */
void lmx_write(uint32_t frame24)
{
    pin_lo(LMX_SPI_PORT, LMX_CS_PIN);      /* LE low: start */
    pin_lo(LMX_SPI_PORT, LMX_CLK_PIN);
    for (uint32_t mask = 0x800000u; mask; mask >>= 1) {
        if (frame24 & mask) pin_hi(LMX_SPI_PORT, LMX_DATA_PIN);
        else                pin_lo(LMX_SPI_PORT, LMX_DATA_PIN);
        short_delay();
        pin_hi(LMX_SPI_PORT, LMX_CLK_PIN); short_delay();  /* rising edge shifts in */
        pin_lo(LMX_SPI_PORT, LMX_CLK_PIN);
    }
    pin_hi(LMX_SPI_PORT, LMX_CS_PIN);      /* LE high: latch */
    short_delay();
    pin_lo(LMX_SPI_PORT, LMX_CS_PIN);
}

void lmx_init(void)
{
    lmx_ce(true);
    for (volatile int i = 0; i < 200000; i++) __NOP();     /* ~10ms LDO settle */
    for (unsigned i = 0; i < LMX_NREGS; i++) lmx_write(lmx_init_regs[i]);
    r44_shadow = R44_BASE;
    r45_shadow = R45_BASE;
}

/* CHDIV ladder from reverse FUN_08005F50: (threshold_khz_low, chdiv, r75_idx).
 * Pick smallest divider whose VCO lands in 7.5..15 GHz. Thresholds are the exact
 * boundaries the stock firmware used. */
struct chdiv_row { uint32_t lo_khz; uint16_t chdiv; uint8_t idx; };
static const struct chdiv_row CHDIV[] = {
    {3750000,   2, 0}, {2000000,   4, 1}, {1300000,   6, 2}, {940000,    8, 3},
    {680000,  0xC, 4}, {473000, 0x10, 5}, {340000, 0x18, 6}, {237000, 0x20, 7},
    {168000, 0x30, 8}, {138000, 0x40, 9}, {116000, 0x48,10}, {84000,  0x60,11},
    {59300,  0x80,12}, {41500,  0xC0,13}, {29600, 0x100,14}, {20900, 0x180,15},
    {14800, 0x200,16}, {9500,  0x300,17},
};
#define CHDIV_N (sizeof(CHDIV)/sizeof(CHDIV[0]))

bool lmx_set_freq_khz(uint32_t f_khz)
{
    if (f_khz < LMX_FOUT_MIN_KHZ || f_khz > LMX_FOUT_MAX_KHZ) return false;  /* fix B6 */

    /* Output routing: for 7.5-15 GHz the output taps the VCO directly (no channel
     * divider, OUTA_MUX=VCO). Below 7.5 GHz use the channel divider (OUTA_MUX=chdiv).
     * chdiv is chosen so VCO = f*chdiv lands in 7.5..15 GHz. */
    uint16_t chdiv; uint8_t idx; bool vco_direct;
    if (f_khz >= LMX_VCO_MIN_KHZ) {            /* >=7.5 GHz: straight from VCO */
        chdiv = 1; idx = 0; vco_direct = true;
    } else {
        vco_direct = false; chdiv = 0; idx = 0;
        for (unsigned i = 0; i < CHDIV_N; i++)
            if (f_khz >= CHDIV[i].lo_khz) { chdiv = CHDIV[i].chdiv; idx = CHDIV[i].idx; break; }
        if (!chdiv) return false;             /* below lowest band */
    }

    uint64_t vco = vco_direct ? f_khz : (uint64_t)f_khz * chdiv;
    if (vco < LMX_VCO_MIN_KHZ || vco > LMX_VCO_MAX_KHZ) return false;

    uint32_t n   = (uint32_t)(vco / LMX_PFD_KHZ);
    uint32_t rem = (uint32_t)(vco % LMX_PFD_KHZ);
    /* Full 32-bit fractional denominator (PLL_DEN = MAX_DEN = 2^32-1), matching TI
     * TICS Pro / reference impl — max resolution. NUM = round(rem/PFD * DEN). */
    uint32_t den = 0xFFFFFFFFu;
    uint32_t num = (uint32_t)(((uint64_t)rem * den) / LMX_PFD_KHZ);

    /* R75 CHDIV: field [10:6] (mask 0x07C0), code=idx (ref set_Channel_Divider). */
    uint16_t r75 = 0x0800u;                     /* R75 default high bits (reverse) */
    r75 = (uint16_t)((r75 & ~0x07C0u) | ((uint32_t)idx << 6));
    lmx_write(0x4B0000u | r75);
    /* R45 OUTA_MUX[12:11]: 1=VCO (direct), 0=channel divider. Keep OUTB_PWR base. */
    r45_shadow = (r45_shadow & ~(0x3u << 11)) | ((vco_direct ? 1u : 0u) << 11);
    lmx_write(r45_shadow);
    /* R38/R39 = DEN[31:16]/[15:0]; R42/R43 = NUM[31:16]/[15:0] (ref addrs). */
    lmx_write(0x260000u | (den >> 16));
    lmx_write(0x270000u | (den & 0xFFFF));
    lmx_write(0x2A0000u | (num >> 16));
    lmx_write(0x2B0000u | (num & 0xFFFF));
    /* R34 = N[18:16] (bits[2:0]), R36 = N[15:0] — fix B5. */
    lmx_write(0x220000u | ((n >> 16) & 0x7u));
    lmx_write(0x240000u | (n & 0xFFFFu));

    /* MASH_ORDER (R44[2:0]) + PFD_DLY_SEL (R37[13:8]) depend on N / VCO band.
     * Derived from reference nDivider table + LMX2594 datasheet: for fractional
     * mode use MASH=3; PFD_DLY grows with N. Integer (num==0) can use MASH=0. */
    uint8_t mash, pfd_dly;
    if (num == 0)                { mash = 0; pfd_dly = 1; }   /* integer mode */
    else if (n < 12)            { mash = 2; pfd_dly = 2; }
    else if (n < 20)            { mash = 3; pfd_dly = 2; }
    else                         { mash = 3; pfd_dly = 3; }
    r44_shadow = (r44_shadow & ~0x0007u) | mash;             /* keep power/PD bits */
    lmx_write(r44_shadow);
    /* R37 = 0x25; base data 0x0004, PFD_DLY_SEL in [13:8] (mask 0x3F00). */
    lmx_write(0x250000u | ((0x0304u & ~0x3F00u) | ((uint32_t)pfd_dly << 8)));

    lmx_fcal();   /* toggle FCAL_EN: clear->delay->set (ref toggle_FCAL_EN) */
    return true;
}

void lmx_set_power(lmx_output_t out, uint8_t pwr)
{
    if (pwr > LMX_PWR_MAX) pwr = LMX_PWR_MAX;   /* hard cap — last line of defense */
    if (out == LMX_OUTA) { r44_shadow = (r44_shadow & ~(0x3Fu << 8)) | ((uint32_t)pwr << 8); lmx_write(r44_shadow); }
    else                 { r45_shadow = (r45_shadow & ~0x3Fu) | pwr; lmx_write(r45_shadow); }
}

void lmx_output_enable(lmx_output_t out, bool on)
{
    uint32_t bit = (out == LMX_OUTA) ? (1u << 6) : (1u << 7);   /* OUTA_PD=b6, OUTB_PD=b7 */
    if (on) r44_shadow &= ~bit; else r44_shadow |= bit;
    lmx_write(r44_shadow);
}

void lmx_powerdown(bool pd){ lmx_write(pd ? R0_PD : R0_ON); }

bool lmx_locked(void)
{
    /* MUXout (PB10) configured as hardware lock-detect: R0 MUXOUT_LD_SEL=1 (bit2),
     * pin HIGH = locked. Init table sets R0=0x00251C which has bit2=1 already. */
    return (LMX_MUX_PORT->IDR & (1u << LMX_MUX_PIN)) != 0;
}

#ifdef LMX_SELFTEST
#include <assert.h>
/* host build: verify freq math on known points (no hardware). */
static uint64_t calc_vco(uint32_t f){
    uint16_t ch=1; if(f<CHDIV[0].lo_khz) for(unsigned i=0;i<CHDIV_N;i++) if(f>=CHDIV[i].lo_khz){ch=CHDIV[i].chdiv;break;}
    return (uint64_t)f*ch;
}
void lmx_selftest(void){
    assert(calc_vco(15000000)==15000000ull);      /* 15G direct */
    assert(calc_vco(1000000) >=7500000ull && calc_vco(1000000)<=15000000ull);
    assert(calc_vco(100000)  >=7500000ull && calc_vco(100000) <=15000000ull);
    assert(!(20000 < LMX_FOUT_MIN_KHZ));           /* 20M is valid min */
}
#endif
