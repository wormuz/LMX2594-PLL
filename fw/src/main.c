/* main.c — clock/GPIO init, USART1 CLI, cooperative superloop. */
#include "board.h"
#include "app.h"
#include "cli.h"
#include "ui.h"
#include "buttons.h"
#include "settings.h"

/* ---- 1ms tick via SysTick ---- */
static volatile uint32_t ms;
void SysTick_Handler(void){ ms++; }
static uint32_t now(void){ return ms; }

/* ---- MCU internal temperature (ADC1 ch16). Board-heat proxy (LMX has no sensor). ---- */
static void adc_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_ADCPRE) | RCC_CFGR_ADCPRE_DIV6;  /* 72/6=12MHz */
    ADC1->CR2 = ADC_CR2_TSVREFE | ADC_CR2_ADON;                        /* enable temp sensor */
    ADC1->SMPR1 |= (7u << 18);                                         /* ch16 max sample time */
    ADC1->SQR3 = 16;                                                   /* channel 16 */
    for (volatile int d=0; d<10000; d++) {}
    ADC1->CR2 |= ADC_CR2_CAL; while (ADC1->CR2 & ADC_CR2_CAL) {}       /* calibrate */
}
int mcu_temp_c(void)
{
    ADC1->CR2 |= ADC_CR2_ADON; while (!(ADC1->SR & ADC_SR_EOC)) {}
    uint32_t v = ADC1->DR;
    int mv = (int)(v * 3300 / 4095);        /* mV */
    /* T = (V25 - Vsense)/Avg_Slope + 25; V25=1430mV, slope=4.3mV/C */
    return (1430 - mv) * 10 / 43 + 25;
}

/* ---- clock: HSE 8MHz -> PLL x9 -> 72MHz ---- */
static void clock_init(void)
{
    RCC->CR |= RCC_CR_HSEON; while(!(RCC->CR & RCC_CR_HSERDY)){}
    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_2;
    RCC->CFGR = RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL9 | RCC_CFGR_PPRE1_DIV2;  /* PCLK1=36M */
    RCC->CR |= RCC_CR_PLLON; while(!(RCC->CR & RCC_CR_PLLRDY)){}
    RCC->CFGR |= RCC_CFGR_SW_PLL; while((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL){}
    SystemCoreClockUpdate();
}

static void gpio_mode(GPIO_TypeDef *p, unsigned pin, unsigned mode_cnf) /* 4-bit MODE|CNF */
{
    volatile uint32_t *cr = (pin < 8) ? &p->CRL : &p->CRH;
    unsigned s = (pin & 7) * 4;
    *cr = (*cr & ~(0xFu << s)) | ((mode_cnf & 0xF) << s);
}

static void gpio_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN;

    /* LMX SPI outputs (PB1/11/12) + CE (PA3), push-pull 50MHz = 0x3 */
    gpio_mode(LMX_SPI_PORT, LMX_CS_PIN, 0x3);
    gpio_mode(LMX_SPI_PORT, LMX_DATA_PIN, 0x3);
    gpio_mode(LMX_SPI_PORT, LMX_CLK_PIN, 0x3);
    gpio_mode(LMX_CE_PORT,  LMX_CE_PIN, 0x3);
    gpio_mode(LMX_MUX_PORT, LMX_MUX_PIN, 0x4);   /* MUXout = input floating (lock detect) */
    /* LCD outputs PB5/6/7/8 */
    gpio_mode(LCD_PORT, LCD_SCLK_PIN, 0x3); gpio_mode(LCD_PORT, LCD_MOSI_PIN, 0x3);
    gpio_mode(LCD_PORT, LCD_RST_PIN, 0x3);  gpio_mode(LCD_PORT, LCD_DC_PIN, 0x3);
    /* buttons input pull-up (MODE=00 CNF=10 => 0x8), set ODR=1 for pull-up */
    gpio_mode(BTN_LEFT_PORT, BTN_LEFT_PIN, 0x8);  BTN_LEFT_PORT->ODR |= (1u<<BTN_LEFT_PIN);
    gpio_mode(BTN_DOWN_PORT, BTN_DOWN_PIN, 0x8);  BTN_DOWN_PORT->ODR |= (1u<<BTN_DOWN_PIN);
    gpio_mode(BTN_RIGHT_PORT,BTN_RIGHT_PIN,0x8);  BTN_RIGHT_PORT->ODR |= (1u<<BTN_RIGHT_PIN);
    gpio_mode(BTN_UP_PORT,   BTN_UP_PIN, 0x8);    BTN_UP_PORT->ODR |= (1u<<BTN_UP_PIN);
    gpio_mode(BTN_CENTER_PORT,BTN_CENTER_PIN,0x8);BTN_CENTER_PORT->ODR |= (1u<<BTN_CENTER_PIN);
    /* USART1: PA9 TX = AF-PP 50MHz (0xB), PA10 RX = input floating (0x4) */
    gpio_mode(GPIOA, CLI_TX_PIN, 0xB);
    gpio_mode(GPIOA, CLI_RX_PIN, 0x4);
}

/* ---- USART1 @115200, RX interrupt -> cli_rx_byte ---- */
static void uart_out(const char *s){ while(*s){ while(!(CLI_USART->SR & USART_SR_TXE)){} CLI_USART->DR = (uint8_t)*s++; } }

static void uart_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    CLI_USART->BRR = PCLK2_HZ / CLI_BAUD;                 /* 72M/115200 = 625 */
    CLI_USART->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE;
    NVIC_EnableIRQ(USART1_IRQn);
    cli_set_output(uart_out);
}
void USART1_IRQHandler(void)
{
    if (CLI_USART->SR & USART_SR_RXNE) cli_rx_byte((char)(CLI_USART->DR & 0xFF));  /* fix B3 */
}

int main(void)
{
    clock_init();
    SysTick_Config(SYSCLK_HZ / 1000u);   /* 1ms */
    gpio_init();
    adc_init();
    uart_init();
    buttons_init();
    ui_init();
    app_init();          /* loads settings, inits LMX, applies output/thermal state */

    for (;;) {
        btn_t b = buttons_poll(now());
        if (b != BTN_NONE) { ui_handle(b); }
        app_tick(now());
        ui_refresh();
        /* USB-CDC service would go here (usb_cdc_poll -> cli_rx_byte) */
    }
}
