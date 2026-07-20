#!/usr/bin/env python3
"""Макети екранів прошивки LMX2594 — рендер РЕАЛЬНИМ 9x18 шрифтом прошивки
(ті самі гліфи що на залізі, з fw/inc/font.inc + font_map.inc). Pixel-perfect.
240x240 ST7789. Вивід PNG на ~/Desktop, масштаб 3x."""
import os
from PIL import Image, ImageDraw

W=H=240; S=3; GW,GH=9,18
BLACK=(0,0,0); WHITE=(235,235,235); RED=(248,50,50); GREEN=(0,220,90)
GREY=(120,120,140); CYAN=(70,205,235); YELLOW=(240,210,60)
INC=os.path.join(os.path.dirname(__file__),"..","fw","inc")
DESK=os.path.expanduser("~/Desktop"); os.makedirs(DESK,exist_ok=True)

# ---- load firmware font ----
def loadbytes(p):
    return [int(x,16) for x in open(p).read().strip().rstrip(",").split(",")]
FONT=loadbytes(os.path.join(INC,"font.inc"))
FMAP={}
mp=open(os.path.join(INC,"font_map.inc")).read().strip().rstrip(",")
for pair in mp.split("},{"):
    cp,idx=pair.strip("{}").split(","); FMAP[int(cp,16)]=int(idx)

def glyph_rows(cp):
    idx=FMAP.get(cp,0); base=idx*GH*2
    return [(FONT[base+r*2]<<8)|FONT[base+r*2+1] for r in range(GH)]

def cv(): im=Image.new("RGB",(W,H),BLACK); return im,im.load()
def draw_glyph(px,x,y,cp,fg,bg,inv,scale=1):
    rows=glyph_rows(cp)
    for r in range(GH):
        for b in range(GW):
            on=(rows[r]&(0x8000>>b))!=0
            if inv: on=not on
            c=fg if on else bg
            if c is None: continue
            for sy in range(scale):
                for sx in range(scale):
                    X=x+b*scale+sx; Y=y+r*scale+sy
                    if 0<=X<W and 0<=Y<H: px[X,Y]=c
def txt(px,x,y,s,fg=WHITE,bg=None,inv=False,scale=1):
    for ch in s:
        draw_glyph(px,x,y,ord(ch),fg,(bg if bg else BLACK),inv,scale)
        x+=GW*scale
    return x
def txtbg(im,x,y,s,fg=WHITE,bg=WHITE,scale=1):   # inverted: solid bg bar
    d=ImageDraw.Draw(im); w=len(s)*GW*scale
    d.rectangle([x-1,y-1,x+w,y+GH*scale],fill=bg)
    px=im.load();
    for ch in s: draw_glyph(px,x,y,ord(ch),BLACK,bg,False,scale); x+=GW*scale
def line(im,y,c=GREY): ImageDraw.Draw(im).line([0,y,W,y],fill=c)
def save(im,n): im.resize((W*S,H*S),Image.NEAREST).save(f"{DESK}/{n}"); print("wrote",n)

# ===== 1. ГОЛОВНИЙ (усе вимкнено за замовч.) =====
im,px=cv()
txt(px,4,2,"Вимкнено",GREY); txt(px,120,2,"UART 115200",CYAN)
line(im,22)
txt(px,84,30,"Частота",GREY)
txt(px,14,52,"12 450 000",GREY,scale=2); txt(px,200,62,"кГц",GREY)
line(im,96)
txt(px,6,108,"Down-вихід: вимк  45",GREY)
txt(px,6,132,"Up-вихід:   вимк  31",GREY)
line(im,160)
txt(px,6,172,"Температура: 24 C",GREY)
line(im,196)
txt(px,6,206,"центр — меню",GREY)
save(im,"lmx_ui_1_home.png")

# ===== 2. МЕНЮ =====
im,px=cv()
txt(px,96,3,"МЕНЮ",CYAN)
line(im,24)
rows=["Задати частоту","Down-вихід","Up-вихід","Потужність виходів",
      "Зберегти налаштування","Налаштування системи","Назад"]
for i,r in enumerate(rows):
    if i==0: txtbg(im,6,34+i*26,r)
    else: txt(px,6,34+i*26,r,WHITE)
save(im,"lmx_ui_2_menu.png")

# ===== 3. ЗАДАТИ ЧАСТОТУ (порозрядно) =====
im,px=cv()
txt(px,40,6,"Задати частоту",CYAN)
line(im,28)
freq="12 450 000"; x=20; y=60; active=4; ci=0
d=ImageDraw.Draw(im)
for ch in freq:
    isd=ch.isdigit(); sel=(isd and ci==active)
    if sel: d.rectangle([x-1,y-1,x+GW*2,y+GH*2],fill=WHITE); draw_glyph(px,x,y,ord(ch),BLACK,WHITE,False,2)
    else: draw_glyph(px,x,y,ord(ch),YELLOW,BLACK,False,2)
    if isd: ci+=1
    x+=GW*2+2
txt(px,90,116,"крок: 100 кГц",GREEN)
line(im,150)
txt(px,6,156,"вліво/вправо — розряд",GREY)
txt(px,6,178,"вгору/вниз — змінити",GREY)
txt(px,6,200,"центр — зберегти",GREY)
save(im,"lmx_ui_3_setfreq.png")

# (Свіп прибрано з UI — лишається в коді для UART-сумісності, без екрана.)

# ===== 5. НАЛАШТУВАННЯ СИСТЕМИ =====
im,px=cv()
txt(px,20,4,"Налаштування",CYAN)
line(im,24)
rows=["Опорна:  10 МГц","UART:    115200","При старті: OFF",
      "Яскравість: 80%","Скинути","Назад"]
for i,r in enumerate(rows):
    if i==1: txtbg(im,6,32+i*26,r)
    else: txt(px,6,32+i*26,r,WHITE)
save(im,"lmx_ui_5_system.png")
print("готово: 5 екранів (реальний 9x18 шрифт)")
