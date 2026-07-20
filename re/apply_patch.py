#!/usr/bin/env python3
"""Apply LMX2594 STOP-overheat fix to firmware.bin. Writes firmware_patched.bin.
Redirects USB suspend/resume callbacks to thunks in the 0xFF flash tail that
power the LMX2594 down on suspend (R44 OUTx_PD + R0 POWERDOWN) and restore on resume.
Verified against arm-none-eabi-objdump. See re/report_final.md §9."""
import hashlib, pathlib

SRC = pathlib.Path(__file__).parent.parent / "firmware" / "firmware.bin"
DST = pathlib.Path(__file__).parent.parent / "firmware" / "firmware_patched.bin"
EXPECT_MD5 = "4b17c1d805489ce22e26968827f88490"

PATCHES = [  # (file_offset, expected_orig_bytes, new_bytes)
    (0x52ac, "fc f7 e0 ff", "06 f0 80 f9"),  # BL suspend cb -> thunk_powerdown
    (0x4848, "ff f7 3a fa", "06 f0 c0 fe"),  # BL resume cb  -> thunk_restore
    (0xb5b0, None, "00 b5 f6 f7 5d fe 40 f2 e3 10 c0 f2 2c 00 fb f7 19 fa 01 20 fb f7 16 fa 00 bd"),
    (0xb5cc, None, "00 b5 f8 f7 77 fb 40 f2 a3 10 c0 f2 2c 00 fb f7 0b fa 42 f2 1c 50 fb f7 07 fa 00 bd"),
]

def hx(s): return bytes(int(x, 16) for x in s.split())

def main():
    d = bytearray(SRC.read_bytes())
    assert hashlib.md5(d).hexdigest() == EXPECT_MD5, "source firmware md5 mismatch"
    for off, orig, new in PATCHES:
        nb = hx(new)
        if orig is not None:
            assert bytes(d[off:off+len(hx(orig))]) == hx(orig), f"orig mismatch @0x{off:x}"
        else:  # thunk lands in the 0xFF tail — assert it is free
            assert all(b == 0xFF for b in d[off:off+len(nb)]), f"tail not free @0x{off:x}"
        d[off:off+len(nb)] = nb
    DST.write_bytes(d)
    print(f"wrote {DST} ({len(d)} bytes, md5 {hashlib.md5(d).hexdigest()})")

if __name__ == "__main__":
    main()
