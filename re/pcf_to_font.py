#!/usr/bin/env python3
"""Minimal PCF parser -> pixel-perfect 8x16 glyph table for ASCII + Ukrainian Cyrillic.
Reads /usr/share/fonts/X11/misc/8x16rk.pcf (bitmap font, no antialias by nature).
Outputs fw/inc/font8x16.inc (glyph bytes) + font_map.inc (codepoint->index).
8x16rk uses KOI8-R-ish encoding? No — it's iso10646/unicode-indexed via PCF encodings table."""
import gzip, struct, os

PCF = "/usr/share/fonts/X11/misc/9x18.pcf.gz"   # unicode, full Ukrainian, pixel-perfect
OUTDIR = os.path.join(os.path.dirname(__file__), "..", "fw", "inc")
GW, GH = 9, 18                                    # glyph cell width/height

PCF_BITMAPS      = 1<<3
PCF_BDF_ENCODINGS= 1<<5
PCF_METRICS      = 1<<2
FMT_BYTE_MASK    = 1<<2   # msbyte first
FMT_BIT_MASK     = 1<<3   # msbit first
GLYPHPAD_MASK    = 3

def load(path):
    raw = gzip.open(path,'rb').read() if path.endswith('.gz') else open(path,'rb').read()
    return raw

def main():
    d = load(PCF)
    assert d[:4] == b'\x01fcp', "not a PCF"
    (count,) = struct.unpack_from('<i', d, 4)
    tables = {}
    off = 8
    for _ in range(count):
        typ, fmt, size, offset = struct.unpack_from('<iiii', d, off); off += 16
        tables[typ] = (fmt, size, offset)

    def table_endian(base):
        fmt = struct.unpack_from('<i', d, base)[0]   # format repeated inline, LE
        return ('>' if (fmt & FMT_BYTE_MASK) else '<'), (fmt & FMT_BIT_MASK), (fmt & GLYPHPAD_MASK)

    # ---- ENCODINGS ----
    fmt,size,base = tables[PCF_BDF_ENCODINGS]
    e,_,_ = table_endian(base)
    # after inline format(4): minCB2,maxCB2,minB1,maxB1,defChar (all 16-bit)
    minCB2, maxCB2, minB1, maxB1, defChar = struct.unpack_from(e+'HHHHh', d, base+4)
    idx_base = base + 4 + struct.calcsize(e+'HHHHh')
    ncols = maxCB2 - minCB2 + 1
    nrows = maxB1 - minB1 + 1
    enc = {}
    for r in range(nrows):
        for c in range(ncols):
            (gi,) = struct.unpack_from(e+'H', d, idx_base + (r*ncols+c)*2)
            if gi != 0xFFFF:
                cp = ((minB1+r)<<8)|(minCB2+c)
                enc[cp] = gi

    # ---- BITMAPS ----
    fmt,size,base = tables[PCF_BITMAPS]
    e, bitmsb, glyphpad = table_endian(base)
    (glyph_count,) = struct.unpack_from(e+'i', d, base+4)
    offs_base = base+8
    offsets = [struct.unpack_from(e+'i', d, offs_base+i*4)[0] for i in range(glyph_count)]
    sizes_base = offs_base + glyph_count*4
    bitmapSizes = struct.unpack_from(e+'4i', d, sizes_base)
    padbytes = [1,2,4,8][glyphpad]
    bm_data_base = sizes_base + 16
    bmp = d[bm_data_base:]
    row_bytes = ((GW + padbytes*8 - 1)//(padbytes*8))*padbytes   # bytes per glyph row
    def bit_at(byte, k):                 # k-th pixel (0=left) of a stored row byte(s)
        return 0
    def glyph_rows(gi):
        """Return GH rows; each row = 16-bit int, MSB-aligned, GW significant bits."""
        o = offsets[gi]; rows=[]
        for r in range(GH):
            raw = bmp[o + r*row_bytes : o + r*row_bytes + row_bytes]
            # assemble left-to-right pixel bits into a 16-bit MSB-aligned word
            word = 0; bitpos = 0
            for by in raw:
                b = by if bitmsb else int('{:08b}'.format(by)[::-1], 2)
                for k in range(8):
                    px = (b >> (7-k)) & 1
                    if bitpos < GW: word |= px << (15 - bitpos)
                    bitpos += 1
            rows.append(word)            # 16-bit, top GW bits used
        return rows

    charset = [chr(c) for c in range(0x20,0x7F)] + \
        list("АБВГҐДЕЄЖЗИІЇЙКЛМНОПРСТУФХЦЧШЩЬЮЯабвгґдеєжзиіїйклмнопрстуфхцчшщьюя")
    glyphs=[]; missing=[]
    for ch in charset:
        cp = ord(ch)
        gi = enc.get(cp)
        if gi is None: missing.append(ch); glyphs.append([0]*16)
        else: glyphs.append(glyph_rows(gi))

    # sanity preview
    for ch in ['A','2','Ф','ж','і','Я','Ґ','Є']:
        i=charset.index(ch); print(ch)
        for w in glyphs[i]: print(''.join('#' if w&(0x8000>>x) else '.' for x in range(GW)))
        print()

    # each glyph = GH rows of 16-bit words (2 bytes each, big-endian in the .inc)
    flat=[]
    for g in glyphs:
        for w in g: flat += [ (w>>8)&0xFF, w&0xFF ]
    os.makedirs(OUTDIR, exist_ok=True)
    open(os.path.join(OUTDIR,"font.inc"),"w").write(",".join("0x%02X"%b for b in flat)+"\n")
    open(os.path.join(OUTDIR,"font_map.inc"),"w").write(
        ",".join("{0x%04X,%d}"%(ord(ch),i) for i,ch in enumerate(charset))+"\n")
    print("glyphs:",len(charset),"cell %dx%d"%(GW,GH),"missing:",missing,"bytes:",len(flat))

if __name__=="__main__": main()
