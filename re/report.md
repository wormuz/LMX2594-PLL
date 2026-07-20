I have all the deep-dive data needed. Producing the synthesis report directly.

# Реверс-інжиніринг прошивки LMX2594-EVAL (STM32F103, firmware.bin)

## 1. Огляд архітектури прошивки

Прошивка збудована на bare-metal STM32F103 (Cortex-M3, Thumb-2), 64 KiB flash @ `0x08000000`. Ланцюг старту: `Reset_Handler → FUN_080002a0 → FUN_08006410` (головний цикл, ніколи не повертається). Значна частина образу - newlib/soft-float бібліотека (vfprintf-рушій, `__adddf3/__muldf3/__divdf3/dtoa`).

Пристрій - генератор частоти на LMX2594 з керуванням через:
- LCD-меню з поворотним енкодером (29 полів редагування);
- USB-CDC CLI (текстові команди `start/stop/w1../w2../w3../wt../wv..`).

Ключові особливості реалізації:
- SPI до LMX2594 - програмний bit-bang на GPIOB (апаратний SPI не задіяний ніде в образі);
- частотна математика - програмна IEEE-754 double (softfp), не апаратний FPU (його немає в M3);
- MCU ніколи не входить у STOP/SLEEP: немає жодного опкоду `WFI` (`0xBF30`) і жодного посилання на базу PWR (`0x40007000`) у всьому образі. Головний цикл - нескінченний polling `do{}while(true)`.

"usb low power mode" у цій прошивці стосується виключно USB-периферії (D+/D- suspend за специфікацією USB, біти CNTR/ISTR), і повністю відв'язане і від сну CPU, і від керування LMX2594.

## 2. Карта функцій по підсистемах

### Boot / libc / soft-float

| addr | роль | периферія |
|------|------|-----------|
| `0x080002a0` | реальний reset/startup: RCC init → `FUN_0800027c` → `FUN_08006410` | none |
| `0x08000040` | startup-трамплін (Thumb PIC-veneer) | none |
| `0x0800027c` | libc reentrancy/errno-context setup | none |
| `0x08000f58` | Cortex-M3 fault/stack-frame clearing (не USB) | none |
| `0x080001ec` | vfprintf конвертер специфікаторів | none |
| `0x080003b8` | core format-string scanner (vfprintf) | none |
| `0x08000304`/`0x08000328` | printf/sprintf-обгортки | none |
| `0x08000540` | strlen (SWAR) | none |
| `0x0800057e` | memset/bzero (unrolled) | none |
| `0x08000a9c` | dtoa (double→ASCII) | none |
| `0x08007024`/`0x08007220`/`0x08006cf4` | `__adddf3`/`__subdf3`/`__muldf3` | none |
| `0x08006ffe`/`0x08006fa4` | int→double / double→uint32 | none |
| `0x08001708`/`0x080012a0` | `__divdf3`/`__muldf3` | none |

### USB core / CDC

| addr | роль | периферія |
|------|------|-----------|
| `0x080058d4` | USB ISR-диспетчер (CTR/RESET/SUSP/WKUP/SOF/ESOF/ERR з ISTR) | USB `0x40005c00` |
| `0x080019b8` | USB EP dispatch: читає EPnR, чистить CTR_RX/TX, диспетч на RX/TX callback-таблиці | USB EPnR (+0xc00) |
| `0x08001c40`/`0x08001c74` | EP TX/RX callback-стаби (STAT_TX/RX toggle) | USB EPnR |
| `0x08001ca8` | SET/GET_LINE_CODING handler (28-байт struct, копіює 14 байт) | USB (CDC) |
| `0x08001d08`/`0x08001dc8` | CDC bulk-IN/OUT data-pump state machines | USB (EP-object) |
| `0x08001e84` | EP0 class-request state machine (bmRequestType/bRequest) | USB |
| `0x08005224` | SUSP-bit ISR handler (PMA save + виклик suspend cb) | USB |
| `0x08002270` | **suspend cb**: друк рядка + `RAM[0x20001ED0]=3` | none (БАГ) |
| `0x08003cc0` | resume cb: друк + `RAM_gate=1/5` | none |
| `0x08004838` | WKUP-bit handler | USB |
| `0x0800485c` | reader `RAM[0x20001ED0]==5` → CDC-TX pump (flow control) | none |

