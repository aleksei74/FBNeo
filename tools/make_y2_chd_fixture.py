"""Synthetic, compressed V3 CHD fixture (no game data)."""
import struct
import sys
import zlib
import hashlib

HUNK=16896
N=96
SECTOR=4224
raw=[bytes((i*31+j*17+(j>>8))&255 for j in range(HUNK)) for i in range(N)]
packed=[]
for r in raw:
    compressor=zlib.compressobj(6,zlib.DEFLATED,-15)
    packed.append(compressor.compress(r)+compressor.flush())
meta=b"CYLS:96,HEADS:1,SECS:4,BPS:4224\0"
metaoff=120+16*N+16
offset=metaoff+16+len(meta)
header=bytearray(120)
header[:8]=b"MComprHD"
struct.pack_into(">IIIIIQQ",header,8,120,3,0,1,N,N*HUNK,metaoff)
struct.pack_into(">I",header,76,HUNK)
header[80:100]=hashlib.sha1(b"".join(raw)).digest()
with open(sys.argv[1],"wb") as f:
    f.write(header)
    for r,c in zip(raw,packed):
        f.write(struct.pack(">QIHBB",offset,zlib.crc32(r),len(c)&65535,len(c)>>16,1))
        offset+=len(c)
    f.write(b"EndOfListCookie\0")
    f.write(b"GDDD"+struct.pack(">IQ",len(meta),0)+meta)
    for c in packed:f.write(c)
