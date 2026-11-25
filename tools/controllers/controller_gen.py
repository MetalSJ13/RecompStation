"""Controller drawings for RecompStation (200 x 130 box).
Writes the Xbox and PlayStation layouts as JSON (for qml/ControllerShapes.js)
and as SVG (for design-canvas mockups), so both come from the same geometry.

    python3 tools/controllers/controller_gen.py json
"""
import json, math

def circle(cx, cy, r):
    return f"M {cx-r:.2f} {cy:.2f} a {r} {r} 0 1 0 {2*r} 0 a {r} {r} 0 1 0 {-2*r} 0 Z"

def rrect(x, y, w, h, r):
    return (f"M {x+r} {y} L {x+w-r} {y} Q {x+w} {y} {x+w} {y+r} L {x+w} {y+h-r} Q {x+w} {y+h} {x+w-r} {y+h} "
            f"L {x+r} {y+h} Q {x} {y+h} {x} {y+h-r} L {x} {y+r} Q {x} {y} {x+r} {y} Z")

def cross(cx, cy, arm, half, r=1.6):
    # Rounded d-pad cross: arm = length from centre, half = half width.
    a, h = arm, half
    pts = [(-h,-a),(h,-a),(h,-h),(a,-h),(a,h),(h,h),(h,a),(-h,a),(-h,h),(-a,h),(-a,-h),(-h,-h)]
    # Round only the outer corners (indices 0,1,3,4,6,7,9,10).
    d = []
    n = len(pts)
    for i,(x,y) in enumerate(pts):
        X, Y = cx+x, cy+y
        outer = i in (0,1,3,4,6,7,9,10)
        if not outer:
            d.append(("L", X, Y)); continue
        px, py = pts[i-1]; nx, ny = pts[(i+1)%n]
        def toward(ax, ay, bx, by, t):
            L = math.hypot(bx-ax, by-ay); return ax+(bx-ax)*t/L, ay+(by-ay)*t/L
        sx, sy = toward(x, y, px, py, r); ex, ey = toward(x, y, nx, ny, r)
        d.append(("L", cx+sx, cy+sy)); d.append(("Q", X, Y, cx+ex, cy+ey))
    out = ""
    for k, item in enumerate(d):
        if item[0] == "L":
            out += ("M" if k == 0 else " L") + f" {item[1]:.2f} {item[2]:.2f}"
        else:
            out += f" Q {item[1]:.2f} {item[2]:.2f} {item[3]:.2f} {item[4]:.2f}"
    return out + " Z"

def mirrored(segments):
    """segments: right-half cubic path from top centre (100, y0) clockwise to
    bottom centre; returns the full symmetric outline."""
    start, curves = segments
    d = f"M {start[0]} {start[1]}"
    for c in curves:
        d += " C " + " ".join(f"{x} {y}" for x, y in c)
    # Left half: the same curves reversed and mirrored.
    pts = [start] + [p for c in curves for p in c]
    rev = list(reversed(pts))
    for i in range(0, len(curves)):
        c1, c2, end = rev[3*i+1], rev[3*i+2], rev[3*i+3]
        d += " C " + " ".join(f"{200-x} {y}" for x, y in (c1, c2, end))
    return d + " Z"

def stick_tokens(side):
    return [f"{side}stick-click", f"{side}stick", f"{side}stick-up", f"{side}stick-down",
            f"{side}stick-left", f"{side}stick-right"]

def stick(cx, cy, r, side):
    return [
        {"path": circle(cx, cy, r), "role": "well", "tokens": []},
        {"path": circle(cx, cy, r*0.68), "role": "part", "tokens": stick_tokens(side)},
        {"path": circle(cx, cy, r*0.38), "role": "trim", "tokens": []},
    ]

def xbox():
    body = mirrored(((100, 24), [
        ((118, 24), (136, 19), (152, 20)),
        ((168, 21), (181, 30), (187, 45)),
        ((193, 61), (198, 83), (199, 100)),
        ((200, 116), (193, 127), (181, 127)),
        ((170, 127), (163, 119), (156, 109)),
        ((149, 99), (141, 90), (128, 88)),
        ((118, 87), (108, 88), (100, 88)),
    ]))
    els = [{"path": body, "role": "body", "tokens": []}]
    els += stick(56, 52, 13, "left")
    els += [{"path": cross(78, 76, 10, 3.6), "role": "part", "tokens": ["dpad-up", "dpad-down", "dpad-left", "dpad-right"]}]
    els += stick(122, 76, 12, "right")
    els += [
        {"path": circle(100, 37, 6.5), "role": "part", "tokens": ["guide"]},
        {"path": rrect(83, 50, 7, 5, 2.4), "role": "part", "tokens": ["back"]},
        {"path": rrect(110, 50, 7, 5, 2.4), "role": "part", "tokens": ["start"]},
    ]
    labels = []
    for (cx, cy, t) in ((146, 42, "y"), (156, 52, "b"), (146, 62, "a"), (136, 52, "x")):
        els.append({"path": circle(cx, cy, 5.4), "role": "accent", "tokens": [t]})
        labels.append({"x": cx, "y": cy, "text": t.upper(), "tokens": [t]})
    return {"elements": els, "labels": labels}

