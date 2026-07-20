Have enough to produce structured findings.

## RESET → main entry chain
- `FUN_080002a0` (0x080002a0) = actual reset/startup: calls `FUN_08000f58()`(clock/RCC init)→`FUN_0800027c`→`FUN_08006410()` (**main_sm**, never returns — infinite `do{}while(true)`)→ dead code after (`FUN_08001120`, jump-table warnings) unreachable.
- `FUN_080002c0` referenced in task is a different/adjacent stub; real chain is `Reset_Handler → FUN_080002a0 → FUN_08006410`.

## main_sm = FUN_08006410 (0x08006410, size 1128B)
Init block: LCD/GPIO setup (`FUN_08006364`,`FUN_08003d78(0x500)`), SPI/LMX2594 base freq `FUN_08006a6c(0x2580)`=9600 write, `FUN_08002630`,`FUN_08003ec4`,LCD color regs `*DAT_08006810=0xffff`(white)/`*DAT_08006814=0xf800`(red), USB init `FUN_08001ca8`, `FUN_08005aac(0)/(1)` (reset toggle), then `FUN_08005f50`+`FUN_08003480` — initial freq program + LCD draw.

Infinite loop body, key state vars (all globals via DAT_*, base 0x20001Exx presumably):
- `*DAT_0800681c` = **F1** (current/start freq, Hz*10 units)
- `*DAT_0800682c` = **F2** (stop freq)
- `*DAT_08006828` = **Step** (freq increment)
- `*DAT_08006820` = **SpanTime** (sweep dwell, ms)
- `*_DAT_080068ec` = **OUTPOWER**
- `*DAT_08006824` = sweep-enable flag ('\x01' = running)
- `*DAT_08006838`/`*DAT_0800683c` = tick/dwell-elapsed flags gating step advance
- `*DAT_08006834` = current sweep freq accumulator

Sweep step logic (each loop iteration):
```
if (F2==F1_current && sweep_enabled) latch = F1_current;
if (sweep_enabled && F1_current<F2 && tick_flag && dwell_elapsed==1) {
    tick_flag=0;
    cur += Step;
    FUN_08005f50(cur, outpower);      // reprogram LMX2594 freq
    if (cur >= F2) cur = F1;          // wrap sweep
}
FUN_080040a8();                       // key/rotary-encoder handler (menu nav, see below)
```

USB/UART command parser (`_DAT_08006844` = 10-byte ASCII command buffer, gated by `*DAT_08006840 & 0x8000` = "command ready" flag):
- `"start"` → `DAT_08006824=1` (sweep ON), redraws "START", programs freq
- `"stop"` → `DAT_08006824=0` (sweep OFF), redraws "STOP "
- `"w1..."` → parses 8-digit ASCII → **F1** (`DAT_0800681c`), label "Freq1"
- `"w2..."` → parses 8-digit ASCII → **F2** (`DAT_0800682c`), label "Freq2"
- `"w3..."` → parses 8-digit ASCII → **Step** (`DAT_08006828`)
- `"wt..."` → 4-digit ASCII → **SpanTime** (`DAT_08006820`)
- `"wv.."` → 2-digit ASCII → **OUTPOWER** (`*_DAT_080068ec`)
After each command branch: `FUN_08005f50(freq,outpower)` (LMX2594 program), `FUN_08003480(...)` (LCD refresh of F1/F2/Step/state text), `FUN_08005ec4(...)` (unresolved, likely power/attenuator write).

## Physical key handler FUN_080040a8 (0x080040a8)
Reads key/encoder code via `FUN_0800266c(0)` → `iVar2`:
- `3` = encoder CW: menu index `*DAT_08004434` increments mod 0x1d (29 menu fields), wraps via `FUN_080062e0` (redraw cursor)
- `1` = encoder CCW: decrements mod 0x1d
- `2` = "-" key: subtracts digit-place value from the field selected by menu index range: idx 0-7→F1 digit (`*DAT_0800443c`), 8-15→F2 (`DAT_08004440`), 16-23→Step (`DAT_08004444`), 24-27→SpanTime(`DAT_0800444c`), 28-29→OUTPOWER(`DAT_08004454`); each range calls corresponding LCD digit-redraw fn (`FUN_08002f4c/08003108/080032c4/08003c28/08003af4`) then re-programs LMX2594 (`FUN_08005f50`) + refreshes LCD (`FUN_08003b3c`)
- `4` = "+" key: same field ranges, adds instead of subtracts, with upper-bound clamp checks (`<= DAT_0800445c`, `<10000`, `<0x40`)
- `5` = "confirm/enter" key: reprograms freq (`FUN_08005f50`), toggles a bool at `*DAT_08004458` (likely OUTPOWER-on/off or sweep direction), calls `FUN_08003b3c`+`FUN_08005ec4`

So the LCD menu digit editing (rotary encoder selects digit position 0-0x1d across F1/F2/Step/SpanTime/OUTPOWER fields) and +/- keys edit individual decimal digits in-place, each edit immediately re-triggers `FUN_08005f50` → SPI program of LMX2594 — confirming freq/power changes are applied live, no separate "commit" step except for the encoder confirm key (case 5) which additionally flips a run/power toggle.

**Not yet resolved**: exact semantics of `FUN_08005ec4` (likely LMX2594 power-down/output-enable register write — candidate site where STOP/suspend *should* call powerdown but doesn't; only called from key-handler paths, never from USB suspend `FUN_08002270`) and `FUN_08003b3c` (LCD text redraw, unconfirmed). RCC/GPIO/SPI base-address correlation for `FUN_08006a6c`/`FUN_08005f50` SPI writes not individually verified against SPI1=0x40013000 in this pass — file `firmware.bin`, addresses as given.