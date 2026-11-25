#!/usr/bin/env python3
# make_logo.py v1.1.1 - generates the JesusSaves logo set (banner, icons, xpm, ico)
# Original artwork (cross and wave) drawn procedurally (no upstream NetSurf artwork reused).
import math, sys, os
from PIL import Image, ImageDraw, ImageFont, ImageChops

NS = sys.argv[1]            # netsurf source dir
OUT = sys.argv[2]           # preview dir
os.makedirs(OUT, exist_ok=True)
SS = 4                      # supersampling factor
FONT = "/usr/share/fonts/opentype/inter/Inter-ExtraBold.otf"

NAVY = (10, 24, 64)
BLUE = (30, 78, 216)
CYAN = (64, 220, 255)
WHITE = (255, 255, 255)


def vgrad(w, h, top, bot):
    g = Image.new("RGB", (w, h))
    px = g.load()
    for y in range(h):
        t = y / max(1, h - 1)
        c = tuple(int(top[i] + (bot[i] - top[i]) * t) for i in range(3))
        for x in range(w):
            px[x, y] = c
    return g


def mark(size):
    """Square JesusSaves mark: gradient rounded tile, white cross rising from a cyan wave."""
    s = size * SS
    tile = vgrad(s, s, BLUE, NAVY).convert("RGBA")
    mask = Image.new("L", (s, s), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, s - 1, s - 1), radius=int(s * 0.22), fill=255)
    tile.putalpha(mask)
    d = ImageDraw.Draw(tile)
    # cyan wave band behind the foot of the cross (the "surf")
    amp, cy = s * 0.045, s * 0.74
    band = s * 0.11
    pts_top, pts_bot = [], []
    n = 60
    for i in range(n + 1):
        t = i / n
        x = s * 0.12 + t * s * 0.76
        y = cy + amp * math.sin(t * 2 * math.pi)
        pts_top.append((x, y - band * 0.5))
        pts_bot.append((x, y + band * 0.5))
    d.polygon(pts_top + pts_bot[::-1], fill=CYAN)
    # Latin cross: long vertical beam, crossbeam at about one third of the height
    bar = int(s * 0.15)
    r = bar // 2
    cx = s // 2
    d.rounded_rectangle((cx - bar // 2, int(s * 0.14), cx + bar // 2, int(s * 0.80)), radius=r, fill=WHITE)
    d.rounded_rectangle((int(s * 0.27), int(s * 0.31), int(s * 0.73), int(s * 0.31) + bar), radius=r, fill=WHITE)
    return tile.resize((size, size), Image.LANCZOS)


def banner(w=308, h=86):
    s = (w * SS, h * SS)
    bg = vgrad(s[0], s[1], (22, 58, 170), (8, 20, 56)).convert("RGBA")
    # soft wave band along the bottom
    ov = Image.new("RGBA", s, (0, 0, 0, 0))
    od = ImageDraw.Draw(ov)
    for k, (a, alpha) in enumerate(((7, 70), (5, 50), (3, 35))):
        pts = [(x, s[1] * (0.80 + 0.05 * k) + a * SS * math.sin(x / (SS * 22.0) + k)) for x in range(0, s[0] + 1, 4)]
        od.polygon(pts + [(s[0], s[1]), (0, s[1])], fill=(64, 220, 255, alpha))
    bg = Image.alpha_composite(bg, ov)
    m = mark(66).resize((66 * SS, 66 * SS), Image.LANCZOS)
    bg.alpha_composite(m, (10 * SS, 10 * SS))
    d = ImageDraw.Draw(bg)
    # "Jesus" in white + "Saves" in cyan, sized to fit the space right of the mark
    fs = 40
    while True:
        f = ImageFont.truetype(FONT, fs * SS)
        wJ = d.textlength("Jesus", font=f)
        wS = d.textlength("Saves", font=f)
        if wJ + wS <= (w - 96) * SS or fs <= 12:
            break
        fs -= 1
    d.text((88 * SS, 41 * SS), "Jesus", font=f, fill=WHITE, anchor="lm")
    d.text((88 * SS + wJ, 41 * SS), "Saves", font=f, fill=CYAN, anchor="lm")
    f2 = ImageFont.truetype("/usr/share/fonts/opentype/inter/Inter-SemiBold.otf", 9 * SS)
    d.text((90 * SS, 69 * SS), "based on NetSurf", font=f2, fill=(200, 215, 255), anchor="lm")
    return bg.resize((w, h), Image.LANCZOS).convert("RGB")


def write_xpm(img, path, name):
    img = img.convert("RGBA")
    # quantise to <= 250 colours + transparent
    rgb = Image.new("RGB", img.size, (0, 0, 0))
    rgb.paste(img, mask=img.split()[3])
    q = rgb.quantize(colors=250, method=Image.MEDIANCUT, dither=Image.NONE)
    pal = q.getpalette()
    w, h = img.size
    pix = q.load()
    al = img.split()[3].load()
    used = sorted({pix[x, y] for y in range(h) for x in range(w) if al[x, y] >= 128})
    chars = [chr(c) for c in range(35, 127) if chr(c) not in '"\\']
    codes = {}
    n = 0
    for i in used:
        a, b = divmod(n, len(chars))
        codes[i] = chars[a] + chars[b]
        n += 1
    lines = ["/* XPM */", "static char *%s[] = {" % name,
             '"%d %d %d 2",' % (w, h, len(used) + 1), '"  c None",']
    for i in used:
        lines.append('"%s c #%02X%02X%02X",' % (codes[i], pal[3 * i], pal[3 * i + 1], pal[3 * i + 2]))
    for y in range(h):
        row = "".join(codes[pix[x, y]] if al[x, y] >= 128 else "  " for x in range(w))
        lines.append('"%s"%s' % (row, "," if y < h - 1 else ""))
    lines.append("};")
    open(path, "w").write("\n".join(lines) + "\n")


def p(*a):
    return os.path.join(NS, *a)


bn = banner()
bn.save(p("resources", "netsurf.png"))                      # about:logo + welcome banner
bn.save(p("frontends", "windows", "res", "banner.bmp"))
bn.convert("P", palette=Image.ADAPTIVE, colors=128).save(p("frontends", "windows", "res", "netsurf.gif"))
bn.save(os.path.join(OUT, "jesussaves_banner.png"))

icon256 = mark(256)
icon256.save(os.path.join(OUT, "jesussaves_icon_256.png"))
fav = mark(17)
fav.save(p("resources", "favicon.png"))
mark(16).save(os.path.join(OUT, "jesussaves_16.png"))
mark(16).save(p("frontends", "gtk", "res", "default.ico"), format="ICO", sizes=[(16, 16)])
icon256.save(p("frontends", "windows", "res", "NetSurf.ico"), format="ICO",
             sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
write_xpm(mark(128), p("frontends", "gtk", "res", "netsurf.xpm"), "jesussaves")
write_xpm(mark(16), p("frontends", "gtk", "res", "netsurf-16x16.xpm"), "jesussaves16")
print("logo set written")
