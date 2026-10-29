#!/usr/bin/env python3
"""tinyLINK silkscreen logo footprints
(KiBuzzard UbuntuMono-B text knocked out of a filled pill, inside a 0.2 mm stadium ring).

usage: python3 -I gen_logo.py <KiBuzzard plugin dir> <output .pretty dir>
"""
import math, os, sys, uuid

KB = sys.argv[1]
OUT = sys.argv[2]
sys.path.insert(0, os.path.join(KB, 'deps', 'fonttools', 'Lib'))
from fontTools.ttLib import TTFont
from fontTools.pens.recordingPen import RecordingPen
from fontTools.pens.basePen import decomposeQuadraticSegment

TEXT = 'tinyLINK'
# Scale and baseline match the logo of another project, so both come out the same size.
SCALE = 0.681 / 281
BASELINE = 0.623
PILL_R = 1.193861
RING_R = 1.588466
RING_W = 0.2
RING_S = 4.80865
PILL_HALF = 5.959410
TRACK = 0
DOT_LIFT = 0
STEP = 0.04


def glyph_contours():
    font = TTFont(os.path.join(KB, 'buzzard', 'typeface', 'UbuntuMono-B.ttf'))
    gs, cmap = font.getGlyphSet(), font.getBestCmap()
    adv = gs[cmap[ord('o')]].width * SCALE + TRACK
    x0 = -(adv * len(TEXT) - TRACK) / 2
    out = []
    for i, ch in enumerate(TEXT):
        rec = RecordingPen()
        gs[cmap[ord(ch)]].draw(rec)
        ox = x0 + i * adv
        tf = lambda p: (ox + p[0] * SCALE, BASELINE - p[1] * SCALE)
        cur, last = [], None
        for op, args in rec.value:
            if op == 'moveTo':
                cur = [tf(args[0])]; last = args[0]
            elif op == 'lineTo':
                cur.append(tf(args[0])); last = args[0]
            elif op == 'qCurveTo':
                for c, e in decomposeQuadraticSegment(args) if len(args) > 2 else [args]:
                    n = max(2, int(math.dist(tf(last), tf(e)) / STEP))
                    for k in range(1, n + 1):
                        t = k / n
                        x = (1 - t) ** 2 * last[0] + 2 * (1 - t) * t * c[0] + t * t * e[0]
                        y = (1 - t) ** 2 * last[1] + 2 * (1 - t) * t * c[1] + t * t * e[1]
                        cur.append(tf((x, y)))
                    last = e
            elif op == 'closePath':
                if math.dist(cur[0], cur[-1]) < 1e-6:
                    cur.pop()
                out.append(cur)
        if ch == 'i' and DOT_LIFT:
            dot = min(out[-2:], key=lambda c: max(y for x, y in c))
            dot[:] = [(x, y - DOT_LIFT) for x, y in dot]
    return out, -x0


def area(p):
    return sum(p[i][0] * p[i - 1][1] - p[i - 1][0] * p[i][1] for i in range(len(p))) / 2


def crosses_vertical(x, ya, yb, poly):
    lo, hi = min(ya, yb), max(ya, yb)
    for i in range(len(poly)):
        (x1, y1), (x2, y2) = poly[i - 1], poly[i]
        if (x1 - x) * (x2 - x) < 0:
            y = y1 + (y2 - y1) * (x - x1) / (x2 - x1)
            if lo < y < hi:
                return True
    return False


def knocked_out_pill(holes, half):
    s = half - PILL_R
    outer_sign = None
    bottom, top = [], []
    for h in holes:
        others = [o for o in holes if o is not h]
        lo = max(h, key=lambda p: p[1])
        hi = min(h, key=lambda p: p[1])
        if not any(crosses_vertical(lo[0], lo[1], PILL_R, o) for o in others):
            k = h.index(lo); bottom.append((lo[0], h[k:] + h[:k]))
        else:
            k = h.index(hi); top.append((hi[0], h[k:] + h[:k]))
    path = [(-s, PILL_R)]
    for bx, h in sorted(bottom):
        path += [(bx, PILL_R)] + h + [h[0], (bx, PILL_R)]
    path.append((s, PILL_R))
    n = max(8, int(math.pi * PILL_R / STEP))
    path += [(s + PILL_R * math.sin(math.pi * k / n), PILL_R * math.cos(math.pi * k / n)) for k in range(1, n)]
    path.append((s, -PILL_R))
    for bx, h in sorted(top, reverse=True):
        path += [(bx, -PILL_R)] + h + [h[0], (bx, -PILL_R)]
    path.append((-s, -PILL_R))
    path += [(-s - PILL_R * math.sin(math.pi * k / n), -PILL_R * math.cos(math.pi * k / n)) for k in range(1, n)]
    return path


def orient_holes(holes):
    return [h if area(h) > 0 else h[::-1] for h in holes]


def u(): return str(uuid.uuid4())


