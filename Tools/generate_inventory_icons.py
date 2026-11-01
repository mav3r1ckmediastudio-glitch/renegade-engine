"""Original Renegade inventory prototype icon art. No third-party artwork.
Generates transparent 128px PNGs. Only Python and Pillow are needed."""
from PIL import Image, ImageDraw, ImageFilter
from pathlib import Path
import math
OUT=Path(__file__).resolve().parents[1] / "assets" / "InventoryStarter" / "Icons"
OUT.mkdir(parents=True,exist_ok=True)
S=256
def icon(name,draw_subject):
    im=Image.new("RGBA",(S,S),(0,0,0,0))
    d=ImageDraw.Draw(im,'RGBA')
    d.ellipse((34,203,220,236),fill=(0,0,0,65))
    draw_subject(d,im)
    im=im.resize((128,128),Image.Resampling.LANCZOS)
    im.save(OUT/f"{name}.png")
def potion(d,im):
    # Faceted glass, cyan glass edges, luminous crimson alchemical filling.
    d.polygon([(94,35),(162,35),(159,65),(176,86),(199,131),(195,194),
               (172,217),(84,217),(61,194),(57,131),(80,86),(97,65)],
              fill=(39,143,168,155),outline=(185,245,253,220),width=4)
    d.polygon([(94,39),(162,39),(162,62),(94,62)],fill=(62,43,37,255))
    d.rounded_rectangle((91,20,165,45),radius=6,fill=(163,123,65,255),
                        outline=(248,207,122,255),width=3)
    for i in range(3):
        d.line([(103,23+i*5),(154,23+i*5)],fill=(105,77,45,155),width=2)
    d.polygon([(64,136),(189,136),(189,189),(168,207),(86,207),(64,189)],
              fill=(175,26,55,240))
    d.polygon([(66,141),(186,141),(184,153),(67,153)],
              fill=(246,83,108,210))
    d.polygon([(88,89),(67,134),(76,189),(89,202)],
              fill=(164,229,250,72))
    d.line([(89,94),(72,136),(76,170)],fill=(246,255,255,178),width=7)
    d.ellipse((136,156,159,179),fill=(255,180,171,118))
    d.ellipse((120,182,131,193),fill=(251,190,163,140))
    d.line([(107,214),(160,214)],fill=(159,246,255,100),width=3)
def key(d,im):
    d.ellipse((37,58,117,138),fill=(186,144,62,255),
              outline=(255,224,143,255),width=5)
    d.ellipse((63,83,93,113),fill=(37,27,30,0),outline=(84,57,41,255),width=4)
    # Transparent hole using alpha
    d.ellipse((63,84,93,113),fill=(0,0,0,0))
    d.polygon([(106,125),(128,104),(199,175),(190,186),(204,200),
               (184,219),(168,201),(157,212)],fill=(196,143,54,255),
              outline=(252,205,104,255))
    d.line([(118,120),(186,190)],fill=(251,223,140,220),width=6)
def coin(d,im):
    d.ellipse((39,40,212,215),fill=(103,72,39,255),outline=(241,195,87,255),width=5)
    d.ellipse((52,53,199,202),fill=(213,153,61,255),outline=(255,222,116,255),width=6)
    d.ellipse((66,67,186,188),outline=(119,78,32,200),width=5)
    d.polygon([(127,84),(139,112),(169,119),(146,141),(151,172),(125,157),
               (98,171),(105,141),(80,119),(113,112)],fill=(255,226,119,255),
              outline=(171,106,42,240))
def unknown(d,im):
    d.rounded_rectangle((58,55,199,203),radius=29,
                        fill=(45,64,81,220),outline=(123,174,198,240),width=6)
    d.arc((93,78,165,146),195,530,fill=(201,225,236,255),width=16)
    d.ellipse((116,158,135,177),fill=(201,225,236,255))
for name,fn in (("potion",potion),("key",key),("coin",coin),("unknown",unknown)):
    icon(name,fn)
print("Generated original Renegade UI icon assets in",OUT)
