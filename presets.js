/* Ready-made scenes. Coordinates assume an 800x600 window with (0,0) in the centre. */
(function (root) {
    "use strict";
    const K = "#000000", RED = "#e01b24", GRN = "#26a269", BLU = "#1c71d8", YEL = "#f6d32d",
        ORG = "#ff7800", MAG = "#c061cb", BRN = "#865e3c", TEAL = "#2190a4";

    const line = (x1, y1, x2, y2, algo, color, extra) => Object.assign({ type: "line", x1, y1, x2, y2, algo, color, thick: 1, style: "solid" }, extra);
    const circle = (cx, cy, r, color, extra) => Object.assign({ type: "circle", cx, cy, r, color, thick: 1, style: "solid" }, extra);
    const rect = (x1, y1, x2, y2, color, extra) => Object.assign({ type: "rect", x1, y1, x2, y2, algo: "bres", color, thick: 1, style: "solid" }, extra);
    const tri = (x1, y1, x2, y2, x3, y3, color, extra) => Object.assign({ type: "triangle", x1, y1, x2, y2, x3, y3, algo: "bres", color, thick: 1, style: "solid" }, extra);
    const fill = (x, y, fillAlgo, fillColor, borderColor) => ({ type: "fill", x, y, fillAlgo, fillColor, borderColor: borderColor || K });
    const tf = (ref, color, ops) => ({ type: "transform", ref, color, ops });

    const PRESETS = [
        {
            name: "Line algorithms: DDA vs Symmetric DDA vs Bresenham",
            items: [
                line(-300, 200, 300, 260, "dda", RED),
                line(-300, 120, 300, 180, "sdda", GRN),
                line(-300, 40, 300, 100, "bres", BLU),
                line(-200, -250, -50, 200, "dda", RED),
                line(-100, -250, 50, 200, "sdda", GRN),
                line(0, -250, 150, 200, "bres", BLU),
            ],
        },
        {
            name: "Line styles and thickness",
            items: [
                line(-300, 200, 300, 200, "bres", K, { style: "solid", thick: 1 }),
                line(-300, 130, 300, 130, "bres", RED, { style: "dotted", thick: 2 }),
                line(-300, 60, 300, 60, "bres", BLU, { style: "dashed", thick: 3 }),
                line(-300, -10, 300, -10, "dda", GRN, { style: "dashdot", thick: 3 }),
                line(-300, -80, 300, -80, "dda", MAG, { style: "longdash", thick: 5 }),
                circle(0, -190, 80, ORG, { style: "dashed", thick: 2 }),
            ],
        },
        {
            name: "Circle, ellipse and arc (midpoint)",
            items: [
                circle(-200, 0, 120, BLU, { thick: 2 }),
                { type: "ellipse", cx: 170, cy: 0, rx: 180, ry: 90, color: RED, thick: 2, style: "solid" },
                { type: "arc", cx: 0, cy: -150, r: 100, a0: 0, a1: 180, color: GRN, thick: 3, style: "solid" },
            ],
        },
        {
            name: "Concentric circles",
            items: [
                circle(0, 0, 50, RED, { thick: 2 }), circle(0, 0, 100, ORG, { thick: 2 }),
                circle(0, 0, 150, GRN, { thick: 2 }), circle(0, 0, 200, BLU, { thick: 2 }),
                circle(0, 0, 250, MAG, { thick: 2 }),
            ],
        },
        {
            name: "Olympic rings",
            items: [
                circle(-170, 50, 75, BLU, { thick: 5 }), circle(0, 50, 75, K, { thick: 5 }),
                circle(170, 50, 75, RED, { thick: 5 }), circle(-85, -30, 75, YEL, { thick: 5 }),
                circle(85, -30, 75, GRN, { thick: 5 }),
            ],
        },
        {
            name: "Hut / house (with boundary fills)",
            items: [
                rect(-150, -200, 150, 0, K),                       // 1 walls
                tri(-180, 0, 180, 0, 0, 150, K),                   // 2 roof
                rect(-40, -200, 40, -80, K),                       // 3 door
                rect(70, -110, 120, -60, K),                       // 4 window
                fill(0, 60, "b4", RED),                            // roof
                fill(-100, -100, "b4", YEL),                       // walls
                fill(0, -150, "b4", BRN),                          // door
                fill(95, -85, "b4", "#99c1f1"),                    // window
            ],
        },
        {
            name: "Smiley face",
            items: [
                circle(0, 0, 160, K, { thick: 2 }),
                circle(-60, 50, 22, K, { thick: 2 }), circle(60, 50, 22, K, { thick: 2 }),
                { type: "arc", cx: 0, cy: 10, r: 95, a0: 200, a1: 340, color: K, thick: 3, style: "solid" },
                fill(-60, 50, "b4", BLU), fill(60, 50, "b4", BLU),
                fill(110, -60, "b4", YEL),
            ],
        },
        {
            name: "Star in a circle (scan-line fill)",
            items: [
                circle(0, 0, 150, BLU, { thick: 2 }),
                { type: "polygon", pts: "0 150, -88 -121, 143 46, -143 46, 88 -121", algo: "bres", color: K, thick: 1, style: "solid" },
                { type: "scanfill", ref: 2, fillColor: YEL },
            ],
        },
        {
            name: "Fill algorithms side by side",
            items: [
                tri(-370, -80, -230, -80, -300, 80, K),                                   // 1
                rect(-170, -80, -30, 80, K),                                              // 2
                { type: "polygon", pts: "100 80, 170 0, 100 -80, 30 0", algo: "bres", color: K, thick: 1, style: "solid" }, // 3 diamond
                { type: "regpoly", cx: 300, cy: 0, r: 80, n: 5, rot: 90, algo: "bres", color: K, thick: 1, style: "solid" }, // 4 pentagon
                fill(-300, -40, "b4", RED),
                fill(-100, 0, "b8", GRN),
                fill(100, 0, "f4", BLU),
                { type: "scanfill", ref: 4, fillColor: ORG },
            ],
        },
        {
            name: "Transformations of a triangle",
            items: [
                tri(0, 0, 100, 0, 50, 80, K, { thick: 2 }),
                tf(1, RED, [{ type: "translate", tx: 150, ty: 60 }]),
                tf(1, BLU, [{ type: "rotate", deg: 90, px: 0, py: 0 }]),
                tf(1, GRN, [{ type: "scale", sx: 1.5, sy: 1.5, px: 0, py: 0 }]),
                tf(1, MAG, [{ type: "reflect", axis: "x" }]),
                tf(1, ORG, [{ type: "shear", shx: 1, shy: 0, px: 0, py: 0 }]),
            ],
        },
        {
            name: "Composite transformation (rotate a square about its centre)",
            items: [
                rect(50, 50, 150, 150, K, { thick: 2 }),
                tf(1, RED, [
                    { type: "rotate", deg: 45, px: 100, py: 100 },
                    { type: "scale", sx: 1.5, sy: 1.5, px: 100, py: 100 },
                    { type: "translate", tx: -250, ty: -200 },
                ]),
            ],
        },
        {
            name: "Single line (good for typed / mouse input)",
            items: [line(-200, -100, 250, 150, "dda", K, { thick: 2 })],
        },
        {
            name: "Single circle (good for typed / mouse input)",
            items: [circle(0, 0, 150, K, { thick: 2 })],
        },
    ];

    if (typeof module !== "undefined" && module.exports) module.exports = PRESETS;
    else root.GlutPresets = PRESETS;
})(typeof window !== "undefined" ? window : this);