### LMX2594 / freq / UI

| addr | роль | периферія |
|------|------|-----------|
| `0x080069f4` | **bit-bang SPI write** 24 біт MSB-first до LMX2594 | GPIOB `0x40010c00` |
| `0x08006a6c` | обгортка над `FUN_080069f4` (base freq write) | GPIOB |
| `0x08005f50` | freq→register compute+program (вхід: freq_kHz, outpower) | GPIOB (через SPI) |
| `0x08005ec4` | power/attenuator write (кандидат на powerdown, викл. лише з key-handler) | GPIOB |
| `0x08006410` | **main_sm**: init + нескінченний цикл sweep/CLI | LCD/GPIO/USB |
| `0x080040a8` | обробник клавіш/енкодера (29 полів меню) | GPIO |
| `0x08003480` | LCD-рендер F1/F2/Step/OUTPOWER/state | LCD |
| `0x0800194c` | LCD command sender (0x2a/0x2b/0x2c address-set) | LCD bit-bang |

## 3. Керування LMX2594

### SPI (bit-bang, GPIOB `0x40010c00`)

`FUN_080069f4(uint param_1)` @ `0x080069f4` - запис 24 біт MSB-first. Кадр: біти[23:16]=addr, біти[15:0]=data. Помічники BSRR(+0x14)/BRR(+0x10) через `FUN_080024da`/`FUN_080024de`:
- CS/LE = біт `0x0002` (low на початку, high в кінці - LE-імпульс защіпки);
- CLK = біт `0x1000`;
- DATA (MOSI) = біт `0x0800` (маска стартує з `0x800000`).

Апаратний SPI1 (`0x40013000`)/SPI2 не задіяні ніде - підтверджено відсутністю літералів у пулі.

### Таблиця регістрів / init-послідовність

Розташована `0x080029f0–0x08002d0a`, по одному `bl 0x080069f4` на регістр, спадаючи R112(0x70)→R37(0x25): `0x700000, 0x6f0000, ... 0x2b0000, 0x2a0000, 0x290000, 0x280000, ...`.

Більшість регістрів - фіксовані константи; R44/R45 та частина freq-регістрів беруться з RAM (динамічні, від `FUN_08005f50`). Сегмент таблиці, який оглянуто, - лише R37–R112; **R0/R1 (POWERDOWN) у цій таблиці відсутні**, окремий блок R0–R36 у цьому проході не захоплено.

### Freq math (`FUN_08005f50`)

1. Вибір дільника: 18-смуговий ladder проти порогів `DAT_08006258..0x08006288` (kHz), обирає CHDIV `local_48` (2,4,6,8,0xc,...,0x300) та індекс `local_4c` (0..0x11).
2. VCO-ціль = `freq_kHz * divider` через softfp double (`FUN_08006ffe/08006cf4/08007024/08007220/08006fa4`).
3. N-integer = VCO/ref (`DAT_08006298`), truncate через `FUN_08006fa4`.
4. FRAC numerator = `(vco - N*ref) * 50000`, потім /ref → `uVar4`.
5. Фінальні SPI-записи:
   - `DAT_0800629c | local_4c<<6` - CHDIV-регістр;
   - `DAT_080062a0 | outpower<<8` - **OUTA_PWR: OUTPOWER з UI пишеться прямо в біти [14:8], без масштабування**;
   - `uVar4&0xffff | 0x2b0000` / `uVar4>>0x10 | 0x2a0000` - FRAC low/high (R43/R42 addr 0x2b/0x2a);
   - `uVar3 | 0x240000` - N-integer (R36 addr 0x24);
   - `0x251c` фікс - R37 (FCAL/PLL config).

### OUTPOWER / R44-R45

R44 (addr `0x2c`) і R45 (`0x2d`) - RAM-джерело (`ldr r0,[0x08002d5c]`/`[0x08002d60]`), тобто OUTx_PWR/OUTx_PD пишуться щоразу при зміні freq/power, але **тільки в нормальному режимі тюнінгу, ніколи з suspend**.

## 4. USB-CDC CLI

Парсер у головному циклі `FUN_08006410`. Буфер `_DAT_08006844` (10 байт ASCII), готовність гейтить `*DAT_08006840 & 0x8000`:

