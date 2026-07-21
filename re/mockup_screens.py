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
txt(px,4,2,"LOCK",GREEN); txt(px,120,2,"UART",CYAN)
line(im,22)
txt(px,66,28,"Частота, кГц",GREY)
txt(px,30,54,"2 400 000",WHITE,scale=2)
line(im,98)
txt(px,6,108,"Down-вихід: ON   45",GREEN)   # увімкнено
txt(px,6,132,"Up-вихід:   OFF  31",GREY)
line(im,160)
txt(px,6,172,"Температура: 38 C",GREY)
line(im,196)
txt(px,6,206,"3.28В",GREY); txt(px,120,206,"центр—меню",GREY)
save(im,"lmx_ui_1_home.png")

# ===== 2. МЕНЮ =====
im,px=cv()
txt(px,80,4,"МЕНЮ",CYAN)   # x=80 як код
line(im,24)
# точно як paint_menu: MENU[] + Y0=30, ROW_H=20; Down/Up зі станом
rows=[("Задати частоту",WHITE),("Down-вихід: ON",GREEN),("Up-вихід:   OFF",GREY),
      ("Потужність виходів",WHITE),("Зберегти налаштування",WHITE),
      ("Налаштування системи",WHITE),("Назад",WHITE)]
for i,(r,c) in enumerate(rows):
    if i==0: txtbg(im,6,30+i*20,r)
    else: txt(px,6,30+i*20,r,c)
line(im,214); txt(px,6,220,"вліво — назад",GREY)
save(im,"lmx_ui_2_menu.png")

# ===== 3. ЗАДАТИ ЧАСТОТУ (порозрядно) =====
im,px=cv()
txt(px,30,6,"Задати частоту",CYAN)   # x=30 y=6 як код
# код: 8 цифр "%08lu", x=20 y=70, крок x += GLYPH_W+8; активний = інверсія
digits="02400000"; x=20; y=70; active=3   # edit_digit=3 напр.
d=ImageDraw.Draw(im)
for i,ch in enumerate(digits):
    if i==active: d.rectangle([x-1,y-1,x+GW,y+GH],fill=WHITE); draw_glyph(px,x,y,ord(ch),BLACK,WHITE,False,1)
    else: draw_glyph(px,x,y,ord(ch),YELLOW,BLACK,False,1)
    x+=GW+8
txt(px,80,120,"кГц",GREY)
txt(px,60,150,"крок: 100000",GREEN)   # pow10(FREQ_DIGITS-1-active)
txt(px,6,200,"L/R розряд  U/D змінити",GREY)
save(im,"lmx_ui_3_setfreq.png")

# (Свіп прибрано з UI — лишається в коді для UART-сумісності, без екрана.)

# ===== 5. НАЛАШТУВАННЯ СИСТЕМИ =====
im,px=cv()
txt(px,80,4,"Система",CYAN)   # title = paint_list("Система",...) at x=80
line(im,24)
# рядки й позиції точно як код: SYS[] + Y0=30, ROW_H=20
rows=["Опора: 10 МГц","UART: 115200","При старті: OFF",
      "Яскравість: 80%","Скинути","Назад"]
for i,r in enumerate(rows):
    if i==0: txtbg(im,6,30+i*20,r)   # sel=0 default, inverted
    else: txt(px,6,30+i*20,r,WHITE)
line(im,214); txt(px,6,220,"вліво — назад",GREY)
save(im,"lmx_ui_5_system.png")
print("готово: 5 екранів (реальний 9x18 шрифт)")
