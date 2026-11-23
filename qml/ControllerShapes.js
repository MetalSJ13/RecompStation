.pragma library

// Controller outline geometry for ControllerOutline.qml, in a 200 x 130 box.
// Generated from the design-canvas drawings (Controller outlines board).
// Each element is an SVG path, a role ("body", "part" or "accent": face
// buttons) and the controller input tokens that light it up in the button
// test. Line drawings only: no logos or brand marks.

var layouts = {
 "xbox": [
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
   "path": "M 93 32 a 7 7 0 1 0 14 0 a 7 7 0 1 0 -14 0",
   "role": "part",
   "tokens": [
    "guide"
   ]
  },
  {
   "path": "M 83 50 a 3 3 0 1 0 6 0 a 3 3 0 1 0 -6 0",
   "role": "part",
   "tokens": [
    "back"
   ]
  },
  {
   "path": "M 111 50 a 3 3 0 1 0 6 0 a 3 3 0 1 0 -6 0",
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
 ],
 "playstation": [
  {
   "path": "M 40 22 C 60 14 80 18 100 18 C 120 18 140 14 160 22 C 180 30 191 62 196 96 C 199 118 187 128 173 122 C 162 117 152 103 143 95 C 131 89 69 89 57 95 C 48 103 38 117 27 122 C 13 128 1 118 4 96 C 9 62 20 30 40 22 Z",
   "role": "body",
   "tokens": []
  },
  {
   "path": "M 51.5 40 L 60.5 40 L 60.5 47.5 L 68 47.5 L 68 56.5 L 60.5 56.5 L 60.5 64 L 51.5 64 L 51.5 56.5 L 44 56.5 L 44 47.5 L 51.5 47.5 Z",
   "role": "part",
   "tokens": [
    "dpad-up",
    "dpad-down",
    "dpad-left",
    "dpad-right"
   ]
  },
  {
   "path": "M 69 74 a 11 11 0 1 0 22 0 a 11 11 0 1 0 -22 0",
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
   "path": "M 109 74 a 11 11 0 1 0 22 0 a 11 11 0 1 0 -22 0",
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
   "path": "M 84 24 L 116 24 Q 122 24 122 30 L 122 44 Q 122 50 116 50 L 84 50 Q 78 50 78 44 L 78 30 Q 78 24 84 24 Z",
   "role": "part",
   "tokens": []
  },
  {
   "path": "M 63.8 34 a 2.2 2.2 0 1 0 4.4 0 a 2.2 2.2 0 1 0 -4.4 0",
   "role": "part",
   "tokens": [
    "back"
   ]
  },
  {
   "path": "M 131.8 34 a 2.2 2.2 0 1 0 4.4 0 a 2.2 2.2 0 1 0 -4.4 0",
   "role": "part",
   "tokens": [
    "start"
   ]
  },
  {
   "path": "M 144 34.5 L 148.5 42 L 139.5 42 Z",
   "role": "accent",
   "tokens": [
    "y"
   ]
  },
  {
   "path": "M 140.5 58.5 L 147.5 65.5 M 147.5 58.5 L 140.5 65.5",
   "role": "accent",
   "tokens": [
    "a"
   ]
  },
  {
   "path": "M 128.5 48.5 L 135.5 48.5 L 135.5 55.5 L 128.5 55.5 Z",
   "role": "accent",
   "tokens": [
    "x"
   ]
  },
  {
   "path": "M 152.4 52 a 3.6 3.6 0 1 0 7.2 0 a 3.6 3.6 0 1 0 -7.2 0",
   "role": "accent",
   "tokens": [
    "b"
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

function elements(kind) {
    return layouts[kind] || layouts["generic"];
}
