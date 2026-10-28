from pathlib import Path
import math,struct,zlib,sys
out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
def png(name,mist):
 size=128;raw=bytearray()
 for y in range(size):
  raw.append(0)
  for x in range(size):
   u=(x+.5)/size*2-1;v=(y+.5)/size*2-1;r=math.hypot(u,v)
   a=max(0,1-r*r)**(2.5 if mist else 1.5)
   if mist:a*=.72+.18*math.sin(u*16+math.sin(v*11))+.1*math.cos(v*23-u*9)
   raw.extend((255,255,255,round(max(0,min(1,a))*255)))
 def chunk(t,b):return struct.pack('>I',len(b))+t+b+struct.pack('>I',zlib.crc32(t+b)&0xffffffff)
 data=bytes([137,80,78,71,13,10,26,10])+chunk(b'IHDR',struct.pack('>IIBBBBB',size,size,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(bytes(raw),9))+chunk(b'IEND',b'')
 (out/name).write_bytes(data)
png('spray.png',False);png('mist.png',True)
print('ORIGINAL_WATERFALL_SPRITES_READY')