| команда | дія | ціль |
|---------|-----|------|
| `start` | sweep ON, `DAT_08006824=1`, LCD "START" | - |
| `stop` | sweep OFF, `DAT_08006824=0`, LCD "STOP " | - |
| `w1<8 digits>` | F1 | `DAT_0800681c` |
| `w2<8 digits>` | F2 | `DAT_0800682c` |
| `w3<8 digits>` | Step | `DAT_08006828` |
| `wt<4 digits>` | SpanTime (ms) | `DAT_08006820` |
| `wv<2 digits>` | OUTPOWER | `*_DAT_080068ec` |

Після кожної команди: `FUN_08005f50(freq,outpower)` (програмування LMX), `FUN_08003480(...)` (LCD refresh), `FUN_08005ec4(...)` (power/attenuator).

Line-coding (baud/stop/parity) обробляється в `FUN_08001ca8` (SET/GET_LINE_CODING), але значення суто зберігаються - фізичного UART немає.

## 5. Меню / sweep

Sweep-логіка в `FUN_08006410` (кожна ітерація циклу):

```
if (F2==cur && sweep_enabled) latch = cur;
if (sweep_enabled && cur<F2 && tick_flag && dwell_elapsed==1) {
    tick_flag=0;
    cur += Step;
    FUN_08005f50(cur, outpower);   // репрограмування LMX2594
    if (cur >= F2) cur = F1;       // wrap
}
FUN_080040a8();                    // обробник клавіш/енкодера
```

Стан-змінні: F1 `DAT_0800681c`, F2 `DAT_0800682c`, Step `DAT_08006828`, SpanTime `DAT_08006820`, OUTPOWER `_DAT_080068ec`, sweep-flag `DAT_08006824`, tick/dwell `DAT_08006838/3c`, акумулятор `DAT_08006834`.

Обробник `FUN_080040a8` читає код енкодера через `FUN_0800266c(0)`:
- `3`/`1` = CW/CCW: індекс меню `*DAT_08004434` ±1 mod `0x1d` (29 полів);
- `2`/`4` = "-"/"+": редагування конкретного десяткового розряду поля (idx 0-7→F1, 8-15→F2, 16-23→Step, 24-27→SpanTime, 28-29→OUTPOWER), кожне редагування одразу репрограмує LMX через `FUN_08005f50` + LCD-refresh;
- `5` = confirm: репрограмує freq, перемикає bool `*DAT_08004458`, викликає `FUN_08003b3c`+`FUN_08005ec4`.

Зміни freq/power застосовуються live, без окремого commit-кроку.

## 6. ROOT CAUSE нагріву в STOP

### Точний ланцюг

USB-suspend ISR `FUN_08005224` @ `0x08005224` (гілка біта SUSP `0x400` у диспетчері `FUN_080058d4`) робить PMA-save (копіювання EP-буферів) і викликає callback:

```c
void FUN_08002270(void)              // @0x08002270
{
  FUN_08000328(s_usb_enter_low_power_mode_08002280);  // друк рядка
  *DAT_0800229c = 3;                 // RAM[0x20001ED0] = 3
  return;
}
```

Що cb **робить**: друкує "usb enter low power mode" і ставить прапорець стану USB `RAM[0x20001ED0]=3`.

Що cb **НЕ робить** (підтверджено повним перебором посилань):
- жодного виклику `FUN_080069f4` (SPI-запис до LMX2594);
- жодного доступу до GPIO/CS/LE LMX;
- жодного запису R0 POWERDOWN, R44/R45 OUTx_PD, чи будь-якого вимкнення виходу.

`RAM[0x20001ED0]` - чистий прапорець стану USB: пишеться 3 (suspended)/1 або 5 (resumed) двома callback'ами; читається лише `FUN_0800485c` (==5 → CDC-TX pump). Це USB flow-control, не керування RF-чипом.

**Механізм перегріву**: при USB-suspend (host в STOP, кабель у сплячий host) синтезатор LMX2594 лишається повністю запрограмованим і з активним RF-виходом (OUTA_PWR не занулено, POWERDOWN не встановлено). Оскільки MCU до того ж ніколи не входить у власний STOP (немає `WFI`), струм і тепловиділення тримаються на робочому рівні необмежено. Це і є підтверджений, не гіпотетичний, механізм.

### Які регістри LMX треба гасити