def playstation():
    body = mirrored(((100, 22), [
        ((120, 22), (138, 17), (154, 19)),
        ((172, 21), (184, 32), (190, 49)),
        ((196, 67), (199, 89), (198, 105)),
        ((197, 121), (189, 129), (178, 128)),
        ((167, 127), (160, 118), (154, 108)),
        ((148, 98), (141, 92), (129, 91)),
        ((118, 90), (108, 91), (100, 91)),
    ]))
    els = [{"path": body, "role": "body", "tokens": []}]
    els += [{"path": "M 76 25 C 90 23 110 23 124 25 L 126 47 C 117 51 83 51 74 47 Z", "role": "well", "tokens": []}]
    # D-pad: four separate arrows.
    cx, cy = 55, 56
    arrows = {
        "dpad-up":    f"M {cx-3.4} {cy-12} L {cx+3.4} {cy-12} L {cx+3.4} {cy-6} L {cx} {cy-2.6} L {cx-3.4} {cy-6} Z",
        "dpad-down":  f"M {cx-3.4} {cy+12} L {cx+3.4} {cy+12} L {cx+3.4} {cy+6} L {cx} {cy+2.6} L {cx-3.4} {cy+6} Z",
        "dpad-left":  f"M {cx-12} {cy-3.4} L {cx-12} {cy+3.4} L {cx-6} {cy+3.4} L {cx-2.6} {cy} L {cx-6} {cy-3.4} Z",
        "dpad-right": f"M {cx+12} {cy-3.4} L {cx+12} {cy+3.4} L {cx+6} {cy+3.4} L {cx+2.6} {cy} L {cx+6} {cy-3.4} Z",
    }
    for token, path in arrows.items():
        els.append({"path": path, "role": "part", "tokens": [token]})
    els += stick(79, 81, 11.5, "left")
    els += stick(121, 81, 11.5, "right")
    els += [
        {"path": circle(100, 66, 3.6), "role": "part", "tokens": ["guide"]},
        {"path": rrect(68, 29, 3.6, 7, 1.8), "role": "part", "tokens": ["back"]},
        {"path": rrect(128.4, 29, 3.6, 7, 1.8), "role": "part", "tokens": ["start"]},
    ]
    face = (("y", 145, 45), ("b", 155, 56), ("a", 145, 67), ("x", 135, 56))
    for t, fx, fy in face:
        els.append({"path": circle(fx, fy, 5.6), "role": "accent", "tokens": [t]})
    # Symbols: triangle, circle, cross, square.
    s = 2.6
    glyphs = {
        "y": f"M {145} {45-s} L {145+s*0.95} {45+s*0.7} L {145-s*0.95} {45+s*0.7} Z",
        "b": circle(155, 56, s*0.95),
        "a": f"M {145-s*0.8} {67-s*0.8} L {145+s*0.8} {67+s*0.8} M {145+s*0.8} {67-s*0.8} L {145-s*0.8} {67+s*0.8}",
        "x": rrect(135-s*0.8, 56-s*0.8, s*1.6, s*1.6, 0.2),
    }
    for t, path in glyphs.items():
        els.append({"path": path, "role": "glyph", "tokens": [t]})
    return {"elements": els, "labels": []}

LAYOUTS = {"xbox": xbox(), "playstation": playstation()}

THEME = {"line": "#b5bbcd", "accent": "#8174e8", "body": "#222635", "well": "#171a24", "part": "#2b3042", "lit": "#8174e8", "text": "#d3d7e4"}

def svg(kind, width, lit=(), theme=THEME, line=1.4):
    layout = LAYOUTS[kind]
    out = [f'<svg viewBox="0 16 200 116" width="{width}" xmlns="http://www.w3.org/2000/svg" style="display:block">']
    for e in layout["elements"]:
        on = any(t in lit for t in e["tokens"])
        role = e["role"]
        if role == "body":
            fill, stroke, sw = theme["body"], theme["line"], line
        elif role == "well":
            fill, stroke, sw = theme["well"], theme["line"], line*0.6
        elif role == "trim":
            fill, stroke, sw = "none", theme["line"], line*0.45
        elif role == "glyph":
            fill, stroke, sw = "none", (theme["well"] if on else theme["text"]), line*0.7
        elif role == "accent":
            fill, stroke, sw = (theme["lit"] if on else theme["part"]), (theme["lit"] if on else theme["accent"]), line*0.8
        else:
            fill, stroke, sw = (theme["lit"] if on else theme["part"]), (theme["lit"] if on else theme["line"]), line*0.7
        out.append(f'<path d="{e["path"]}" fill="{fill}" stroke="{stroke}" stroke-width="{sw:.2f}" stroke-linejoin="round" stroke-linecap="round"/>')
    for l in layout["labels"]:
        on = any(t in lit for t in l["tokens"])
        out.append(f'<text x="{l["x"]}" y="{l["y"]+2.2}" text-anchor="middle" font-family="Inter,sans-serif" font-size="6.2" font-weight="700" fill="{theme["well"] if on else theme["text"]}">{l["text"]}</text>')
    out.append("</svg>")
    return "".join(out)

if __name__ == "__main__":
    import sys
    if sys.argv[1] == "json":
        print(json.dumps(LAYOUTS, indent=1))
