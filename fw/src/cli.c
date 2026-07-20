/* cli.c — command parser. Legacy contract (start/stop/w1..wv, kHz, CRLF) verified
 * exhaustively against stock FUN_08006410, PLUS new commands for dual-output control.
 * Fix B7: length + range validation before applying. See cli-command-reference.md. */
#include "cli.h"
#include "app.h"
#include "board.h"
#include <string.h>
#include <stdio.h>

#define LINE_MAX 32
static char line[LINE_MAX];
static uint8_t len;
static void (*out_fn)(const char *) = 0;

void cli_set_output(void (*out)(const char *s)){ out_fn = out; }
static void reply(const char *s){ if (out_fn) out_fn(s); }

/* parse exactly n decimal digits starting at s; return false if any non-digit. */
static bool parse_digits(const char *s, uint8_t n, uint32_t *out)
{
    uint32_t v = 0;
    for (uint8_t i = 0; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') return false;
        v = v * 10u + (uint32_t)(s[i] - '0');
    }
    *out = v;
    return true;
}

static bool freq_ok(uint32_t khz){ return khz >= LMX_FOUT_MIN_KHZ && khz <= LMX_FOUT_MAX_KHZ; }

static void dispatch(void)
{
    uint32_t v;

    if (!strcmp(line, "start")) { app_sweep(true);  reply("START\r\n"); return; }
    if (!strcmp(line, "stop"))  { app_sweep(false); reply("STOP\r\n");  return; }
    if (line[0] == '?')         { char b[64]; app_status(b, sizeof b); reply(b); return; }
    if (line[0] == 's' && len == 1) { app_save(); reply("SAVED\r\n"); return; }
    if (line[0] == 'r' && len == 1) { NVIC_SystemReset(); return; }

    char r[48];
    if (line[0] == 'w') {
        char c = line[1];
        if ((c=='1'||c=='2'||c=='3') && len == 10 && parse_digits(line+2, 8, &v)) {
            if ((c!='3') && !freq_ok(v)) { reply("ERR range\r\n"); return; }
            if (c=='1') app_set_f1(v); else if (c=='2') app_set_f2(v); else app_set_step(v);
            snprintf(r,sizeof r,"OK %c=%lu kHz\r\n", c, (unsigned long)v); reply(r); return;
        }
        if (c=='t' && len == 6 && parse_digits(line+2, 4, &v)) {
            app_set_dwell((uint16_t)v); snprintf(r,sizeof r,"OK dwell=%lu ms\r\n",(unsigned long)v); reply(r); return; }
        if (c=='v' && len == 4 && parse_digits(line+2, 2, &v)) {
            uint8_t p=(uint8_t)(v>63?63:v); app_set_power(0,p);
            snprintf(r,sizeof r,"OK OUTA pwr=%u\r\n",p); reply(r); return; }
        if (c=='a' && len == 3 && (line[2]=='0'||line[2]=='1')) {
            app_output_enable(0, line[2]=='1'); snprintf(r,sizeof r,"OK Down=%s\r\n",line[2]=='1'?"ON":"OFF"); reply(r); return; }
        if (c=='b' && len == 3 && (line[2]=='0'||line[2]=='1')) {
            app_output_enable(1, line[2]=='1'); snprintf(r,sizeof r,"OK Up=%s\r\n",line[2]=='1'?"ON":"OFF"); reply(r); return; }
        if (c=='f' && len == 10 && parse_digits(line+2, 8, &v)) {
            if (!freq_ok(v)) { reply("ERR range\r\n"); return; }
            app_set_lo(v); snprintf(r,sizeof r,"OK LO=%lu kHz\r\n",(unsigned long)v); reply(r); return;
        }
    }
    if (line[0] == 'p' && (line[1]=='a'||line[1]=='b') && len == 4 && parse_digits(line+2, 2, &v)) {
        uint8_t p=(uint8_t)(v>63?63:v); app_set_power(line[1]=='b', p);
        snprintf(r,sizeof r,"OK OUT%c pwr=%u\r\n", line[1]=='b'?'B':'A', p); reply(r); return;
    }
    reply("ERR unknown cmd\r\n");
}

void cli_rx_byte(char c)
{
    if (c == '\r') return;                 /* wait for \n */
    if (c == '\n') { line[len] = 0; if (len) dispatch(); len = 0; return; }
    if (len < LINE_MAX - 1) line[len++] = c;
    else len = 0;                          /* overflow: drop line */
}
