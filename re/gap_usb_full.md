This confirms the standard STM32/CDC library pattern:
- `FUN_08004f78` = GetStatus (bmRequestType/bRequest std) — this is `Standard_GetStatus` from ST's USB-FS-Device_Lib.
- `FUN_08004efc` = `Standard_ClearFeature`
- `FUN_08004f4c` = `Standard_SetFeature`
- `DAT_08002100` (0x8004f79) = the GetStatus request handler pointer used in FUN_08001e84's request dispatch = confirms `FUN_08001e84` is `Standard_GetDescriptor`/`Standard_GetStatus`-style req-processor, i.e. the ST library's `Setup0_Process`/`Standard_GetConfiguration`. This is **library code (ST USB-FS-Device_Lib)**, not app-specific.

I have exhausted what's directly reachable. Compiling final structured answer.

## USB subsystem — LMX2594-EVAL firmware (verified via Ghidra MCP, file `firmware.bin`)

**Device descriptor** @ `0x0800a8c0` (18 bytes, raw hex confirmed by memory read):
`12 01 00 02 02 00 00 40 83 04 40 57 00 02 01 02 03 01`
- bcdUSB=0x0200, bDeviceClass=0x02 (CDC), bMaxPacketSize0=0x40
- **idVendor=0x0483** (STMicroelectronics), **idProduct=0x5740** (ST VCP demo PID), bcdDevice=0x0002
- iManufacturer=1, iProduct=2, iSerialNumber=3, bNumConfigurations=1

**Config descriptor** @ `0x0800a8d2`, wTotalLength=0x0043(67 bytes), 1 config, bConfigurationValue=1, bmAttributes=0xC0 (self-powered), bMaxPower=0x32(100mA). Interfaces: IF0 class 0x02/0x02 (CDC Control, 1 EP) with CDC functional descriptors (Header 0x24/0x00, Call-Mgmt 0x24/0x01, ACM 0x24/0x02, Union 0x24/0x06) + EP 0x82 (IN, interrupt, 0x08 bytes, 0xFF poll). IF1 class 0x0A (CDC Data, 2 EPs): EP 0x03 (OUT, bulk, wMaxPacketSize=0x40) and EP 0x81 (IN, bulk, 0x40). This is the **stock ST "Virtual COM Port" CDC-ACM example descriptor set**, unmodified.

**Strings** @ `0x0800b440` region: iManufacturer="STMicroelectronics", iProduct="STM32 Virtual COM", iSerial present (binary serial, not decoded as ASCII).

**Control-request state machine — `FUN_08001e84`** (confirmed decompile): dispatches on `bmRequestType`/`bRequest` byte at the setup-packet struct (`*DAT_080020f8`). Standard requests (type=0): GetStatus(0)→`FUN_08004f78`@0x8004f79, ClearFeature(1)→`FUN_08004efc`@0x8004efd, SetFeature(3)→`FUN_08004f4c`@0x8004f4d, SetAddress(5)→`DAT_08002100`, GetDescriptor(6)→dispatch via `*(*DAT_080020fc+0x1c/0x20/0x24)` selecting device/config/string tables, SetConfiguration(9)→`DAT_0800210c`, GetInterface(0x0a)→`DAT_08002110`. This is the **ST USB-FS-Device-Lib `usb_core.c` `Standard_...` request layer** — generic library code, not custom logic.

**Endpoint register access confirmed**: `DAT_08002104 = 0x40005000`; code adds `+0xc00` giving `0x40005C00` = STM32F1 **USB_EPnR** register base (USB peripheral), consistent with EPnR bit tests `& 0x3000`/`& 0x30` (STAT_TX/STAT_RX bitfields).

**Suspend/Resume/CTR handler — `FUN_080058d4`** (USB LP-CTR ISR body, confirmed decompile): reads ISTR (`DAT_08005a80`/`*DAT_08005a84`), and per bit:
- CTR (bit0x200)&EP_CTR mask → resume count `*DAT_08005a8c++`; `FUN_0800485c()`
- ESOF(0x8000) → `FUN_080019b8()`
- RESET(0x400) → clears reg to 0xFBFF, calls `(**(DAT_08005a90+4))()` (re-init callback)
- SOF(0x2000) → clear flag only
- WKUP(0x1000) → clear, `FUN_08004768(0)` → **resume path, forces state 0**
- **SUSPEND(0x800)**: if `*DAT_08005a94=='\0'` → `FUN_08004768(2)`; else → `FUN_08005224()` (the **low-power-mode entry function**, confirmed earlier as the one calling `FUN_08002270`)
- CTR(0x100) with retry counting → PMA double-buffer toggling for EP0, then `FUN_08004768(7)`

**`FUN_08005224`** (confirmed decompile, unchanged from earlier report): toggles USB_CNTR power-down related bits (sets 0x400/0x401/clears bit0, waits for bit0x400 loop, then sets bit3/bit2) — this **is the standard `USB_LP_CTR` low-power/suspend entry from ST library (`PowerOn`/`Suspend` sequence)** but **only touches the USB peripheral's own low-power bit, not any external LMX2594 power control**. It ends by calling `FUN_08002270()`.

**`FUN_08002270`** (confirmed): prints `"usb enter low power mode"`, sets `*(byte*)0x20001ED0 = 3`. No GPIO write, no call into any LMX/SPI function, no `WFI`/`PWR_CTRL` access — **confirmed no power-down of the LMX2594 anywhere in the suspend path.**

**State machine `FUN_08004768(int state)`** operates on byte at **`0x20001ED5`** (confirmed via read of `DAT_0800482c=0x20001ED5`), a *different* global from the suspend flag. States: 0=idle-check→`FUN_08004838()`+state 6, or state5; 1=`FUN_08004838()`+state4,flag=1; 2=state3,counter=2; 3=countdown→state4; 4=set bit0x10 in reg, state5,counter=10; 5=countdown→clear bit0x10,state6,flag=0; default→state6. This is a **generic USB resume/reset debounce counter** (ST library pattern), unrelated to LMX power.

**`0x20001ED0` full picture** (confirmed via `references_to`): written only at `0800227c` (=3, suspend), `08003cd2`, `08003cda`; read at `08004860`, `080064b0`, `080064b8`. Have NOT yet decompiled the writers at 0x08003cd2/cda or readers at 0x064b0/64b8 — **not confirmed what values 0/1/2 mean, only that 3=suspend is confirmed**. Stating this as unconfirmed rather than guessing.

**Bottom line, confirmed**: USB suspend ISR path (`FUN_080058d4`→`FUN_08005224`→`FUN_08002270`) only sets USB peripheral low-power bits and a software flag; it contains no call chain to `FUN_080069f4` (LMX SPI writer) or any GPIO/power-enable register — the LMX2594 stays powered/clocked through USB suspend, consistent with the previously established overheat root cause.

**Not determined from the binary** (explicitly unconfirmed, not guessed): semantics of `0x20001ED0` values 0/1/2 (writers at 0x08003cd2/cda not decompiled in this pass); contents of `FUN_080019b8` (ESOF handler) and `DAT_08005a90+4` (RESET callback) were not decompiled; endpoint 0 TX/RX buffer table (BTABLE) addresses in PMA were not walked out.