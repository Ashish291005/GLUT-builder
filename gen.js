/* GLUT program generator.
   generate(config) -> C++ source (string) for one standalone program that
   contains only the algorithms the selected items need. Works in the
   browser (window.GlutGen) and in Node (module.exports). */
(function (root) {
    "use strict";

    const LINE_FN = { dda: "ddaLine", sdda: "symmetricDdaLine", bres: "bresenhamLine" };
    const LINE_NAME = { dda: "DDA", sdda: "Symmetric DDA", bres: "Bresenham" };
    /* 1 = pixel on, 0 = off; the pattern repeats along the line */
    const STYLES = {
        solid: null,
        dotted: "100",
        dashed: "1111110000",
        dashdot: "1111111100100",
        longdash: "11111111111111000000",
    };
    const STYLE_IDS = { solid: 0, dotted: 1, dashed: 2, dashdot: 3, longdash: 4 };
    const FILL_NAME = {
        b4: "Boundary fill (4-connected)", b8: "Boundary fill (8-connected)",
        f4: "Flood fill (4-connected)", f8: "Flood fill (8-connected)",
    };
    const POLY_TYPES = ["rect", "triangle", "polygon", "regpoly"];
    const PREFIX = {
        line: "L", circle: "C", arc: "A", ellipse: "E", rect: "R", triangle: "T",
        polygon: "P", regpoly: "G", fill: "F", scanfill: "S", transform: "X",
    };
    const TYPE_LABEL = {
        line: "Line", circle: "Circle", arc: "Arc", ellipse: "Ellipse", rect: "Rectangle",
        triangle: "Triangle", polygon: "Polygon", regpoly: "Regular polygon", fill: "Seed fill",
        scanfill: "Scan-line polygon fill", transform: "2D transformation",
    };

    function num(v, def) {
        const n = Number(v);
        return Number.isFinite(n) ? n : (def === undefined ? 0 : def);
    }
    function lit(v) {
        const n = num(v);
        return Number.isInteger(n) ? String(n) : String(Math.round(n * 1000) / 1000);
    }
    function rgb(hex) {
        const m = /^#?([0-9a-f]{6})$/i.exec(hex || "#000000");
        const v = m ? parseInt(m[1], 16) : 0;
        const c = [(v >> 16) & 255, (v >> 8) & 255, v & 255].map(x => {
            const f = Math.round((x / 255) * 100) / 100;
            return f === 0 ? "0.0f" : f === 1 ? "1.0f" : f + "f";
        });
        return c.join(", ");
    }
    function parsePts(text) {
        const n = String(text || "").match(/-?\d+(\.\d+)?/g) || [];
        const pts = [];
        for (let i = 0; i + 1 < n.length; i += 2) pts.push([Number(n[i]), Number(n[i + 1])]);
        return pts;
    }
    function isPoly(t) { return POLY_TYPES.indexOf(t) >= 0; }
    /* vertex arrays are double: cast int variables, keep literals as they are */
    function D(e) { return /^-?\d+(\.\d+)?$/.test(e) ? e : "(double)" + e; }
    function V(p) { return "{" + D(p[0]) + ", " + D(p[1]) + "}"; }
    function vertexCount(it) {
        if (it.type === "rect") return 4;
        if (it.type === "triangle") return 3;
        if (it.type === "polygon") return parsePts(it.pts).length;
        if (it.type === "regpoly") return Math.max(3, Math.min(64, Math.round(num(it.n, 6))));
        if (it.type === "line") return 2;
        return 0;
    }

    /* ---- parameter expressions, depending on the input mode ----
       fixed   : the literal value
       console : a global variable read with cin in main()
       mouse   : points come from clicks; derived values from the clicks */
    function Ctx(cfg) {
        this.mode = cfg.input || "fixed";
        this.consoleVars = [];     // names
        this.consoleGroups = [];   // {prompt, vars[]}
        this.clickLabels = [];     // prompt per click
        this.cur = null;           // current item group
    }
    Ctx.prototype.begin = function (label) {
        this.cur = { prompt: label, vars: [], fields: [], maxClick: -1 };
        return this.cur;
    };
    Ctx.prototype.end = function () {
        if (this.cur && this.cur.vars.length) this.consoleGroups.push(this.cur);
        const g = this.cur; this.cur = null; return g;
    };
    /* plain number (never from the mouse) */
    Ctx.prototype.n = function (name, field, value, isFloat) {
        if (this.mode !== "console") return lit(value);
        this.consoleVars.push((isFloat ? "double " : "int ") + name + " = " + lit(value) + ";");
        this.cur.vars.push(name); this.cur.fields.push(field);
        return name;
    };
    /* a point: [xExpr, yExpr] */
    Ctx.prototype.pt = function (name, fx, fy, vx, vy, clickLabel) {
        if (this.mode === "mouse") {
            const k = this.clickLabels.length;
            this.clickLabels.push(clickLabel);
            this.cur.maxClick = k;
            return ["clickX[" + k + "]", "clickY[" + k + "]"];
        }
        return [this.n(name + "_" + fx, fx, vx), this.n(name + "_" + fy, fy, vy)];
    };

    function generate(cfg) {
        cfg = cfg || {};
        const W = Math.max(200, Math.round(num(cfg.width, 800)));
        const H = Math.max(200, Math.round(num(cfg.height, 600)));
        const centered = cfg.origin !== "corner";
        const items = Array.isArray(cfg.items) ? cfg.items : [];
        const ctx = new Ctx(cfg);
        const mouse = ctx.mode === "mouse";
        const recursive = cfg.fillEngine === "recursive";
        const useReadPixels = cfg.pixelRead === "glread";
        const flushEvery = Math.max(1, Math.round(num(cfg.flushEvery, 150)));

        /* ---- which features are needed ---- */
        const need = {
            line: {}, circle: false, arc: false, ellipse: false, poly: false, regpoly: false,
            style: false, fill: {}, scan: false, transform: false, tfKinds: {}, pixelGrid: false,
        };
        const named = {};    // item index -> C array name for polygonal/line items
        const errors = [];
        items.forEach((it, i) => {
            const style = it.style && it.style !== "solid";
            if (style && ["line", "rect", "triangle", "polygon", "regpoly", "circle", "ellipse", "arc"].indexOf(it.type) >= 0) need.style = true;
            if (it.type === "line" || isPoly(it.type)) need.line[it.algo || "bres"] = true;
            if (isPoly(it.type)) need.poly = true;
            if (it.type === "regpoly") need.regpoly = true;
            if (it.type === "circle") need.circle = true;
            if (it.type === "arc") need.arc = true;
            if (it.type === "ellipse") need.ellipse = true;
            if (it.type === "fill") { need.fill[it.fillAlgo || "b4"] = true; need.pixelGrid = true; }
            if (it.type === "scanfill") { need.scan = true; need.poly = true; need.pixelGrid = true; }
            if (it.type === "transform") {
                need.transform = true; need.poly = true;
                (it.ops || []).forEach(op => { need.tfKinds[op.type] = true; });
            }
        });
        items.forEach((it, i) => {
            if (it.type !== "scanfill" && it.type !== "transform") return;
            const r = Math.round(num(it.ref, -1)) - 1;
            const ref = items[r];
            if (!(r >= 0 && r < i && ref && (isPoly(ref.type) || (it.type === "transform" && ref.type === "line")))) {
                errors.push(i);
                return;
            }
            named[r] = true;
            if (ref.type === "line") need.poly = true;
        });
        const anyLine = Object.keys(need.line).length > 0 || need.poly;
        if (need.poly && Object.keys(need.line).length === 0) need.line.bres = true;
        const anyFill = Object.keys(need.fill).length > 0 || need.scan;

        const out = [];
        const add = (s) => out.push(s === undefined ? "" : s);

        /* ---------- header ---------- */
        add("/*");
        add(" * " + (cfg.title || "Computer Graphics Practical"));
        add(" *");
        const used = [];
        Object.keys(need.line).forEach(k => used.push(LINE_NAME[k] + " line"));
        if (need.circle) used.push("Midpoint circle");
        if (need.arc) used.push("Midpoint circle arc");
        if (need.ellipse) used.push("Midpoint ellipse");
        Object.keys(need.fill).forEach(k => used.push(FILL_NAME[k] + (recursive ? " (recursive)" : " (stack based)")));
        if (need.scan) used.push("Scan-line polygon fill");
        if (need.transform) used.push("2D transformations (3x3 homogeneous matrices)");
        if (used.length) add(" * Algorithms: " + used.join(", "));
        add(" * Input     : " + (ctx.mode === "console" ? "values typed in the console" :
            mouse ? "mouse clicks on the window" : "fixed values in the code"));
        add(" * Origin    : " + (centered ? "centre of the window (x right, y up)" : "bottom-left corner"));
        add(" *");
        add(" * Build: Visual Studio C++ console project with the nupengl.core NuGet");
        add(" *        package (freeglut), or  g++ prog.cpp -lfreeglut -lopengl32 -lglu32");
        add(" */");
        if (anyFill && recursive) {
            add("#ifdef _MSC_VER");
            add("#pragma comment(linker, \"/STACK:268435456\")   // big stack for deep recursion");
            add("#endif");
        }
        add("#define _CRT_SECURE_NO_WARNINGS   // allow sprintf/printf in Visual Studio");
        add("#include <GL/glut.h>");
        add("#include <cmath>");
        add("#include <cstdio>");
        add("#include <iostream>");
        if (anyFill && !recursive) add("#include <vector>");
        if (need.scan) add("#include <algorithm>");
        add("using namespace std;");
        add();
        add("const int W = " + W + ", H = " + H + ";          // window size in pixels");
        add("const int OX = " + (centered ? "W / 2" : "0") + ", OY = " + (centered ? "H / 2" : "0") +
            ";  // where (0,0) is, in window pixels");
        if (need.circle || need.arc || need.ellipse || need.regpoly || need.tfKinds.rotate || need.tfKinds.reflect || mouse)
            add("const double PI = 3.14159265358979;");
        add();

        /* ---------- mouse globals ---------- */
        const clickDecl = [];   // filled later once count is known

        /* ---------- pixel plotting ---------- */
        if (need.pixelGrid && !useReadPixels) {
            add("/* Copy of every pixel's colour, so a fill can 'read' the screen");
            add("   (getpixel) quickly and reliably. */");
            add("float screenBuf[H][W][3];");
            add("float bgColor[3] = { " + rgb(cfg.bg || "#ffffff") + " };");
            add("float curColor[3] = { 0, 0, 0 };");
            add("float curSize = 1;");
            add();
            add("void setColor(float r, float g, float b) { curColor[0] = r; curColor[1] = g; curColor[2] = b; glColor3f(r, g, b); }");
            add("void setSize(float s) { curSize = s; glPointSize(s); }");
            add();
            add("void clearScreenBuf() {");
            add("    for (int y = 0; y < H; y++)");
            add("        for (int x = 0; x < W; x++)");
            add("            for (int k = 0; k < 3; k++) screenBuf[y][x][k] = bgColor[k];");
            add("}");
            add();
            add("void plot(int x, int y) {");
            add("    glBegin(GL_POINTS);");
            add("    glVertex2f(x + 0.5f, y + 0.5f);");
            add("    glEnd();");
            add("    int h = (int)curSize / 2;            // a thick point covers a square");
            add("    for (int j = -h; j <= h; j++)");
            add("        for (int i = -h; i <= h; i++) {");
            add("            int px = x + i + OX, py = y + j + OY;");
            add("            if (px >= 0 && px < W && py >= 0 && py < H)");
            add("                for (int k = 0; k < 3; k++) screenBuf[py][px][k] = curColor[k];");
            add("        }");
            add("}");
        }
        else {
            add("void setColor(float r, float g, float b) { glColor3f(r, g, b); }");
            add("void setSize(float s) { glPointSize(s); }");
            add();
            add("void plot(int x, int y) {");
            add("    glBegin(GL_POINTS);");
            add("    glVertex2f(x + 0.5f, y + 0.5f);   // +0.5 = centre of pixel (x,y)");
            add("    glEnd();");
            add("}");
        }
        add();

        /* ---------- line styles ---------- */
        if (need.style) {
            add("/* Line styles: 1 = draw the pixel, 0 = skip it. The pattern repeats. */");
            add("const char* STYLE[] = {");
            add("    \"1\",                       // 0 solid");
            add("    \"" + STYLES.dotted + "\",                     // 1 dotted");
            add("    \"" + STYLES.dashed + "\",              // 2 dashed");
            add("    \"" + STYLES.dashdot + "\",           // 3 dash-dot");
            add("    \"" + STYLES.longdash + "\"     // 4 long dash");
            add("};");
            add("int styleNow = 0;   // style used by the line / circle functions");
            add();
            add("void stylePlot(int x, int y, int i) {   // i = pixel number along the curve");
            add("    const char* p = STYLE[styleNow];");
            add("    int len = (int)strlen(p);");
            add("    if (p[i % len] == '1') plot(x, y);");
            add("}");
            add();
            out.splice(out.indexOf("#include <cstdio>") + 1, 0, "#include <cstring>");
        }
        const P = need.style ? (x, y, i) => "stylePlot(" + x + ", " + y + ", " + i + ")" : (x, y) => "plot(" + x + ", " + y + ")";
        /* the pixel counter i is only needed for line styles */
        const IP = need.style ? ", int i" : "";
        const IA = (e) => need.style ? ", " + e : "";
        const IL = (line) => need.style ? [line] : [];

        /* ---------- line algorithms ---------- */
        if (need.line.dda) {
            add("/* DDA line: step along the longer axis one pixel at a time and add");
            add("   the slope to the other coordinate. */");
            add("void ddaLine(int x1, int y1, int x2, int y2) {");
            add("    int dx = x2 - x1, dy = y2 - y1;");
            add("    int steps = abs(dx) > abs(dy) ? abs(dx) : abs(dy);");
            add("    if (steps == 0) { " + P("x1", "y1", "0") + "; return; }");
            add("    float xInc = dx / (float)steps;");
            add("    float yInc = dy / (float)steps;");
            add("    float x = (float)x1, y = (float)y1;");
            add("    for (int i = 0; i <= steps; i++) {");
            add("        " + P("(int)floor(x + 0.5f)", "(int)floor(y + 0.5f)", "i") + ";");
            add("        x += xInc;");
            add("        y += yInc;");
            add("    }");
            add("}");
            add();
        }
        if (need.line.sdda) {
            add("/* Symmetric DDA: choose n = 2^k >= max(|dx|,|dy|), epsilon = 1/2^k,");
            add("   and add epsilon*dx, epsilon*dy at each step. */");
            add("void symmetricDdaLine(int x1, int y1, int x2, int y2) {");
            add("    int dx = x2 - x1, dy = y2 - y1;");
            add("    int len = abs(dx) > abs(dy) ? abs(dx) : abs(dy);");
            add("    if (len == 0) { " + P("x1", "y1", "0") + "; return; }");
            add("    int n = 1;");
            add("    while (n < len) n *= 2;               // n = 2^k");
            add("    float eps = 1.0f / n;");
            add("    float x = x1 + 0.5f, y = y1 + 0.5f;");
            add("    for (int i = 0; i <= n; i++) {");
            add("        " + P("(int)floor(x)", "(int)floor(y)", "i") + ";");
            add("        x += eps * dx;");
            add("        y += eps * dy;");
            add("    }");
            add("}");
            add();
        }
        if (need.line.bres) {
            add("/* Bresenham line (all slopes): integer decision parameter p decides");
            add("   whether the minor coordinate also steps. */");
            add("void bresenhamLine(int x1, int y1, int x2, int y2) {");
            add("    int dx = abs(x2 - x1), dy = abs(y2 - y1);");
            add("    int sx = (x2 >= x1) ? 1 : -1, sy = (y2 >= y1) ? 1 : -1;");
            add("    int x = x1, y = y1;");
            add("    if (dx >= dy) {                       // |slope| <= 1: step in x");
            add("        int p = 2 * dy - dx;");
            add("        for (int i = 0; i <= dx; i++) {");
            add("            " + P("x", "y", "i") + ";");
            add("            if (p < 0) p += 2 * dy;");
            add("            else { y += sy; p += 2 * dy - 2 * dx; }");
            add("            x += sx;");
            add("        }");
            add("    }");
            add("    else {                                // |slope| > 1: step in y");
            add("        int p = 2 * dx - dy;");
            add("        for (int i = 0; i <= dy; i++) {");
            add("            " + P("x", "y", "i") + ";");
            add("            if (p < 0) p += 2 * dx;");
            add("            else { x += sx; p += 2 * dx - 2 * dy; }");
            add("            y += sy;");
            add("        }");
            add("    }");
            add("}");
            add();
        }

        /* ---------- circle / arc / ellipse ---------- */
        if (need.circle) {
            add("/* Midpoint circle: compute one octant, mirror it to the other 7. */");
            add("void plot8(int xc, int yc, int x, int y" + IP + ") {");
            ["xc + x, yc + y", "xc - x, yc + y", "xc + x, yc - y", "xc - x, yc - y",
                "xc + y, yc + x", "xc - y, yc + x", "xc + y, yc - x", "xc - y, yc - x"].forEach(a => {
                    const [ax, ay] = a.split(", ");
                    add("    " + P(ax, ay, "i") + ";");
                });
            add("}");
            add();
            add("void midpointCircle(int xc, int yc, int r) {");
            add("    int x = 0, y = r;");
            add("    int p = 1 - r;                        // initial decision parameter");
            IL("    int i = 0;").forEach(add);
            add("    while (x <= y) {");
            add("        plot8(xc, yc, x, y" + IA("i++") + ");");
            add("        x++;");
            add("        if (p < 0) p += 2 * x + 1;");
            add("        else { y--; p += 2 * (x - y) + 1; }");
            add("    }");
            add("}");
            add();
        }
        if (need.arc) {
            add("/* Arc = midpoint circle, keeping only points whose angle is in");
            add("   [startDeg, endDeg] (measured counter-clockwise from +x). */");
            add("void arcPoint(int xc, int yc, int dx, int dy, double a0, double a1" + IP + ") {");
            add("    double a = atan2((double)dy, (double)dx) * 180.0 / PI;");
            add("    if (a < 0) a += 360;");
            add("    bool in = (a0 <= a1) ? (a >= a0 && a <= a1) : (a >= a0 || a <= a1);");
            add("    if (in) " + P("xc + dx", "yc + dy", "i") + ";");
            add("}");
            add();
            add("void midpointArc(int xc, int yc, int r, double startDeg, double endDeg) {");
            add("    double a0 = fmod(fmod(startDeg, 360) + 360, 360), a1 = fmod(fmod(endDeg, 360) + 360, 360);");
            add("    int x = 0, y = r, p = 1 - r;");
            IL("    int i = 0;").forEach(add);
            add("    while (x <= y) {");
            add("        int d[8][2] = { {x,y},{-x,y},{x,-y},{-x,-y},{y,x},{-y,x},{y,-x},{-y,-x} };");
            add("        for (int k = 0; k < 8; k++) arcPoint(xc, yc, d[k][0], d[k][1], a0, a1" + IA("i") + ");");
            IL("        i++;").forEach(add);
            add("        x++;");
            add("        if (p < 0) p += 2 * x + 1;");
            add("        else { y--; p += 2 * (x - y) + 1; }");
            add("    }");
            add("}");
            add();
        }
        if (need.ellipse) {
            add("/* Midpoint ellipse: region 1 (slope > -1) steps in x, region 2 steps");
            add("   in y; each point is mirrored to all 4 quadrants. */");
            add("void plot4(int xc, int yc, int x, int y" + IP + ") {");
            add("    " + P("xc + x", "yc + y", "i") + ";");
            add("    " + P("xc - x", "yc + y", "i") + ";");
            add("    " + P("xc + x", "yc - y", "i") + ";");
            add("    " + P("xc - x", "yc - y", "i") + ";");
            add("}");
            add();
            add("void midpointEllipse(int xc, int yc, int rx, int ry) {");
            add("    double rx2 = (double)rx * rx, ry2 = (double)ry * ry;");
            add("    double x = 0, y = ry;");
            add("    double dx = 2 * ry2 * x, dy = 2 * rx2 * y;");
            IL("    int i = 0;").forEach(add);
            add("    // region 1");
            add("    double p1 = ry2 - rx2 * ry + 0.25 * rx2;");
            add("    while (dx < dy) {");
            add("        plot4(xc, yc, (int)x, (int)y" + IA("i++") + ");");
            add("        x++; dx += 2 * ry2;");
            add("        if (p1 < 0) p1 += dx + ry2;");
            add("        else { y--; dy -= 2 * rx2; p1 += dx - dy + ry2; }");
            add("    }");
            add("    // region 2");
            add("    double p2 = ry2 * (x + 0.5) * (x + 0.5) + rx2 * (y - 1) * (y - 1) - rx2 * ry2;");
            add("    while (y >= 0) {");
            add("        plot4(xc, yc, (int)x, (int)y" + IA("i++") + ");");
            add("        y--; dy -= 2 * rx2;");
            add("        if (p2 > 0) p2 += rx2 - dy;");
            add("        else { x++; dx += 2 * ry2; p2 += dx - dy + rx2; }");
            add("    }");
            add("}");
            add();
        }

        /* ---------- polygons ---------- */
        if (need.poly) {
            add("typedef void (*LineFunc)(int, int, int, int);");
            add();
            add("/* Draws the closed polygon v[0..n-1] edge by edge with a line algorithm. */");
            add("void drawPolygon(double v[][2], int n, LineFunc line) {");
            add("    for (int i = 0; i < n; i++) {");
            add("        int j = (i + 1) % n;");
            add("        line((int)floor(v[i][0] + 0.5), (int)floor(v[i][1] + 0.5),");
            add("             (int)floor(v[j][0] + 0.5), (int)floor(v[j][1] + 0.5));");
            add("    }");
            add("}");
            add();
        }
        if (need.regpoly) {
            add("/* Vertices of a regular n-gon: centre (cx,cy), radius r, first vertex at rotDeg. */");
            add("void regularPolygon(double cx, double cy, double r, int n, double rotDeg, double v[][2]) {");
            add("    for (int i = 0; i < n; i++) {");
            add("        double a = (rotDeg + 360.0 * i / n) * PI / 180.0;");
            add("        v[i][0] = floor(cx + r * cos(a) + 0.5);");
            add("        v[i][1] = floor(cy + r * sin(a) + 0.5);");
            add("    }");
            add("}");
            add();
        }

        /* ---------- fills ---------- */
        if (anyFill) {
            add("/* ---- Filling ---- */");
            add("int fillCount = 0;");
            add("const int FLUSH_EVERY = " + flushEvery + ";   // smaller = slower, more visible animation");
            add();
            if (useReadPixels) {
                add("/* getpixel: read the colour of pixel (x,y) back from the window. */");
                add("void getPixel(int x, int y, float c[3]) {");
                add("    glReadPixels(x + OX, y + OY, 1, 1, GL_RGB, GL_FLOAT, c);");
                add("}");
            }
            else {
                add("/* getpixel: colour of pixel (x,y), from the copy kept by plot(). */");
                add("void getPixel(int x, int y, float c[3]) {");
                add("    for (int k = 0; k < 3; k++) c[k] = screenBuf[y + OY][x + OX][k];");
                add("}");
            }
            add();
            add("bool sameColor(const float a[3], const float b[3]) {");
            add("    return fabs(a[0] - b[0]) < 0.02f && fabs(a[1] - b[1]) < 0.02f && fabs(a[2] - b[2]) < 0.02f;");
            add("}");
            add();
            add("bool inWindow(int x, int y) {");
            add("    return x + OX >= 0 && x + OX < W && y + OY >= 0 && y + OY < H;");
            add("}");
            add();
            add("/* putpixel for fills: always 1 pixel; flushes now and then to animate. */");
            add("void setPixel(int x, int y, const float c[3]) {");
            add("    setColor(c[0], c[1], c[2]);");
            add("    setSize(1);");
            add("    plot(x, y);");
            add("    if (++fillCount % FLUSH_EVERY == 0) glFlush();");
            add("}");
            add();
        }
        const N4 = [["x + 1", "y"], ["x - 1", "y"], ["x", "y + 1"], ["x", "y - 1"]];
        const N8 = N4.concat([["x + 1", "y + 1"], ["x - 1", "y + 1"], ["x - 1", "y - 1"], ["x + 1", "y - 1"]]);
        function emitSeedFill(kind) {
            const conn = kind.endsWith("8") ? 8 : 4;
            const nb = conn === 8 ? N8 : N4;
            const boundary = kind[0] === "b";
            const fname = (boundary ? "boundaryFill" : "floodFill") + conn;
            const extra = boundary ? "const float border[3]" : "const float oldColor[3]";
            const stopCond = boundary ? "sameColor(c, border) || sameColor(c, fill)" : "!sameColor(c, oldColor) || sameColor(c, fill)";
            add("/* " + FILL_NAME[kind] + (boundary
                ? ": colour every pixel reachable from the seed\n   without crossing the boundary colour. */"
                : ": recolour every pixel that has the seed's\n   old colour and is connected to it. */"));
            if (recursive) {
                add("void " + fname + "(int x, int y, const float fill[3], " + extra + ") {");
                add("    if (!inWindow(x, y)) return;");
                add("    float c[3];");
                add("    getPixel(x, y, c);");
                add("    if (" + stopCond + ") return;");
                add("    setPixel(x, y, fill);");
                nb.forEach(n => add("    " + fname + "(" + n[0] + ", " + n[1] + ", fill, " + (boundary ? "border" : "oldColor") + ");"));
                add("}");
            }
            else {
                add("/* Uses an explicit stack instead of recursion (same order of filling,");
                add("   but no stack overflow on big regions). */");
                add("void " + fname + "(int sx, int sy, const float fill[3], " + extra + ") {");
                add("    vector<int> stackX, stackY;");
                add("    stackX.push_back(sx); stackY.push_back(sy);");
                add("    while (!stackX.empty()) {");
                add("        int x = stackX.back(), y = stackY.back();");
                add("        stackX.pop_back(); stackY.pop_back();");
                add("        if (!inWindow(x, y)) continue;");
                add("        float c[3];");
                add("        getPixel(x, y, c);");
                add("        if (" + stopCond + ") continue;");
                add("        setPixel(x, y, fill);");
                add("        // push in reverse so (x+1, y) is visited first, like the recursive version");
                nb.slice().reverse().forEach(n => add("        stackX.push_back(" + n[0] + "); stackY.push_back(" + n[1] + ");"));
                add("    }");
                add("}");
            }
            add();
            if (!boundary) {
                add("void " + fname + "Start(int x, int y, const float fill[3]) {");
                add("    float oldColor[3];");
                add("    getPixel(x, y, oldColor);          // colour to be replaced");
                add("    if (!sameColor(oldColor, fill)) " + fname + "(x, y, fill, oldColor);");
                add("}");
                add();
            }
        }
        Object.keys(need.fill).sort().forEach(emitSeedFill);
        if (need.scan) {
            add("/* Scan-line polygon fill: for every scan line y, find where it crosses");
            add("   the edges, sort the crossings, and fill between pairs of them. */");
            add("void scanlineFill(double v[][2], int n, const float fill[3]) {");
            add("    double ymin = v[0][1], ymax = v[0][1];");
            add("    for (int i = 1; i < n; i++) { ymin = min(ymin, v[i][1]); ymax = max(ymax, v[i][1]); }");
            add("    double xs[64];");
            add("    for (int y = (int)ceil(ymin); y <= (int)floor(ymax); y++) {");
            add("        int k = 0;");
            add("        for (int i = 0; i < n; i++) {");
            add("            double x1 = v[i][0], y1 = v[i][1];");
            add("            double x2 = v[(i + 1) % n][0], y2 = v[(i + 1) % n][1];");
            add("            if (y1 == y2) continue;                       // skip horizontal edges");
            add("            if ((y >= y1 && y < y2) || (y >= y2 && y < y1))   // half-open: no double count at vertices");
            add("                if (k < 64) xs[k++] = x1 + (y - y1) * (x2 - x1) / (y2 - y1);");
            add("        }");
            add("        sort(xs, xs + k);");
            add("        for (int i = 0; i + 1 < k; i += 2)");
            add("            for (int x = (int)ceil(xs[i]); x <= (int)floor(xs[i + 1]); x++)");
            add("                setPixel(x, y, fill);");
            add("        glFlush();                                       // one scan line at a time");
            add("    }");
            add("}");
            add();
        }

        /* ---------- transformations ---------- */
        if (need.transform) {
            add("/* ---- 2D transformations with 3x3 homogeneous matrices ----");
            add("   [x']   [a b c] [x]");
            add("   [y'] = [d e f] [y]");
            add("   [1 ]   [0 0 1] [1]                                        */");
            add("void identity(double M[3][3]) {");
            add("    for (int i = 0; i < 3; i++)");
            add("        for (int j = 0; j < 3; j++) M[i][j] = (i == j) ? 1 : 0;");
            add("}");
            add();
            add("/* M = A * M  (apply A after what M already does) */");
            add("void compose(double M[3][3], double A[3][3]) {");
            add("    double R[3][3];");
            add("    for (int i = 0; i < 3; i++)");
            add("        for (int j = 0; j < 3; j++) {");
            add("            R[i][j] = 0;");
            add("            for (int k = 0; k < 3; k++) R[i][j] += A[i][k] * M[k][j];");
            add("        }");
            add("    for (int i = 0; i < 3; i++)");
            add("        for (int j = 0; j < 3; j++) M[i][j] = R[i][j];");
            add("}");
            add();
            add("/* wraps A so it acts about pivot (px,py): T(px,py) * A * T(-px,-py) */");
            add("void aboutPivot(double A[3][3], double px, double py) {");
            add("    double M[3][3];");
            add("    identity(M); M[0][2] = -px; M[1][2] = -py;   // T(-p)");
            add("    compose(M, A);                               // A * T(-p)");
            add("    double T[3][3];");
            add("    identity(T); T[0][2] = px; T[1][2] = py;     // T(p)");
            add("    compose(M, T);                               // T(p) * A * T(-p)");
            add("    for (int i = 0; i < 3; i++)");
            add("        for (int j = 0; j < 3; j++) A[i][j] = M[i][j];");
            add("}");
            add();
            if (need.tfKinds.translate) {
                add("void translation(double A[3][3], double tx, double ty) {");
                add("    identity(A); A[0][2] = tx; A[1][2] = ty;");
                add("}");
                add();
            }
            if (need.tfKinds.scale) {
                add("void scaling(double A[3][3], double sx, double sy, double px, double py) {");
                add("    identity(A); A[0][0] = sx; A[1][1] = sy;");
                add("    aboutPivot(A, px, py);");
                add("}");
                add();
            }
            if (need.tfKinds.rotate) {
                add("void rotation(double A[3][3], double deg, double px, double py) {   // counter-clockwise");
                add("    double t = deg * PI / 180.0;");
                add("    identity(A);");
                add("    A[0][0] = cos(t); A[0][1] = -sin(t);");
                add("    A[1][0] = sin(t); A[1][1] = cos(t);");
                add("    aboutPivot(A, px, py);");
                add("}");
                add();
            }
            if (need.tfKinds.shear) {
                add("void shearing(double A[3][3], double shx, double shy, double px, double py) {");
                add("    identity(A); A[0][1] = shx; A[1][0] = shy;   // x' = x + shx*y, y' = y + shy*x");
                add("    aboutPivot(A, px, py);");
                add("}");
                add();
            }
            if (need.tfKinds.reflect) {
                add("/* axis: 0 = x-axis, 1 = y-axis, 2 = origin, 3 = line y = x, 4 = line y = -x */");
                add("void reflection(double A[3][3], int axis) {");
                add("    identity(A);");
                add("    if (axis == 0) A[1][1] = -1;");
                add("    else if (axis == 1) A[0][0] = -1;");
                add("    else if (axis == 2) { A[0][0] = -1; A[1][1] = -1; }");
                add("    else if (axis == 3) { A[0][0] = 0; A[0][1] = 1; A[1][0] = 1; A[1][1] = 0; }");
                add("    else if (axis == 4) { A[0][0] = 0; A[0][1] = -1; A[1][0] = -1; A[1][1] = 0; }");
                add("}");
                add();
                add("/* reflection about the line through (x1,y1) and (x2,y2):");
                add("   translate to origin, rotate line onto x-axis, reflect, undo. */");
                add("void reflectionLine(double A[3][3], double x1, double y1, double x2, double y2) {");
                add("    double t = atan2(y2 - y1, x2 - x1);");
                add("    double c = cos(2 * t), s = sin(2 * t);");
                add("    identity(A);");
                add("    A[0][0] = c; A[0][1] = s;");
                add("    A[1][0] = s; A[1][1] = -c;");
                add("    aboutPivot(A, x1, y1);");
                add("}");
                add();
            }
            add("void transformPoints(double M[3][3], double in[][2], double outPts[][2], int n) {");
            add("    for (int i = 0; i < n; i++) {");
            add("        double x = in[i][0], y = in[i][1];");
            add("        outPts[i][0] = M[0][0] * x + M[0][1] * y + M[0][2];");
            add("        outPts[i][1] = M[1][0] * x + M[1][1] * y + M[1][2];");
            add("    }");
            add("}");
            add();
            add("void printMatrix(const char* name, double M[3][3]) {");
            add("    printf(\"%s =\\n\", name);");
            add("    for (int i = 0; i < 3; i++) printf(\"  [ %8.3f %8.3f %8.3f ]\\n\", M[i][0], M[i][1], M[i][2]);");
            add("}");
            add();
            add("void printPoints(const char* name, double v[][2], int n) {");
            add("    printf(\"%s:\", name);");
            add("    for (int i = 0; i < n; i++) printf(\" (%.1f, %.1f)\", v[i][0], v[i][1]);");
            add("    printf(\"\\n\");");
            add("}");
            add();
        }

        /* ---------- display(): one block per item ---------- */
        const body = [];          // lines of display()
        const decl = [];          // array declarations at the top of display()
        const B = (s) => body.push("    " + s);
        const guards = {};        // item index -> required click count
        const arrName = {};

        function colorLine(it) {
            const c = rgb(it.color || "#000000");
            return "setColor(" + c + ");";
        }
        function sizeLine(it) {
            return "setSize(" + Math.max(1, Math.round(num(it.thick, 1))) + ");";
        }
        function styleLine(it) {
            return need.style ? "styleNow = " + (STYLE_IDS[it.style] || 0) + ";   // " + (it.style || "solid") : null;
        }
        function wrap(i, lines) {
            const g = guards[i];
            if (mouse && g > 0) {
                B("if (clicks >= " + g + ") {");
                lines.forEach(l => B("    " + l));
                B("}");
            }
            else lines.forEach(l => B(l));
        }

        items.forEach((it, i) => {
            const k = i + 1;
            const name = PREFIX[it.type] + k;
            const label = (TYPE_LABEL[it.type] || it.type) + " " + k;
            ctx.begin(label);
            const L = [];
            const comment = [];
            const t = it.type;

            if (errors.indexOf(i) >= 0) {
                body.push("    // " + k + ". " + (TYPE_LABEL[t] || t) + ": skipped - it must refer to an earlier polygon/line item");
                ctx.end();
                body.push("");
                return;
            }

            if (t === "line") {
                const fn = LINE_FN[it.algo || "bres"];
                const p1 = ctx.pt(name, "x1", "y1", it.x1, it.y1, label + ": first end point");
                const p2 = ctx.pt(name, "x2", "y2", it.x2, it.y2, label + ": second end point");
                comment.push(k + ". Line (" + LINE_NAME[it.algo || "bres"] + ")");
                if (named[i]) {
                    arrName[i] = name;
                    decl.push("double " + name + "[2][2] = { " + V(p1) + ", " + V(p2) + " };");
                }
                L.push(colorLine(it), sizeLine(it));
                const st = styleLine(it); if (st) L.push(st);
                L.push(fn + "(" + p1[0] + ", " + p1[1] + ", " + p2[0] + ", " + p2[1] + ");");
            }
            else if (t === "circle" || t === "arc") {
                const c = ctx.pt(name, "xc", "yc", it.cx, it.cy, label + ": centre");
                let r;
                if (mouse) {
                    const e = ctx.pt(name, "ex", "ey", 0, 0, label + ": a point on the " + (t === "arc" ? "arc's circle" : "circle"));
                    r = "(int)floor(sqrt(pow(" + e[0] + " - " + c[0] + ", 2.0) + pow(" + e[1] + " - " + c[1] + ", 2.0)) + 0.5)";
                }
                else r = ctx.n(name + "_r", "r", it.r);
                L.push(colorLine(it), sizeLine(it));
                const st = styleLine(it); if (st) L.push(st);
                if (t === "circle") {
                    comment.push(k + ". Circle (midpoint)");
                    L.push("midpointCircle(" + c[0] + ", " + c[1] + ", " + r + ");");
                }
                else {
                    comment.push(k + ". Arc (midpoint circle, " + lit(it.a0) + " to " + lit(it.a1) + " degrees)");
                    const a0 = ctx.n(name + "_start", "start angle", it.a0, true);
                    const a1 = ctx.n(name + "_end", "end angle", it.a1, true);
                    L.push("midpointArc(" + c[0] + ", " + c[1] + ", " + r + ", " + a0 + ", " + a1 + ");");
                }
            }
            else if (t === "ellipse") {
                const c = ctx.pt(name, "xc", "yc", it.cx, it.cy, label + ": centre");
                let rx, ry;
                if (mouse) {
                    const e = ctx.pt(name, "ex", "ey", 0, 0, label + ": corner of its bounding box");
                    rx = "abs(" + e[0] + " - " + c[0] + ")"; ry = "abs(" + e[1] + " - " + c[1] + ")";
                }
                else { rx = ctx.n(name + "_rx", "rx", it.rx); ry = ctx.n(name + "_ry", "ry", it.ry); }
                comment.push(k + ". Ellipse (midpoint)");
                L.push(colorLine(it), sizeLine(it));
                const st = styleLine(it); if (st) L.push(st);
                L.push("midpointEllipse(" + c[0] + ", " + c[1] + ", " + rx + ", " + ry + ");");
            }
            else if (isPoly(t)) {
                const fn = LINE_FN[it.algo || "bres"];
                const n = vertexCount(it);
                arrName[i] = name;
                if (t === "rect") {
                    const a = ctx.pt(name, "x1", "y1", it.x1, it.y1, label + ": first corner");
                    const b = ctx.pt(name, "x2", "y2", it.x2, it.y2, label + ": opposite corner");
                    decl.push("double " + name + "[4][2] = { " + V([a[0], a[1]]) + ", " + V([b[0], a[1]]) + ", " +
                        V([b[0], b[1]]) + ", " + V([a[0], b[1]]) + " };");
                }
                else if (t === "triangle") {
                    const ps = [1, 2, 3].map(j => ctx.pt(name, "x" + j, "y" + j, it["x" + j], it["y" + j], label + ": vertex " + j));
                    decl.push("double " + name + "[3][2] = { " + ps.map(V).join(", ") + " };");
                }
                else if (t === "polygon") {
                    const pts = parsePts(it.pts);
                    if (pts.length < 3) {
                        body.push("    // " + k + ". Polygon: skipped - needs at least 3 points");
                        ctx.end(); body.push(""); return;
                    }
                    const ps = pts.map((p, j) => ctx.pt(name, "x" + (j + 1), "y" + (j + 1), p[0], p[1], label + ": vertex " + (j + 1)));
                    decl.push("double " + name + "[" + n + "][2] = { " + ps.map(V).join(", ") + " };");
                }
                else {
                    const c = ctx.pt(name, "xc", "yc", it.cx, it.cy, label + ": centre");
                    let r, rot;
                    if (mouse) {
                        const e = ctx.pt(name, "vx", "vy", 0, 0, label + ": first vertex");
                        r = "sqrt(pow(" + e[0] + " - " + c[0] + ", 2.0) + pow(" + e[1] + " - " + c[1] + ", 2.0))";
                        rot = "atan2((double)(" + e[1] + " - " + c[1] + "), (double)(" + e[0] + " - " + c[0] + ")) * 180.0 / PI";
                    }
                    else { r = ctx.n(name + "_r", "r", it.r); rot = ctx.n(name + "_rot", "rotation", it.rot || 0, true); }
                    decl.push("double " + name + "[" + n + "][2];");
                    decl.push("regularPolygon(" + c[0] + ", " + c[1] + ", " + r + ", " + n + ", " + rot + ", " + name + ");");
                }
                comment.push(k + ". " + TYPE_LABEL[t] + " (" + LINE_NAME[it.algo || "bres"] + " edges)");
                L.push(colorLine(it), sizeLine(it));
                const st = styleLine(it); if (st) L.push(st);
                L.push("drawPolygon(" + name + ", " + n + ", " + fn + ");");
            }
            else if (t === "fill") {
                const fa = it.fillAlgo || "b4";
                const s = ctx.pt(name, "x", "y", it.x, it.y, label + ": seed point inside the shape");
                comment.push(k + ". " + FILL_NAME[fa] + " from seed");
                const fname = (fa[0] === "b" ? "boundaryFill" : "floodFill") + (fa.endsWith("8") ? 8 : 4);
                L.push("{");
                L.push("    float fillColor[3] = { " + rgb(it.fillColor || "#ff0000") + " };");
                if (fa[0] === "b") {
                    L.push("    float borderColor[3] = { " + rgb(it.borderColor || "#000000") + " };");
                    L.push("    " + fname + "(" + s[0] + ", " + s[1] + ", fillColor, borderColor);");
                }
                else L.push("    " + fname + "Start(" + s[0] + ", " + s[1] + ", fillColor);");
                L.push("    glFlush();");
                L.push("}");
            }
            else if (t === "scanfill") {
                const r = Math.round(num(it.ref)) - 1;
                const ref = items[r];
                comment.push(k + ". Scan-line fill of item " + (r + 1));
                L.push("{");
                L.push("    float fillColor[3] = { " + rgb(it.fillColor || "#ff0000") + " };");
                L.push("    scanlineFill(" + arrName[r] + ", " + vertexCount(ref) + ", fillColor);");
                L.push("    " + colorLine(ref) + "   // redraw the outline on top");
                L.push("    " + sizeLine(ref));
                if (need.style) L.push("    styleNow = " + (STYLE_IDS[ref.style] || 0) + ";");
                L.push("    drawPolygon(" + arrName[r] + ", " + vertexCount(ref) + ", " + LINE_FN[ref.algo || "bres"] + ");");
                L.push("}");
                guards[i] = guards[r];
            }
            else if (t === "transform") {
                const r = Math.round(num(it.ref)) - 1;
                const ref = items[r];
                const n = vertexCount(ref);
                const ops = (it.ops || []).filter(op => op && op.type);
                comment.push(k + ". Transformation of item " + (r + 1) + ": " +
                    (ops.map(op => op.type).join(" then ") || "none"));
                L.push("{");
                L.push("    double M[3][3], A[3][3];");
                L.push("    identity(M);");
                ops.forEach((op, j) => {
                    const q = name + "_" + (j + 1);
                    const px = () => ctx.n(q + "_px", "px", op.px || 0, true);
                    const py = () => ctx.n(q + "_py", "py", op.py || 0, true);
                    let line = null;
                    if (op.type === "translate") {
                        line = "translation(A, " + ctx.n(q + "_tx", "tx", op.tx, true) + ", " + ctx.n(q + "_ty", "ty", op.ty, true) + ");";
                    }
                    else if (op.type === "scale") {
                        const a = ctx.n(q + "_sx", "sx", op.sx === undefined ? 1 : op.sx, true);
                        const b = ctx.n(q + "_sy", "sy", op.sy === undefined ? 1 : op.sy, true);
                        line = "scaling(A, " + a + ", " + b + ", " + px() + ", " + py() + ");";
                    }
                    else if (op.type === "rotate") {
                        const a = ctx.n(q + "_deg", "angle", op.deg, true);
                        line = "rotation(A, " + a + ", " + px() + ", " + py() + ");";
                    }
                    else if (op.type === "shear") {
                        const a = ctx.n(q + "_shx", "shx", op.shx, true);
                        const b = ctx.n(q + "_shy", "shy", op.shy, true);
                        line = "shearing(A, " + a + ", " + b + ", " + px() + ", " + py() + ");";
                    }
                    else if (op.type === "reflect") {
                        const ax = op.axis || "x";
                        const id = { x: 0, y: 1, origin: 2, yx: 3, ynx: 4 }[ax];
                        if (ax === "line") {
                            line = "reflectionLine(A, " + ctx.n(q + "_x1", "x1", op.x1, true) + ", " + ctx.n(q + "_y1", "y1", op.y1, true) +
                                ", " + ctx.n(q + "_x2", "x2", op.x2, true) + ", " + ctx.n(q + "_y2", "y2", op.y2, true) + ");";
                        }
                        else line = "reflection(A, " + (id === undefined ? 0 : id) + ");   // " +
                            { x: "x-axis", y: "y-axis", origin: "origin", yx: "line y = x", ynx: "line y = -x" }[ax];
                    }
                    if (line) { L.push("    " + line); L.push("    compose(M, A);"); }
                });
                L.push("    double result[" + n + "][2];");
                L.push("    transformPoints(M, " + arrName[r] + ", result, " + n + ");");
                L.push("    " + colorLine(it));
                L.push("    " + sizeLine(ref));
                if (need.style) L.push("    styleNow = " + (STYLE_IDS[ref.style] || 0) + ";");
                L.push("    drawPolygon(result, " + n + ", " + LINE_FN[ref.algo || "bres"] + ");");
                L.push("    static bool printed = false;     // print once, not on every redraw");
                L.push("    if (!printed) {");
                L.push("        printf(\"\\n--- " + label + " ---\\n\");");
                L.push("        printPoints(\"Original \", " + arrName[r] + ", " + n + ");");
                L.push("        printMatrix(\"Composite matrix M\", M);");
                L.push("        printPoints(\"Transformed\", result, " + n + ");");
                L.push("        printed = true;");
                L.push("    }");
                L.push("}");
                guards[i] = guards[r];
            }
            const g = ctx.end();
            if (g && g.maxClick >= 0) guards[i] = g.maxClick + 1;
            body.push("    // " + comment.join(" "));
            wrap(i, L);
            body.push("");
        });
        while (body.length && body[body.length - 1] === "") body.pop();

        /* ---------- globals for console / mouse input ---------- */
        if (ctx.mode === "console" && ctx.consoleVars.length) {
            add("/* Values typed in by the user (read in main) */");
            ctx.consoleVars.forEach(v => add(v));
            add();
        }
        if (mouse) {
            const nClicks = ctx.clickLabels.length;
            add("/* Points clicked with the mouse, in order */");
            add("const int NEEDED = " + nClicks + ";");
            add("int clickX[" + Math.max(1, nClicks) + "], clickY[" + Math.max(1, nClicks) + "];");
            add("int clicks = 0;");
            add("const char* clickPrompt[" + Math.max(1, nClicks) + "] = {");
            (nClicks ? ctx.clickLabels : ["(nothing to click)"]).forEach((l, j, a) =>
                add("    \"" + l.replace(/"/g, "'") + "\"" + (j < a.length - 1 ? "," : "")));
            add("};");
            add();
            add("void showPrompt() {");
            add("    char buf[160];");
            add("    if (clicks < NEEDED) sprintf(buf, \"Click %d/%d: %s\", clicks + 1, NEEDED, clickPrompt[clicks]);");
            add("    else sprintf(buf, \"Done - press R to start again\");");
            add("    glutSetWindowTitle(buf);");
            add("    printf(\"%s\\n\", buf);");
            add("}");
            add();
        }

        /* ---------- axes ---------- */
        if (cfg.axes !== false || cfg.grid) {
            add("/* Axes" + (cfg.grid ? " and grid" : "") + " (drawn last so they never block a fill) */");
            add("void drawAxes() {");
            if (cfg.grid) {
                add("    glColor3f(0.85f, 0.85f, 0.85f);");
                add("    glBegin(GL_LINES);");
                add("    for (int x = -OX; x <= W - OX; x += 50) if (x % 50 == 0) { glVertex2i(x, -OY); glVertex2i(x, H - OY); }");
                add("    for (int y = -OY; y <= H - OY; y += 50) if (y % 50 == 0) { glVertex2i(-OX, y); glVertex2i(W - OX, y); }");
                add("    glEnd();");
            }
            if (cfg.axes !== false) {
                add("    glColor3f(0.5f, 0.5f, 0.5f);");
                add("    glBegin(GL_LINES);");
                add("    glVertex2i(-OX, 0); glVertex2i(W - OX, 0);   // x-axis");
                add("    glVertex2i(0, -OY); glVertex2i(0, H - OY);   // y-axis");
                add("    glEnd();");
            }
            add("}");
            add();
        }

        add("void display() {");
        add("    glClear(GL_COLOR_BUFFER_BIT);");
        if (need.pixelGrid && !useReadPixels) add("    clearScreenBuf();");
        if (anyFill) add("    fillCount = 0;");
        if (decl.length) {
            add();
            add("    // vertex lists (x, y) of the polygons");
            decl.forEach(d => add("    " + d));
        }
        add();
        body.forEach(l => add(l));
        add();
        if (cfg.axes !== false || cfg.grid) add("    drawAxes();");
        add("    glFlush();");
        add("}");
        add();

        /* ---------- input callbacks ---------- */
        if (mouse) {
            add("void mouse(int button, int state, int mx, int my) {");
            add("    if (button != GLUT_LEFT_BUTTON || state != GLUT_DOWN || clicks >= NEEDED) return;");
            add("    clickX[clicks] = mx - OX;              // window pixels -> our coordinates");
            add("    clickY[clicks] = (H - my) - OY;        // window y grows downward");
            add("    printf(\"  point %d = (%d, %d)\\n\", clicks + 1, clickX[clicks], clickY[clicks]);");
            add("    clicks++;");
            add("    showPrompt();");
            add("    glutPostRedisplay();");
            add("}");
            add();
        }
        add("void keyboard(unsigned char key, int, int) {");
        if (mouse) add("    if (key == 'r' || key == 'R') { clicks = 0; showPrompt(); glutPostRedisplay(); }");
        add("    if (key == 27) exit(0);                // Esc quits");
        add("}");
        add();
        add("void reshape(int w, int h) {");
        add("    if (w != W || h != H) glutReshapeWindow(W, H);   // keep pixel = unit");
        add("    glViewport(0, 0, W, H);");
        add("}");
        add();
        add("void init() {");
        add("    glClearColor(" + rgb(cfg.bg || "#ffffff") + ", 1.0f);");
        add("    glMatrixMode(GL_PROJECTION);");
        add("    glLoadIdentity();");
        add("    gluOrtho2D(-OX, W - OX, -OY, H - OY);   // 1 unit = 1 pixel");
        add("    glPointSize(1);");
        add("}");
        add();
        add("int main(int argc, char** argv) {");
        if (ctx.mode === "console" && ctx.consoleGroups.length) {
            ctx.consoleGroups.forEach(g => {
                add("    cout << \"" + g.prompt + " - enter " + g.fields.join(" ") + ": \";");
                add("    cin >> " + g.vars.join(" >> ") + ";");
            });
            add();
        }
        add("    glutInit(&argc, argv);");
        add("    glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB);");
        add("    glutInitWindowSize(W, H);");
        add("    glutInitWindowPosition(100, 60);");
        add("    glutCreateWindow(\"" + String(cfg.title || "Computer Graphics Practical").replace(/"/g, "'") + "\");");
        add("    init();");
        add("    glutDisplayFunc(display);");
        add("    glutReshapeFunc(reshape);");
        add("    glutKeyboardFunc(keyboard);");
        if (mouse) {
            add("    glutMouseFunc(mouse);");
            add("    showPrompt();");
        }
        add("    glutMainLoop();");
        add("    return 0;");
        add("}");
        return out.join("\n") + "\n";
    }

    const api = { generate, parsePts, vertexCount, TYPE_LABEL, LINE_NAME, FILL_NAME, isPoly };
    if (typeof module !== "undefined" && module.exports) module.exports = api;
    else root.GlutGen = api;
})(typeof window !== "undefined" ? window : this);
