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

    function strip(src, flags) {
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
        return tidy.join("\n").replace(/\n+$/, "") + "\n";
    }

    const api = { FEATURES, resolve, strip };
    if (typeof module !== "undefined" && module.exports) module.exports = api;
    else root.AppStrip = api;
})(typeof window !== "undefined" ? window : this);