Для коректного power-down при suspend слід записати через `FUN_080069f4(value)`:

1. **R44 OUTA_PD / OUTB_PD** (addr `0x2c`) - встановити біти power-down виходів A/B. Кадр: `0x2c0000 | <data з виставленими OUTA_PD/OUTB_PD>`. Поточне live-значення береться з `[0x08002d5c]` - для гасіння OR-нути біти PD.
2. **R0[0] POWERDOWN** (addr `0x00`) - встановити біт 0. Кадр: `0x000000 | 1` = `0x00000001`. Це вимикає весь чип (VCO/PLL/виходи) - найнадійніший захист від перегріву.

При resume (`FUN_08003cc0`) - зворотній запис: R0[0]=0, потім повне репрограмування через `FUN_08005f50(cur, outpower)` (FCAL перекалібрування VCO обов'язкове після POWERDOWN).

### Де саме вставити fix

- **Suspend**: у тілі `FUN_08002270` @ `0x08002270`, після встановлення прапорця `*DAT_0800229c=3`, перед `return`. Функція зараз мінімальна (лише bl `FUN_08000328` + store), місця для вставки виклику достатньо.
- **Resume**: у `FUN_08003cc0` @ `0x08003cc0`, у гілці нормального resume (`*RAM_gate=1`), додати `FUN_08005f50(cur,outpower)` для повторного програмування.

Готова функція `FUN_08005ec4` (power/attenuator write, викликається з key-handler'а case 5) - найкращий кандидат для повторного використання як "output disable", якщо її семантика саме OUTx_PD; інакше - прямий `FUN_080069f4(0x00000001)` для R0 POWERDOWN.

## 7. Конкретний план патча

Оскільки прошивка bare-metal без vector-table суворих обмежень, рекомендований шлях - патч на рівні C-джерела (якщо доступне) або бінарний патч flash.

### Варіант A (перекомпіляція джерела, кращий)

1. У `FUN_08002270` (suspend cb) додати перед `return`:
   ```c
   FUN_080069f4(0x2c0000 | (*(uint*)0x08002d5c ? ... : 0) | OUTA_PD | OUTB_PD); // гасимо виходи
   FUN_080069f4(0x00000001);  // R0 POWERDOWN=1
   ```
   Спочатку power-down виходів, потім весь чип - щоб уникнути сплеску.
2. У `FUN_08003cc0` (resume cb), гілка нормального resume:
   ```c
   FUN_080069f4(0x00000000);          // R0 POWERDOWN=0
   FUN_08005f50(cur_freq, outpower);  // повне репрограмування + FCAL
   ```
   Врахувати затримку ~10 мс на FCAL-калібрування VCO перед активацією виходу.

### Варіант B (бінарний патч firmware.bin)

`FUN_08002270` зараз (декомпіляція): `push {lr}; ldr r0,=str; bl FUN_08000328; ldr r1,=DAT_0800229c; movs r0,#3; str r0,[r1]; pop {pc}`.

Потрібно замінити фінальний `pop {pc}` на `bl` до нового thunk у вільній flash-області, який:
```
movs r0, #1          ; 0x2001
lsls r0, r0, #? / формує 0x00000001
bl   FUN_080069f4    ; R0 POWERDOWN
... (аналогічно R44 OUTx_PD)
pop  {pc}
```
Знайти вільний хвіст flash (padding `0xFF` в кінці 64 KiB), розмістити thunk, пропатчити outbound-адресу `bl`.

Перед патчем **дообстежити**:
1. Точні бітові поля R44 (OUTA_PD=bit?, OUTB_PD=bit?) і R0[0] за datasheet TI LMX2594 - у цьому проході бітлейаут R0/R44 з коду не витягнуто повністю (R0-R36 блок не захоплено).
2. Семантику `FUN_08005ec4` - чи це готовий output-enable/disable, який можна викликати з мінусовим аргументом для гасіння.
3. Точне значення `cur_freq` для resume-репрограмування - взяти з акумулятора `*DAT_08006834`, не з F1.

Не інвентаризовано в цьому проході: повний блок R0–R36 init (окрема таблиця, не в `0x080029f0–0x08002d0a`), точні бітполя R0/R44, адреси `DAT_0800629c`/`DAT_080062a0`. Перед фінальним патчем ці три пункти треба закрити прямим оглядом коду/datasheet.