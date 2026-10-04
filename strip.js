/* Feature stripper for the full interactive app.
   master.cpp marks every feature with  #if F_NAME ... #elif ... #else ... #endif
   (expressions may use F_ names, !, &&, || and parentheses). strip() keeps
   only the selected code and removes the markers, so the output is plain
   C++ containing nothing that was not selected. */
(function (root) {
    "use strict";

    /* user-facing features: [flag, label, group, default] */
    const FEATURES = [
        ["F_LINE", "Line", "Shapes", true],
        ["F_CIRCLE", "Circle (midpoint)", "Shapes", true],
        ["F_ELLIPSE", "Ellipse (midpoint)", "Shapes", true],
        ["F_RECT", "Rectangle", "Shapes", true],
        ["F_SQUARE", "Square", "Shapes", true],
        ["F_TRIANGLE", "Triangle", "Shapes", true],
        ["F_DIAMOND", "Diamond", "Shapes", true],
        ["F_POLYGON", "Regular polygon", "Shapes", true],
        ["F_STAR", "Star", "Shapes", true],
        ["F_FREEPOLY", "Free polygon", "Shapes", true],

        ["F_BRES", "Bresenham", "Line algorithms (lines and polygon edges)", true],
        ["F_SDDA", "Symmetric DDA", "Line algorithms (lines and polygon edges)", true],
        ["F_DDA", "Simple DDA", "Line algorithms (lines and polygon edges)", true],

        ["F_FILL", "Fill mode", "Fill", true],
        ["F_B4", "Boundary fill 4", "Fill", true],
        ["F_B8", "Boundary fill 8", "Fill", true],
        ["F_FLOOD", "Flood fill", "Fill", true],
        ["F_SCAN", "Scan-line fill", "Fill", true],

        ["F_TRANSFORM", "Transform mode", "2D transformations", true],
        ["F_TRANSLATE", "Translate", "2D transformations", true],
        ["F_SCALE", "Scale", "2D transformations", true],
        ["F_ROTATE", "Rotate", "2D transformations", true],
        ["F_REFLECT", "Reflect", "2D transformations", true],
        ["F_SHEAR", "Shear", "2D transformations", true],
        ["F_COMPOSITE", "Composite", "2D transformations", true],

        ["F_COLOUR", "Line colour list + custom RGB", "Line appearance", true],
        ["F_STYLE", "Line style / dash pattern", "Line appearance", true],
        ["F_THICK", "Thickness buttons", "Line appearance", true],
        ["F_MARKERS", "Endpoint markers", "Line appearance", true],

        ["F_SELECT", "Select / move / group / delete", "Editing and input", true],
        ["F_TYPE", "Type coordinates", "Editing and input", true],
        ["F_UNDO", "Undo / redo", "Editing and input", true],
        ["F_SNAP", "Snap to grid", "Editing and input", true],
        ["F_DRAG", "Drag-to-draw", "Editing and input", true],

        ["F_ZOOM", "Zoom and pan", "View", true],
        ["F_ANIM", "Drawing / fill animation", "View", true],
        ["F_VIEWOPTS", "Grid / labels / dark mode buttons", "View", true],
        ["F_FILE", "Save / load (lines.dat)", "View", true],
    ];

    const SHAPES = ["F_LINE", "F_CIRCLE", "F_ELLIPSE", "F_RECT", "F_SQUARE", "F_TRIANGLE", "F_DIAMOND", "F_POLYGON", "F_STAR", "F_FREEPOLY"];
    const POLYS = ["F_RECT", "F_SQUARE", "F_TRIANGLE", "F_DIAMOND", "F_POLYGON", "F_STAR", "F_FREEPOLY"];
    const ALGOS = ["F_BRES", "F_SDDA", "F_DDA"];
    const FILLS = ["F_B4", "F_B8", "F_FLOOD", "F_SCAN"];
    const TFS = ["F_TRANSLATE", "F_SCALE", "F_ROTATE", "F_REFLECT", "F_SHEAR"];

    /* Applies dependencies and computes helper flags. Returns {flags, notes}. */
    function resolve(sel) {
        const f = {};
        FEATURES.forEach(([k]) => { f[k] = !!(sel && sel[k]); });
        /* Nothing is ever switched ON on your behalf. Features that cannot
           work without something else are left out, with a note. */
        const notes = [];
        const any = (ks) => ks.some(k => f[k]);
        f.F_ANYSHAPE = any(SHAPES);
        f.F_POLY = any(POLYS);
        f.F_LINEALGO = f.F_LINE || f.F_POLY;
        if (!f.F_ANYSHAPE) notes.push("No shape is ticked: the program will have no way to draw anything.");
        if (f.F_LINEALGO && !any(ALGOS))
            notes.push("Warning: lines / polygon edges are ticked but no line algorithm is - they will not be drawn. Tick a line algorithm.");
        if (!f.F_LINEALGO && any(ALGOS)) notes.push("Line algorithms left out: no line or polygon shape is ticked.");
        if (!f.F_LINEALGO) ALGOS.forEach(k => { f[k] = false; });
        if (!f.F_ANYSHAPE) {
            if (f.F_TYPE) notes.push("Type coordinates left out: there is no shape to type.");
            if (f.F_DRAG) notes.push("Drag-to-draw left out: there is no shape to draw.");
            f.F_TYPE = f.F_DRAG = false;
        }
        if (f.F_FILL && !any(FILLS)) { f.F_FILL = false; notes.push("Fill was dropped: no fill algorithm selected."); }
        if (!f.F_FILL) FILLS.forEach(k => { f[k] = false; });
        if (f.F_TRANSFORM && !any(TFS)) { f.F_TRANSFORM = false; notes.push("Transforms were dropped: no transformation selected."); }
        if (!f.F_TRANSFORM) { TFS.forEach(k => { f[k] = false; }); f.F_COMPOSITE = false; }
        if (f.F_TRANSFORM && !f.F_SELECT) { f.F_TRANSFORM = false; notes.push("Transforms left out: they need Select / move (tick both)."); }
        if (!f.F_TRANSFORM) { TFS.forEach(k => { f[k] = false; }); f.F_COMPOSITE = false; }
        f.F_TWOPT = f.F_LINE || f.F_CIRCLE || f.F_ELLIPSE;
        f.F_POLYBUILD = f.F_RECT || f.F_SQUARE || f.F_TRIANGLE || f.F_DIAMOND || f.F_POLYGON || f.F_STAR;
        f.F_TWOCLICK = f.F_TWOPT || f.F_POLYBUILD;
        f.F_VERTS = f.F_FREEPOLY || f.F_TRIANGLE;
        f.F_SEEDFILL = f.F_B4 || f.F_B8 || f.F_FLOOD;
        f.F_PIVOT = f.F_TRANSFORM && (f.F_SCALE || f.F_ROTATE || f.F_SHEAR);
        f.F_INPUTBOX = f.F_TYPE || f.F_COLOUR || f.F_MARKERS || f.F_STYLE || f.F_FILL || f.F_TRANSFORM;
        f.F_DROPDOWN = f.F_COLOUR || f.F_STYLE;
        return { flags: f, notes };
    }

    function evalExpr(expr, flags, lineNo) {
        const js = expr.replace(/\bF_[A-Z0-9_]+\b/g, (name) => {
            if (!(name in flags)) throw new Error("line " + lineNo + ": unknown flag " + name);
            return flags[name] ? "1" : "0";
        });
        if (!/^[01\s|&!()]*$/.test(js)) throw new Error("line " + lineNo + ": bad #if expression: " + expr);
        return !!Function('"use strict"; return (' + js + ");")();
    }


    /* ---- UI styles for the generated app ----
       Each style sets the sidebar side/width, fonts, and every UI colour.
       "classic" is the original GraphicsLab look and stays the default. */
    const C = (r, g, b) => [r, g, b];
    const CLASSIC = {
        name: "Classic (yours)", desc: "Your original look: dark sidebar on the right, white canvas.",
        left: false, width: 260, dark: false,
        font: "GLUT_BITMAP_HELVETICA_12", fontSmall: "GLUT_BITMAP_HELVETICA_10", fontTitle: "GLUT_BITMAP_HELVETICA_12",
        PANEL_BG: C(0.14, 0.15, 0.18), HEADER_BG: C(0.11, 0.12, 0.15), TITLE: C(0.55, 0.75, 1.0),
        SECTION: C(0.42, 0.58, 0.80), HINT: C(0.60, 0.65, 0.72), MUTED: C(0.45, 0.50, 0.58), LINE: C(0.30, 0.33, 0.40),
        BTN: C(0.26, 0.28, 0.32), BTN_ACTIVE: C(0.20, 0.50, 0.90), BTN_BORDER: C(0.40, 0.43, 0.48),
        BTN_TEXT: C(0.92, 0.93, 0.95), BTN_ACTIVE_TEXT: C(0.92, 0.93, 0.95), ARROW: C(0.85, 0.87, 0.90),
        SCROLL_TRACK: C(0.20, 0.21, 0.25), SCROLL_THUMB: C(0.38, 0.42, 0.50), SCROLL_DRAG: C(0.45, 0.65, 0.95),
        DROP_BG: C(0.10, 0.11, 0.13), DROP_BORDER: C(0.35, 0.55, 0.85), HIGHLIGHT: C(0.95, 0.85, 0.45),
        BOX: C(0.10, 0.11, 0.14), BOX_TEXT: C(0.75, 0.78, 0.82), LOG: C(0.45, 0.85, 0.55),
        CANVAS: C(1.0, 1.0, 1.0), CANVAS_DARK: C(0.12, 0.13, 0.15),
        GRID: C(0.90, 0.91, 0.93), GRID_DARK: C(0.22, 0.23, 0.25),
        PIXGRID: C(0.95, 0.95, 0.97), PIXGRID_DARK: C(0.17, 0.18, 0.20),
        AXES: C(0.20, 0.20, 0.22), AXES_DARK: C(0.65, 0.65, 0.68),
        AXLABEL: C(0.45, 0.45, 0.48), AXLABEL_DARK: C(0.55, 0.55, 0.58),
    };
    const STYLES = {
        classic: CLASSIC,
        midnight: Object.assign({}, CLASSIC, {
            name: "Midnight Neon", desc: "Sidebar on the left, deep navy with purple and cyan, dark canvas.",
            left: true, width: 270, dark: true,
            PANEL_BG: C(0.06, 0.07, 0.12), HEADER_BG: C(0.04, 0.05, 0.09), TITLE: C(0.30, 0.90, 1.0),
            SECTION: C(0.75, 0.50, 1.0), HINT: C(0.55, 0.60, 0.80), MUTED: C(0.45, 0.50, 0.70), LINE: C(0.22, 0.25, 0.40),
            BTN: C(0.12, 0.14, 0.22), BTN_ACTIVE: C(0.55, 0.25, 0.95), BTN_BORDER: C(0.28, 0.30, 0.48),
            BTN_TEXT: C(0.88, 0.90, 1.0), BTN_ACTIVE_TEXT: C(1.0, 1.0, 1.0), ARROW: C(0.30, 0.90, 1.0),
            SCROLL_TRACK: C(0.10, 0.11, 0.18), SCROLL_THUMB: C(0.30, 0.32, 0.50), SCROLL_DRAG: C(0.30, 0.90, 1.0),
            DROP_BG: C(0.05, 0.06, 0.10), DROP_BORDER: C(0.55, 0.25, 0.95), HIGHLIGHT: C(0.30, 0.90, 1.0),
            BOX: C(0.05, 0.06, 0.10), BOX_TEXT: C(0.80, 0.84, 1.0), LOG: C(0.40, 0.95, 0.85),
            CANVAS_DARK: C(0.05, 0.06, 0.10), GRID_DARK: C(0.12, 0.14, 0.22), PIXGRID_DARK: C(0.08, 0.09, 0.15),
            AXES_DARK: C(0.55, 0.60, 0.85), AXLABEL_DARK: C(0.50, 0.55, 0.78),
        }),
        light: Object.assign({}, CLASSIC, {
            name: "Clean Light", desc: "Light grey sidebar with dark text and blue buttons, white canvas.",
            PANEL_BG: C(0.95, 0.96, 0.97), HEADER_BG: C(0.88, 0.90, 0.93), TITLE: C(0.10, 0.30, 0.70),
            SECTION: C(0.20, 0.40, 0.75), HINT: C(0.40, 0.44, 0.50), MUTED: C(0.45, 0.48, 0.53), LINE: C(0.78, 0.80, 0.85),
            BTN: C(1.0, 1.0, 1.0), BTN_ACTIVE: C(0.15, 0.45, 0.95), BTN_BORDER: C(0.75, 0.78, 0.83),
            BTN_TEXT: C(0.15, 0.17, 0.20), BTN_ACTIVE_TEXT: C(1.0, 1.0, 1.0), ARROW: C(0.30, 0.32, 0.36),
            SCROLL_TRACK: C(0.86, 0.88, 0.91), SCROLL_THUMB: C(0.65, 0.68, 0.73), SCROLL_DRAG: C(0.15, 0.45, 0.95),
            DROP_BG: C(1.0, 1.0, 1.0), DROP_BORDER: C(0.15, 0.45, 0.95), HIGHLIGHT: C(0.75, 0.45, 0.0),
            BOX: C(0.97, 0.97, 0.98), BOX_TEXT: C(0.20, 0.22, 0.26), LOG: C(0.05, 0.50, 0.20),
        }),
        retro: Object.assign({}, CLASSIC, {
            name: "Retro Terminal", desc: "Black and green like an old terminal, fixed-width font, sidebar on the left.",
            left: true, width: 300, dark: true,
            font: "GLUT_BITMAP_8_BY_13", fontSmall: "GLUT_BITMAP_8_BY_13", fontTitle: "GLUT_BITMAP_9_BY_15",
            PANEL_BG: C(0.0, 0.0, 0.0), HEADER_BG: C(0.0, 0.08, 0.0), TITLE: C(0.2, 1.0, 0.3),
            SECTION: C(0.1, 0.8, 0.25), HINT: C(0.1, 0.65, 0.25), MUTED: C(0.1, 0.6, 0.2), LINE: C(0.1, 0.5, 0.15),
            BTN: C(0.0, 0.10, 0.02), BTN_ACTIVE: C(0.1, 0.75, 0.25), BTN_BORDER: C(0.1, 0.6, 0.2),
            BTN_TEXT: C(0.3, 1.0, 0.4), BTN_ACTIVE_TEXT: C(0.0, 0.0, 0.0), ARROW: C(0.3, 1.0, 0.4),
            SCROLL_TRACK: C(0.0, 0.08, 0.02), SCROLL_THUMB: C(0.1, 0.5, 0.15), SCROLL_DRAG: C(0.3, 1.0, 0.4),
            DROP_BG: C(0.0, 0.06, 0.0), DROP_BORDER: C(0.2, 1.0, 0.3), HIGHLIGHT: C(1.0, 0.9, 0.2),
            BOX: C(0.0, 0.05, 0.0), BOX_TEXT: C(0.3, 1.0, 0.4), LOG: C(0.3, 1.0, 0.4),
            CANVAS_DARK: C(0.0, 0.0, 0.0), GRID_DARK: C(0.0, 0.18, 0.05), PIXGRID_DARK: C(0.0, 0.10, 0.03),
            AXES_DARK: C(0.2, 0.9, 0.3), AXLABEL_DARK: C(0.1, 0.7, 0.2),
        }),
        ocean: Object.assign({}, CLASSIC, {
            name: "Ocean Breeze", desc: "Wide teal sidebar on the right with orange buttons, soft blue canvas.",
            width: 300,
            PANEL_BG: C(0.05, 0.20, 0.25), HEADER_BG: C(0.03, 0.15, 0.19), TITLE: C(1.0, 0.75, 0.40),
            SECTION: C(0.45, 0.85, 0.85), HINT: C(0.60, 0.80, 0.82), MUTED: C(0.45, 0.65, 0.68), LINE: C(0.15, 0.38, 0.43),
            BTN: C(0.08, 0.28, 0.33), BTN_ACTIVE: C(0.95, 0.50, 0.20), BTN_BORDER: C(0.20, 0.45, 0.50),
            BTN_TEXT: C(0.92, 0.98, 0.98), BTN_ACTIVE_TEXT: C(1.0, 1.0, 1.0), ARROW: C(1.0, 0.75, 0.40),
            SCROLL_TRACK: C(0.04, 0.16, 0.20), SCROLL_THUMB: C(0.20, 0.45, 0.50), SCROLL_DRAG: C(0.95, 0.50, 0.20),
            DROP_BG: C(0.03, 0.15, 0.19), DROP_BORDER: C(0.95, 0.50, 0.20), HIGHLIGHT: C(1.0, 0.75, 0.40),
            BOX: C(0.03, 0.15, 0.19), BOX_TEXT: C(0.85, 0.95, 0.95), LOG: C(1.0, 0.80, 0.45),
            CANVAS: C(0.97, 0.99, 1.0), GRID: C(0.85, 0.92, 0.95), PIXGRID: C(0.92, 0.96, 0.98),
            AXES: C(0.10, 0.30, 0.38), AXLABEL: C(0.25, 0.45, 0.52),
        }),
    };
    const COLOUR_KEYS = ["PANEL_BG", "HEADER_BG", "TITLE", "SECTION", "HINT", "MUTED", "LINE",
        "BTN", "BTN_ACTIVE", "BTN_BORDER", "BTN_TEXT", "BTN_ACTIVE_TEXT", "ARROW",
        "SCROLL_TRACK", "SCROLL_THUMB", "SCROLL_DRAG", "DROP_BG", "DROP_BORDER", "HIGHLIGHT",
        "BOX", "BOX_TEXT", "LOG", "CANVAS", "CANVAS_DARK", "GRID", "GRID_DARK",
        "PIXGRID", "PIXGRID_DARK", "AXES", "AXES_DARK", "AXLABEL", "AXLABEL_DARK"];

    function f3(v) {
        return v.map(x => { const s = (Math.round(x * 100) / 100).toFixed(2); return s + "f"; }).join(", ");
    }
    /* C++ block that replaces the //@@UI_STYLE@@ line */
    function styleBlock(key) {
        const t = STYLES[key] || CLASSIC;
        const out = [];
        out.push("/* ===========================================================================");
        out.push("   UI STYLE: " + t.name);
        out.push("   Colours, fonts and layout of the window. Change these to restyle it.");
        out.push("   =========================================================================== */");
        out.push("const bool  PANEL_ON_LEFT = " + (t.left ? "true" : "false") + ";      /* sidebar on the left (true) or right (false) */");
        out.push("const float PANEL_W = " + t.width.toFixed(1) + "f;          /* sidebar width in pixels */");
        out.push("const bool  START_DARK = " + (t.dark ? "true" : "false") + ";         /* canvas starts in dark mode */");
        out.push("void* const UI_FONT = " + t.font + ";         /* button labels */");
        out.push("void* const UI_FONT_SMALL = " + t.fontSmall + ";   /* section titles, hints, status */");
        out.push("void* const UI_TITLE_FONT = " + t.fontTitle + ";   /* sidebar title */");
        COLOUR_KEYS.forEach(k => {
            const name = ("UI_" + k + "[3]").padEnd(20);
            out.push("const float " + name + " = { " + f3(t[k]) + " };");
        });
        return out.join("\n");
    }

    function strip(src, flags, styleKey) {
        const lines = src.split(/\r?\n/);
        const out = [];
        const stack = [];            /* {parent, taken, active} */
        let active = true;
        lines.forEach((line, i) => {
            const m = /^\s*#\s*(if|elif|else|endif)\b(.*)$/.exec(line);
            if (m) {
                const kw = m[1], rest = m[2].trim();
                if (kw === "if") {
                    const cond = evalExpr(rest, flags, i + 1);
                    stack.push({ parent: active, taken: cond, active: active && cond });
                    active = active && cond;
                    return;
                }
                if (!stack.length) throw new Error("line " + (i + 1) + ": #" + kw + " without #if");
                const top = stack[stack.length - 1];
                if (kw === "elif") {
                    const cond = !top.taken && evalExpr(rest, flags, i + 1);
                    top.active = top.parent && cond;
                    if (cond) top.taken = true;
                }
                else if (kw === "else") {
                    top.active = top.parent && !top.taken;
                    top.taken = true;
                }
                else {
                    stack.pop();
                    active = top.parent;
                    return;
                }
                active = top.active;
                return;
            }
            if (active) out.push(line);
        });
        if (stack.length) throw new Error("unclosed #if (" + stack.length + " open)");
        /* tidy: no runs of blank lines */
        const tidy = [];
        out.forEach(l => {
            if (l.trim() === "" && tidy.length && tidy[tidy.length - 1].trim() === "") return;
            tidy.push(l);
        });
        let text = tidy.join("\n").replace(/\n+$/, "") + "\n";
        text = text.replace("//@@UI_STYLE@@", styleBlock(styleKey || "classic"));
        return text;
    }

    const api = { FEATURES, resolve, strip, STYLES, styleBlock };
    if (typeof module !== "undefined" && module.exports) module.exports = api;
    else root.AppStrip = api;
})(typeof window !== "undefined" ? window : this);
