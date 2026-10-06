#!/usr/bin/env python3
"""Hard-edged (pixel-art) downscale of the 192 px console masters.

    python3 tools/crisp_icons.py            # regenerates every -24/-48/-64.png

The icons are drawn 1:1 with nearest sampling on the PSP, so any blur is in
the image itself: a smooth (averaging) downscale blends every edge into the
background. 192 divides evenly into 64, 48 and 24 (3x, 4x, 8x blocks).

Each output pixel takes the dominant colour of its source block: colours are
clustered per block, the most-covered opaque cluster wins and is averaged
within itself, so edges never blend into the background and flat fills stay
flat. Coverage below half the block becomes fully transparent."""
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
def crisp(src,size,dst):
    w,h,rows=rd(src); g=w//size; out=[]
    for y in range(size):
        for x in range(size):
            pix=[]; cov=0
            for yy in range(g):
                r=rows[y*g+yy]
                for xx in range(g):
                    o=(x*g+xx)*4; a=r[o+3]; cov+=a
                    if a>=160: pix.append((r[o],r[o+1],r[o+2]))
            if cov < 128*g*g or not pix: out+=[0,0,0,0]; continue
            # cluster: quantize to 5 bits/channel, pick the most common bucket
            buckets={}
            for c in pix:
                k=(c[0]>>4,c[1]>>4,c[2]>>4); buckets.setdefault(k,[]).append(c)
            best=max(buckets.values(),key=len)
            out+=[sum(c[0] for c in best)//len(best),sum(c[1] for c in best)//len(best),sum(c[2] for c in best)//len(best),255]
    wr(dst,size,size,out)
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