def fp(name, descr, layer, text_polys, mirror):
    fx = (lambda x: -x) if mirror else (lambda x: x)
    fab = 'B.Fab' if layer.startswith('B') else 'F.Fab'
    o = ['(footprint "%s"' % name, '\t(version 20260206)', '\t(generator "pcbnew")', '\t(generator_version "10.0")',
         '\t(layer "%s")' % ('B.Cu' if layer.startswith('B') else 'F.Cu'), '\t(descr "%s")' % descr, '\t(tags "logo tinylink")']
    for k, v, ly in (('Reference', 'G***', layer), ('Value', name, fab), ('Datasheet', '', fab), ('Description', '', fab)):
        o += ['\t(property "%s" "%s"' % (k, v), '\t\t(at 0 0 0)', '\t\t(layer "%s")' % ly, '\t\t(hide yes)',
              '\t\t(uuid "%s")' % u(), '\t\t(effects', '\t\t\t(font', '\t\t\t\t(size 1.27 1.27)', '\t\t\t)'] + \
             (['\t\t\t(justify mirror)'] if mirror else []) + ['\t\t)', '\t)']
    o += ['\t(attr board_only exclude_from_pos_files exclude_from_bom)', '\t(duplicate_pad_numbers_are_jumpers no)']
    s = RING_S
    def stroke(w): return ['\t\t(stroke', '\t\t\t(width %g)' % w, '\t\t\t(type default)', '\t\t)', '\t\t(layer "%s")' % layer, '\t\t(uuid "%s")' % u(), '\t)']
    for y in (-RING_R, RING_R):
        o += ['\t(fp_line', '\t\t(start %.6f %.6f)' % (fx(-s), y), '\t\t(end %.6f %.6f)' % (fx(s), y)] + stroke(RING_W)
    for sx in (-1, 1):
        o += ['\t(fp_arc', '\t\t(start %.6f %.6f)' % (fx(sx * s), sx * RING_R), '\t\t(mid %.6f 0)' % fx(sx * (s + RING_R)),
              '\t\t(end %.6f %.6f)' % (fx(sx * s), -sx * RING_R)] + stroke(RING_W)
    for poly in text_polys:
        o += ['\t(fp_poly', '\t\t(pts']
        pts = ['(xy %.6f %.6f)' % (fx(x), y) for x, y in poly]
        o += ['\t\t\t' + ' '.join(pts[i:i + 5]) for i in range(0, len(pts), 5)]
        o += ['\t\t)', '\t\t(stroke', '\t\t\t(width 0)', '\t\t\t(type default)', '\t\t)', '\t\t(fill yes)',
              '\t\t(layer "%s")' % layer, '\t\t(uuid "%s")' % u(), '\t)']
    o += ['\t(embedded_fonts no)', ')', '']
    open(os.path.join(OUT, name + '.kicad_mod'), 'w').write('\n'.join(o))


SIZES = {
    '': dict(SCALE=0.681 / 281, BASELINE=0.623, PILL_R=1.193861, PILL_HALF=5.959410,
             RING_R=1.588466, RING_W=0.2, RING_S=4.80865, TRACK=0, DOT_LIFT=0),
    # 80 %: the i-dot lifted 0.045 mm, 0.03 mm extra tracking and a taller pill keep every
    # silk bridge >= 0.15 mm (JLC minimum)
    '_small': dict(SCALE=0.8 * 0.681 / 281, BASELINE=0.8 * 0.623, PILL_R=1.025, PILL_HALF=4.872,
                   RING_R=1.28, RING_W=0.2, RING_S=3.927, TRACK=0.03, DOT_LIFT=0.045),
}


def min_gaps(holes, outer_half):
    def seg_dist(p, a, b):
        ax, ay = a; bx, by = b; px, py = p
        dx, dy = bx - ax, by - ay
        t = max(0, min(1, ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy or 1)))
        return math.hypot(px - ax - t * dx, py - ay - t * dy)
    best = 9
    for i, h in enumerate(holes):
        for j, g in enumerate(holes):
            if j <= i:
                continue
            for p in h[::2]:
                for k in range(len(g)):
                    best = min(best, seg_dist(p, g[k - 1], g[k]))
    s = outer_half - PILL_R
    edge = 9
    for h in holes:
        for x, y in h:
            cx = max(-s, min(s, x))
            edge = min(edge, PILL_R - math.hypot(x - cx, y))
    return best, edge


os.makedirs(OUT, exist_ok=True)
D = 'tinyLINK logo (KiBuzzard UbuntuMono-B text knocked out of a pill in a stadium outline)'
for suffix, params in SIZES.items():
    globals().update(params)
    holes, half_text = glyph_contours()
    outer = knocked_out_pill([h[::-1] for h in orient_holes(holes)], PILL_HALF)
    ring_half = RING_S + RING_R + RING_W / 2
    d = D + (', small' if suffix else '')
    fp('tinyLINK' + suffix, d, 'F.SilkS', [outer], False)
    fp('tinyLINK' + suffix + '_back', d + ', back side', 'B.SilkS', [outer], True)
    g, e = min_gaps(holes, PILL_HALF)
    print('tinyLINK%s %.2f x %.2f mm; min bridge between letters %.3f mm, letter to pill edge %.3f mm, pill to ring %.3f mm'
          % (suffix, 2 * ring_half, 2 * RING_R + RING_W, g, e, RING_R - RING_W / 2 - PILL_R))
