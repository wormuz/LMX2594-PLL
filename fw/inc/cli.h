/* cli.h — line-based command parser, shared by USART1 and USB-CDC. */
#ifndef CLI_H
#define CLI_H

#include <stdint.h>

/* Feed received bytes (from UART IRQ or USB OUT). On CRLF, dispatches a command. */
void cli_rx_byte(char c);
/* Output hook: the transport (UART/USB) provides this to send responses. */
void cli_set_output(void (*out)(const char *s));

#endif /* CLI_H */
