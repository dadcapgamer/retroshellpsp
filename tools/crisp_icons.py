#!/usr/bin/env python3
"""Exact integer downscale of the 192 px console masters.

    python3 tools/crisp_icons.py            # regenerates every -24/-48/-64.png

The icons are drawn 1:1 with nearest sampling on the PSP, so their quality
is decided here. 192 divides evenly into 64, 48 and 24 (3x, 4x, 8x blocks),
and each output pixel is the alpha-weighted average of its block: colour is
averaged only over covered texels (no dark fringe from transparent black),
coverage becomes alpha. Lines stay continuous and edges get one pixel of
true anti-aliasing. (A "dominant colour" hard-edged reduction was tried and
rejected: thin outlines and curves broke into stair-steps on the device.)"""
import zlib,struct,sys
def rd(p):
    d=open(p,'rb').read(); i=8; idat=b''
    while i<len(d):
        n=struct.unpack('>I',d[i:i+4])[0]; t=d[i+4:i+8]; c=d[i+8:i+8+n]; i+=12+n
        if t==b'IHDR': w,h,bd,ct=struct.unpack('>IIBB',c[:10])
        if t==b'IDAT': idat+=c
    ch=4; raw=zlib.decompress(idat); s=w*ch; prev=bytearray(s); p=0; rows=[]
    for y in range(h):
        f=raw[p];p+=1;cur=bytearray(raw[p:p+s]);p+=s
        for x in range(s):
            a=cur[x-ch] if x>=ch else 0;b=prev[x];cc=prev[x-ch] if x>=ch else 0
            if f==1:cur[x]=(cur[x]+a)&255
            elif f==2:cur[x]=(cur[x]+b)&255
            elif f==3:cur[x]=(cur[x]+(a+b)//2)&255
            elif f==4:
                pa=abs(b-cc);pb=abs(a-cc);pc=abs(a+b-2*cc);pr=a if pa<=pb and pa<=pc else(b if pb<=pc else cc);cur[x]=(cur[x]+pr)&255
        rows.append(cur);prev=cur
    return w,h,rows
def wr(p,w,h,px):
    raw=b''.join(b'\x00'+bytes(px[y*w*4:(y+1)*w*4]) for y in range(h))
    def ch(t,d): return struct.pack('>I',len(d))+t+d+struct.pack('>I',zlib.crc32(t+d)&0xffffffff)
    open(p,'wb').write(b'\x89PNG\r\n\x1a\n'+ch(b'IHDR',struct.pack('>IIBBBBB',w,h,8,6,0,0,0))+ch(b'IDAT',zlib.compress(raw,9))+ch(b'IEND',b''))
def crisp(src, size, dst):
    w, h, rows = rd(src)
    g = w // size
    out = []
    for y in range(size):
        for x in range(size):
            r = gg = b = cov = 0
            for yy in range(g):
                row = rows[y * g + yy]
                for xx in range(g):
                    o = (x * g + xx) * 4
                    a = row[o + 3]
                    r += row[o] * a
                    gg += row[o + 1] * a
                    b += row[o + 2] * a
                    cov += a
            if cov == 0:
                out += [0, 0, 0, 0]
            else:
                out += [r // cov, gg // cov, b // cov, cov // (g * g)]
    wr(dst, size, size, out)

if __name__=="__main__":
    import glob, os
    if len(sys.argv) == 4:
        crisp(sys.argv[1], int(sys.argv[2]), sys.argv[3])
    else:
        root = os.path.join(os.path.dirname(__file__), "..", "assets", "icons", "consoles")
        for src in sorted(glob.glob(os.path.join(root, "*-192.png"))):
            for size in (24, 48, 64):
                dst = src.replace("-192.png", f"-{size}.png")
                crisp(src, size, dst)
                print(os.path.relpath(dst))
