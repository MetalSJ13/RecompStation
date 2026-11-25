.pragma library

// Controller drawings for ControllerOutline.qml, in a 200 x 130 box.
// Xbox and PlayStation come from tools/controller_gen.py (the approved
// design-canvas drawings, 2026-11-25); Nintendo and generic are the earlier
// line drawings. Each element is an SVG path, a role and the controller
// input tokens that light it up in the button test. Roles:
//   body    the controller's shell, filled
//   well    a recess (stick wells, the touchpad), filled darker
//   part    a button or stick cap
//   accent  a face button (ringed in the accent color)
//   glyph   a face-button symbol (PlayStation)
//   trim    a thin detail line
// labels: letters drawn on face buttons (Xbox). No logos or brand marks.

var layouts = {
 "xbox": [
  {
   "path": "M 100 24 C 118 24 136 19 152 20 C 168 21 181 30 187 45 C 193 61 198 83 199 100 C 200 116 193 127 181 127 C 170 127 163 119 156 109 C 149 99 141 90 128 88 C 118 87 108 88 100 88 C 92 88 82 87 72 88 C 59 90 51 99 44 109 C 37 119 30 127 19 127 C 7 127 0 116 1 100 C 2 83 7 61 13 45 C 19 30 32 21 48 20 C 64 19 82 24 100 24 Z",
   "role": "body",
   "tokens": []
  },
  {
   "path": "M 43.00 52.00 a 13 13 0 1 0 26 0 a 13 13 0 1 0 -26 0 Z",
   "role": "well",
   "tokens": []
  },
  {
   "path": "M 47.16 52.00 a 8.84 8.84 0 1 0 17.68 0 a 8.84 8.84 0 1 0 -17.68 0 Z",
   "role": "part",
   "tokens": [
    "leftstick-click",
    "leftstick",
    "leftstick-up",
    "leftstick-down",
    "leftstick-left",
    "leftstick-right"
   ]
  },
  {
   "path": "M 51.06 52.00 a 4.94 4.94 0 1 0 9.88 0 a 4.94 4.94 0 1 0 -9.88 0 Z",
   "role": "trim",
   "tokens": []
  },
  {
   "path": "M 74.40 67.60 Q 74.40 66.00 76.00 66.00 L 80.00 66.00 Q 81.60 66.00 81.60 67.60 L 81.60 72.40 L 86.40 72.40 Q 88.00 72.40 88.00 74.00 L 88.00 78.00 Q 88.00 79.60 86.40 79.60 L 81.60 79.60 L 81.60 84.40 Q 81.60 86.00 80.00 86.00 L 76.00 86.00 Q 74.40 86.00 74.40 84.40 L 74.40 79.60 L 69.60 79.60 Q 68.00 79.60 68.00 78.00 L 68.00 74.00 Q 68.00 72.40 69.60 72.40 L 74.40 72.40 Z",
   "role": "part",
   "tokens": [
    "dpad-up",
    "dpad-down",
    "dpad-left",
    "dpad-right"
   ]
  },
  {
   "path": "M 110.00 76.00 a 12 12 0 1 0 24 0 a 12 12 0 1 0 -24 0 Z",
   "role": "well",
   "tokens": []
  },
  {
   "path": "M 113.84 76.00 a 8.16 8.16 0 1 0 16.32 0 a 8.16 8.16 0 1 0 -16.32 0 Z",
   "role": "part",
   "tokens": [
    "rightstick-click",
    "rightstick",
    "rightstick-up",
    "rightstick-down",
    "rightstick-left",
    "rightstick-right"
   ]
  },
  {
   "path": "M 117.44 76.00 a 4.5600000000000005 4.5600000000000005 0 1 0 9.120000000000001 0 a 4.5600000000000005 4.5600000000000005 0 1 0 -9.120000000000001 0 Z",
   "role": "trim",
   "tokens": []
  },
  {
   "path": "M 93.50 37.00 a 6.5 6.5 0 1 0 13.0 0 a 6.5 6.5 0 1 0 -13.0 0 Z",
   "role": "part",
   "tokens": [
    "guide"
   ]
  },
  {
   "path": "M 85.4 50 L 87.6 50 Q 90 50 90 52.4 L 90 52.6 Q 90 55 87.6 55 L 85.4 55 Q 83 55 83 52.6 L 83 52.4 Q 83 50 85.4 50 Z",
   "role": "part",
   "tokens": [
    "back"
   ]
  },
  {
   "path": "M 112.4 50 L 114.6 50 Q 117 50 117 52.4 L 117 52.6 Q 117 55 114.6 55 L 112.4 55 Q 110 55 110 52.6 L 110 52.4 Q 110 50 112.4 50 Z",
   "role": "part",
   "tokens": [
    "start"
   ]
  },
  {
   "path": "M 140.60 42.00 a 5.4 5.4 0 1 0 10.8 0 a 5.4 5.4 0 1 0 -10.8 0 Z",
   "role": "accent",
   "tokens": [
    "y"
   ]
  },
  {
   "path": "M 150.60 52.00 a 5.4 5.4 0 1 0 10.8 0 a 5.4 5.4 0 1 0 -10.8 0 Z",
   "role": "accent",
   "tokens": [
    "b"
   ]
  },
  {
   "path": "M 140.60 62.00 a 5.4 5.4 0 1 0 10.8 0 a 5.4 5.4 0 1 0 -10.8 0 Z",
   "role": "accent",
   "tokens": [
    "a"
   ]
  },
  {
   "path": "M 130.60 52.00 a 5.4 5.4 0 1 0 10.8 0 a 5.4 5.4 0 1 0 -10.8 0 Z",
   "role": "accent",
   "tokens": [
    "x"
   ]
  }
 ],
 "playstation": [
  {
   "path": "M 100 22 C 120 22 138 17 154 19 C 172 21 184 32 190 49 C 196 67 199 89 198 105 C 197 121 189 129 178 128 C 167 127 160 118 154 108 C 148 98 141 92 129 91 C 118 90 108 91 100 91 C 92 91 82 90 71 91 C 59 92 52 98 46 108 C 40 118 33 127 22 128 C 11 129 3 121 2 105 C 1 89 4 67 10 49 C 16 32 28 21 46 19 C 62 17 80 22 100 22 Z",
   "role": "body",
   "tokens": []
  },
  {
   "path": "M 76 25 C 90 23 110 23 124 25 L 126 47 C 117 51 83 51 74 47 Z",
   "role": "well",
   "tokens": []
  },
  {
   "path": "M 51.6 44 L 58.4 44 L 58.4 50 L 55 53.4 L 51.6 50 Z",
   "role": "part",
   "tokens": [
    "dpad-up"
   ]
  },
  {
   "path": "M 51.6 68 L 58.4 68 L 58.4 62 L 55 58.6 L 51.6 62 Z",
   "role": "part",
   "tokens": [
    "dpad-down"
   ]
  },
  {
   "path": "M 43 52.6 L 43 59.4 L 49 59.4 L 52.4 56 L 49 52.6 Z",
   "role": "part",
   "tokens": [
    "dpad-left"
   ]
  },
  {
   "path": "M 67 52.6 L 67 59.4 L 61 59.4 L 57.6 56 L 61 52.6 Z",
   "role": "part",
   "tokens": [
    "dpad-right"
   ]
  },
  {
   "path": "M 67.50 81.00 a 11.5 11.5 0 1 0 23.0 0 a 11.5 11.5 0 1 0 -23.0 0 Z",
   "role": "well",
   "tokens": []
  },
  {
   "path": "M 71.18 81.00 a 7.82 7.82 0 1 0 15.64 0 a 7.82 7.82 0 1 0 -15.64 0 Z",
   "role": "part",
   "tokens": [
    "leftstick-click",
    "leftstick",
    "leftstick-up",
    "leftstick-down",
    "leftstick-left",
    "leftstick-right"
   ]
  },
  {
   "path": "M 74.63 81.00 a 4.37 4.37 0 1 0 8.74 0 a 4.37 4.37 0 1 0 -8.74 0 Z",
   "role": "trim",
   "tokens": []
  },
  {
   "path": "M 109.50 81.00 a 11.5 11.5 0 1 0 23.0 0 a 11.5 11.5 0 1 0 -23.0 0 Z",
   "role": "well",
   "tokens": []
  },
  {
   "path": "M 113.18 81.00 a 7.82 7.82 0 1 0 15.64 0 a 7.82 7.82 0 1 0 -15.64 0 Z",
   "role": "part",
   "tokens": [
    "rightstick-click",
    "rightstick",
    "rightstick-up",
    "rightstick-down",
    "rightstick-left",
    "rightstick-right"
   ]
  },
  {
   "path": "M 116.63 81.00 a 4.37 4.37 0 1 0 8.74 0 a 4.37 4.37 0 1 0 -8.74 0 Z",
   "role": "trim",
   "tokens": []
  },
  {
   "path": "M 96.40 66.00 a 3.6 3.6 0 1 0 7.2 0 a 3.6 3.6 0 1 0 -7.2 0 Z",
   "role": "part",
   "tokens": [
    "guide"
   ]
  },
  {
   "path": "M 69.8 29 L 69.8 29 Q 71.6 29 71.6 30.8 L 71.6 34.2 Q 71.6 36 69.8 36 L 69.8 36 Q 68 36 68 34.2 L 68 30.8 Q 68 29 69.8 29 Z",
   "role": "part",
   "tokens": [
    "back"
   ]
  },
  {
   "path": "M 130.20000000000002 29 L 130.2 29 Q 132.0 29 132.0 30.8 L 132.0 34.2 Q 132.0 36 130.2 36 L 130.20000000000002 36 Q 128.4 36 128.4 34.2 L 128.4 30.8 Q 128.4 29 130.20000000000002 29 Z",
   "role": "part",
   "tokens": [
    "start"
   ]
  },
  {
   "path": "M 139.40 45.00 a 5.6 5.6 0 1 0 11.2 0 a 5.6 5.6 0 1 0 -11.2 0 Z",
   "role": "accent",
   "tokens": [
    "y"
   ]
  },
  {
   "path": "M 149.40 56.00 a 5.6 5.6 0 1 0 11.2 0 a 5.6 5.6 0 1 0 -11.2 0 Z",
   "role": "accent",
   "tokens": [
    "b"
   ]
  },
  {
   "path": "M 139.40 67.00 a 5.6 5.6 0 1 0 11.2 0 a 5.6 5.6 0 1 0 -11.2 0 Z",
   "role": "accent",
   "tokens": [
    "a"
   ]
  },
  {
   "path": "M 129.40 56.00 a 5.6 5.6 0 1 0 11.2 0 a 5.6 5.6 0 1 0 -11.2 0 Z",
   "role": "accent",
   "tokens": [
    "x"
   ]
  },
  {
   "path": "M 145 42.4 L 147.47 46.82 L 142.53 46.82 Z",
   "role": "glyph",
   "tokens": [
    "y"
   ]
  },
  {
   "path": "M 152.53 56.00 a 2.4699999999999998 2.4699999999999998 0 1 0 4.9399999999999995 0 a 2.4699999999999998 2.4699999999999998 0 1 0 -4.9399999999999995 0 Z",
   "role": "glyph",
   "tokens": [
    "b"
   ]
  },
  {
   "path": "M 142.92 64.92 L 147.08 69.08 M 147.08 64.92 L 142.92 69.08",
   "role": "glyph",
   "tokens": [
    "a"
   ]
  },
  {
   "path": "M 133.11999999999998 53.92 L 136.88 53.92 Q 137.07999999999998 53.92 137.07999999999998 54.120000000000005 L 137.07999999999998 57.879999999999995 Q 137.07999999999998 58.08 136.88 58.08 L 133.11999999999998 58.08 Q 132.92 58.08 132.92 57.879999999999995 L 132.92 54.120000000000005 Q 132.92 53.92 133.11999999999998 53.92 Z",
   "role": "glyph",
   "tokens": [
    "x"
   ]
  }
 ],
 "nintendo": [
  {
   "path": "M 44 20 C 60 13 80 16 100 19 C 120 16 140 13 156 20 C 176 28 189 60 195 94 C 199 117 186 128 172 122 C 160 116 151 101 141 93 C 129 87 71 87 59 93 C 49 101 40 116 28 122 C 14 128 1 117 5 94 C 11 60 24 28 44 20 Z",
   "role": "body",
   "tokens": []
  },
  {
   "path": "M 48 50 a 12 12 0 1 0 24 0 a 12 12 0 1 0 -24 0",
   "role": "part",
   "tokens": [
    "leftstick-click",
    "leftstick",
    "leftstick-up",
    "leftstick-down",
    "leftstick-left",
    "leftstick-right"
   ]
  },
  {
   "path": "M 75.5 61 L 84.5 61 L 84.5 67.5 L 91 67.5 L 91 76.5 L 84.5 76.5 L 84.5 83 L 75.5 83 L 75.5 76.5 L 69 76.5 L 69 67.5 L 75.5 67.5 Z",
   "role": "part",
   "tokens": [
    "dpad-up",
    "dpad-down",
    "dpad-left",
    "dpad-right"
   ]
  },
  {
   "path": "M 115 72 a 11 11 0 1 0 22 0 a 11 11 0 1 0 -22 0",
   "role": "part",
   "tokens": [
    "rightstick-click",
    "rightstick",
    "rightstick-up",
    "rightstick-down",
    "rightstick-left",
    "rightstick-right"
   ]
  },
  {
   "path": "M 118 36 L 124 36 M 121 33 L 121 39",
   "role": "part",
   "tokens": [
    "start"
   ]
  },
  {
   "path": "M 76 36 L 82 36",
   "role": "part",
   "tokens": [
    "back"
   ]
  },
  {
   "path": "M 105 50 a 3 3 0 1 0 6 0 a 3 3 0 1 0 -6 0",
   "role": "part",
   "tokens": [
    "guide"
   ]
  },
  {
   "path": "M 143 40 a 5 5 0 1 0 10 0 a 5 5 0 1 0 -10 0",
   "role": "accent",
   "tokens": [
    "y"
   ]
  },
  {
   "path": "M 153 50 a 5 5 0 1 0 10 0 a 5 5 0 1 0 -10 0",
   "role": "accent",
   "tokens": [
    "b"
   ]
  },
  {
   "path": "M 143 60 a 5 5 0 1 0 10 0 a 5 5 0 1 0 -10 0",
   "role": "accent",
   "tokens": [
    "a"
   ]
  },
  {
   "path": "M 133 50 a 5 5 0 1 0 10 0 a 5 5 0 1 0 -10 0",
   "role": "accent",
   "tokens": [
    "x"
   ]
  }
 ],
 "generic": [
  {
   "path": "M 44 20 C 60 13 80 16 100 19 C 120 16 140 13 156 20 C 176 28 189 60 195 94 C 199 117 186 128 172 122 C 160 116 151 101 141 93 C 129 87 71 87 59 93 C 49 101 40 116 28 122 C 14 128 1 117 5 94 C 11 60 24 28 44 20 Z",
   "role": "body",
   "tokens": []
  },
  {
   "path": "M 48 50 a 12 12 0 1 0 24 0 a 12 12 0 1 0 -24 0",
   "role": "part",
   "tokens": [
    "leftstick-click",
    "leftstick",
    "leftstick-up",
    "leftstick-down",
    "leftstick-left",
    "leftstick-right"
   ]
  },
  {
   "path": "M 75.5 61 L 84.5 61 L 84.5 67.5 L 91 67.5 L 91 76.5 L 84.5 76.5 L 84.5 83 L 75.5 83 L 75.5 76.5 L 69 76.5 L 69 67.5 L 75.5 67.5 Z",
   "role": "part",
   "tokens": [
    "dpad-up",
    "dpad-down",
    "dpad-left",
    "dpad-right"
   ]
  },
  {
   "path": "M 115 72 a 11 11 0 1 0 22 0 a 11 11 0 1 0 -22 0",
   "role": "part",
   "tokens": [
    "rightstick-click",
    "rightstick",
    "rightstick-up",
    "rightstick-down",
    "rightstick-left",
    "rightstick-right"
   ]
  },
  {
   "path": "M 87 40 a 3 3 0 1 0 6 0 a 3 3 0 1 0 -6 0",
   "role": "part",
   "tokens": [
    "back"
   ]
  },
  {
   "path": "M 107 40 a 3 3 0 1 0 6 0 a 3 3 0 1 0 -6 0",
   "role": "part",
   "tokens": [
    "start"
   ]
  },
  {
   "path": "M 143 40 a 5 5 0 1 0 10 0 a 5 5 0 1 0 -10 0",
   "role": "accent",
   "tokens": [
    "y"
   ]
  },
  {
   "path": "M 153 50 a 5 5 0 1 0 10 0 a 5 5 0 1 0 -10 0",
   "role": "accent",
   "tokens": [
    "b"
   ]
  },
  {
   "path": "M 143 60 a 5 5 0 1 0 10 0 a 5 5 0 1 0 -10 0",
   "role": "accent",
   "tokens": [
    "a"
   ]
  },
  {
   "path": "M 133 50 a 5 5 0 1 0 10 0 a 5 5 0 1 0 -10 0",
   "role": "accent",
   "tokens": [
    "x"
   ]
  }
 ]
};

var labelSets = {
 "xbox": [
  {
   "x": 146,
   "y": 42,
   "text": "Y",
   "tokens": [
    "y"
   ]
  },
  {
   "x": 156,
   "y": 52,
   "text": "B",
   "tokens": [
    "b"
   ]
  },
  {
   "x": 146,
   "y": 62,
   "text": "A",
   "tokens": [
    "a"
   ]
  },
  {
   "x": 136,
   "y": 52,
   "text": "X",
   "tokens": [
    "x"
   ]
  }
 ],
 "playstation": []
};

function elements(kind) {
    return layouts[kind] || layouts["generic"];
}

function labels(kind) {
    return labelSets[kind] || [];
}
