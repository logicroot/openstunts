#!/usr/bin/env python3
# Generate src/world/track_route_data.h from reference/restunts dseg.asm: the
# TRKOBJINFO connection table (entry/exit point + type per sub-block, plus the
# si_opp3 runway marker) for every track element, the extra fields the
# checkpoint walker (seg001.asm sub_18D60) reads (si_arrowType,
# si_arrowOrient, si_cameraDataOffset, si_opp1:si_opp2, si_oppSpedCode), each
# element's TRACKOBJECT.ss_multiTileFlag, and the camera-data tables those
# offsets point into.
#
# Usage: python3 tools/gen_route_data.py
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DSEG = os.path.join(ROOT, "reference/restunts/src/restunts/asmorig/dseg.asm")
OUT = os.path.join(ROOT, "src/world/track_route_data.h")
lines = open(DSEG, encoding="latin-1").read().split("\n")

start = next(i for i, l in enumerate(lines) if re.match(r"^dseg\s+segment\b", l.strip(), re.I)) + 1
end = next(i for i in range(start, len(lines)) if re.match(r"^dseg\s+ends\b", lines[i].strip(), re.I))

def strip_comment(s):
    out=[]; q=None
    for ch in s:
        if q:
            out.append(ch)
            if ch==q: q=None
        elif ch in "'\"":
            q=ch; out.append(ch)
        elif ch==";": break
        else: out.append(ch)
    return "".join(out)

def parse_int(tok):
    tok=tok.strip(); neg=tok.startswith("-")
    if neg: tok=tok[1:]
    if tok.startswith(("0x","0X")): v=int(tok,16)
    elif re.fullmatch(r"[0-9A-Fa-f]+h",tok): v=int(tok[:-1],16)
    elif re.fullmatch(r"[0-9]+",tok): v=int(tok)
    else: return None
    return -v if neg else v

data={}; labels={}; off=0
def emit(b):
    global off
    data[off]=b&0xFF; off+=1

def split_items(rest):
    items=[]; cur=""; q=None
    for ch in rest:
        if q:
            cur+=ch
            if ch==q: q=None
        elif ch in "'\"": q=ch; cur+=ch
        elif ch==",": items.append(cur); cur=""
        else: cur+=ch
    if cur.strip(): items.append(cur)
    return items

i=start
while i<end:
    line=strip_comment(lines[i]).strip(); i+=1
    if not line: continue
    m=re.match(r"^([A-Za-z_][A-Za-z0-9_$]*)\s*:\s*(.*)$", line)
    if m:
        labels.setdefault(m.group(1), off); line=m.group(2).strip()
        if not line: continue
    mm=re.match(r"^([A-Za-z_][A-Za-z0-9_$]*)\s+(db|dw|dd)\b(.*)$", line, re.I)
    if mm:
        labels.setdefault(mm.group(1), off); directive=mm.group(2).lower(); rest=mm.group(3)
    else:
        mm=re.match(r"^(db|dw|dd)\b(.*)$", line, re.I)
        if not mm: continue
        directive=mm.group(1).lower(); rest=mm.group(2)
    unit={"db":1,"dw":2,"dd":4}[directive]
    for it in split_items(rest):
        it=it.strip()
        if not it: continue
        m2=re.match(r"^(.+?)\s+dup\s*\((.*)\)$", it, re.I)
        if m2:
            cnt=parse_int(m2.group(1)) or 0
            inner=[x.strip() for x in m2.group(2).split(",") if x.strip()]
            for _ in range(cnt):
                for sub in inner:
                    v=parse_int(sub)
                    for k in range(unit): emit((v>>(8*k))&0xFF if v is not None else 0)
            continue
        if it[0] in "'\"":
            q=it[0]; s=it[1:-1] if it.endswith(q) else it[1:]
            for ch in s:
                emit(ord(ch))
                for k in range(1,unit): emit(0)
            continue
        v=parse_int(it)
        for k in range(unit): emit(0 if v is None else (v>>(8*k))&0xFF)

def u16(o): return data.get(o,0)|(data.get(o+1,0)<<8)
def i16(o):
    v=u16(o); return v-0x10000 if v>=0x8000 else v

toff=labels["trkObjectList"]
ptrs=[u16(toff+n*14) for n in range(215)]

# Per-element TRACKOBJECT fields the walker/checkpoint code reads.
multitile=[data.get(toff+n*14+11,0) for n in range(215)]

# Decode blocks. Connection fields plus the checkpoint fields:
#   arrow_type  = si_arrowType      (byte +5)
#   arrow_orient= si_arrowOrient    (word +6)
#   cam_offset  = si_cameraDataOffset (word +8)
#   opp1_word   = si_opp1:si_opp2   (word +0x0A)
#   opp_sped    = si_oppSpedCode    (byte +13)
elems=[]
maxb=0
for n in range(215):
    p=ptrs[n]
    if p==0:
        elems.append((0,[])); continue
    nb=data.get(p,0)
    blocks=[]
    for b in range(nb):
        q=p+b*14
        blocks.append((
            data.get(q+1,0), data.get(q+2,0), data.get(q+3,0), data.get(q+4,0),
            data.get(q+12,0), data.get(q+5,0), u16(q+6), u16(q+8), u16(q+10),
            data.get(q+13,0)))
    maxb=max(maxb,nb)
    elems.append((nb,blocks))
print("max blocks per element:", maxb, "elements with blocks:", sum(1 for e in elems if e[0]), file=sys.stderr)

# ---- camera-data tables --------------------------------------------------
# Every block's camera data is two consecutive 6-byte VECTORs at
# camera_offset + 6*var_10, where var_10 spans 0..(arrow_type-1)*2. So each
# table spans arrow_type-1 steps of 12 bytes plus the final 12-byte pair.
def span_hi(offset, arrow_type):
    steps=max(arrow_type-1, 0)
    return offset + steps*12 + 11

accesses=[]  # (offset, arrow_type)
for nb,blocks in elems:
    for blk in blocks:
        cam=blk[7]; opp1=blk[8]; at=blk[5]
        if cam: accesses.append((cam, at))
        if opp1: accesses.append((opp1, at))
assert accesses, "no camera data referenced"
assert all(o % 2 == 0 for o,_ in accesses), "odd camera offset"
cam_base=min(o for o,_ in accesses)
cam_top=max(span_hi(o, at) for o,at in accesses)
cam_count=(cam_top - cam_base)//2 + 1
print("camera data: base", cam_base, "top", cam_top, "words", cam_count, file=sys.stderr)

flat=[]
eoff=[]
ecnt=[]
for nb,blocks in elems:
    eoff.append(len(flat))
    ecnt.append(nb)
    flat.extend(blocks)

with open(OUT,"w") as f:
    f.write("// Generated by tools/gen_route_data.py from reference/restunts dseg.asm.\n")
    f.write("// TRKOBJINFO connection + checkpoint tables for every track element (0..214).\n")
    f.write("// Do not edit by hand.\n")
    f.write("#pragma once\n\n#include <cstdint>\n\nnamespace s3d::world {\n\n")
    f.write("// One TRKOBJINFO block: the connection data used by the route walker\n")
    f.write("// (entry/exit point 1..12, connection types, si_opp3 runway marker) plus\n")
    f.write("// the fields the checkpoint walker reads. `camera_offset`/`opp1_word` are\n")
    f.write("// absolute dseg byte offsets into kCameraData.\n")
    f.write("struct RouteBlockDef {\n")
    f.write("  uint8_t entry_point;\n  uint8_t exit_point;\n")
    f.write("  uint8_t entry_type;\n  uint8_t exit_type;\n  uint8_t opp3;\n")
    f.write("  uint8_t arrow_type;\n  uint16_t arrow_orient;\n")
    f.write("  uint16_t camera_offset;\n  uint16_t opp1_word;\n")
    f.write("  uint8_t opp_sped_code;\n};\n\n")
    f.write(f"inline constexpr int kRouteBlockCount = {len(flat)};\n")
    f.write("inline constexpr RouteBlockDef kRouteBlocks[] = {\n")
    for j in range(0,len(flat),4):
        row=flat[j:j+4]
        f.write("    " + " ".join(
            f"{{{a},{b},{c},{d},{e},{fat},{fo},{co},{o1},{sp}}}," for
            (a,b,c,d,e,fat,fo,co,o1,sp) in row) + "\n")
    f.write("};\n\n")
    f.write("inline constexpr int kRouteElementCount = 215;\n")
    f.write("// Index of the first block in kRouteBlocks, and the block count, per element.\n")
    f.write("inline constexpr uint16_t kRouteElementBlockOffset[] = {\n    ")
    f.write(", ".join(str(x) for x in eoff))
    f.write("\n};\n")
    f.write("inline constexpr uint8_t kRouteElementBlockCount[] = {\n    ")
    f.write(", ".join(str(x) for x in ecnt))
    f.write("\n};\n")
    f.write("// TRACKOBJECT.ss_multiTileFlag per element.\n")
    f.write("inline constexpr uint8_t kRouteElementMultiTile[] = {\n    ")
    mt=[str(x) for x in multitile]
    for j in range(0,len(mt),24):
        f.write(", ".join(mt[j:j+24]) + (",\n    " if j+24 < len(mt) else ",\n"))
    f.write("};\n\n")
    f.write("// The raw TRKOBJINFO camera/arrow points (seg001.asm sub_18D60), stored\n")
    f.write("// as little-endian signed 16-bit words. `kCameraDataBase` is the dseg\n")
    f.write("// byte offset of kCameraData[0]; a RouteBlockDef offset indexes it via\n")
    f.write("//   &kCameraData[(offset - kCameraDataBase) / 2].\n")
    f.write(f"inline constexpr int kCameraDataBase = {cam_base};\n")
    f.write(f"inline constexpr int kCameraDataCount = {cam_count};\n")
    f.write("inline constexpr int16_t kCameraData[] = {\n    ")
    cd=[str(i16(cam_base+2*k)) for k in range(cam_count)]
    for j in range(0,len(cd),12):
        f.write(", ".join(cd[j:j+12]) + (",\n    " if j+12 < len(cd) else ",\n"))
    f.write("};\n\n")
    f.write("}  // namespace s3d::world\n")
print("wrote", OUT, "blocks", len(flat), file=sys.stderr)
