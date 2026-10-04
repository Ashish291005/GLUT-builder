
/* ===========================================================================
   5.  PIXEL PLOTTING AND ALGORITHMS
   =========================================================================== */
float plotThickness = 1.0f;
unsigned int plotPattern = 0xFFFF;
int plotBits = 16, plotScale = 3;
#if F_ELLIPSE
float plotAngle = 0.0f;       /* rotation of the ellipse being drawn */
float currentAngle = 0.0f;    /* rotation for the next ellipse */
#endif

/* is pixel number i of the current line/curve "on" in the pattern? */
bool patternOn(int i) {
    int idx = (i / plotScale) % plotBits;
    int bitPos = plotBits - 1 - idx;
    return ((plotPattern >> bitPos) & 1) != 0;
}
#if F_ANIM || F_FILL

/* When recording, plotted pixels are collected instead of drawn
   (used by the drawing animation and to find fill boundaries). */
bool animRecording = false;
std::vector<std::pair<float, float>> animRecordBuf;
#endif

void plotPixel(float wx, float wy) {
#if F_ANIM || F_FILL
    if (animRecording) { animRecordBuf.push_back(std::make_pair(wx, wy)); return; }
#endif
    if (viewScale <= 1.0f) {
        glVertex2f(sx(wx), sy(wy));
    } else {
        float cx = sx(wx), cy = sy(wy);
        float h = (plotThickness * viewScale) / 2.0f;
        if (h < 0.5f) h = 0.5f;
        glVertex2f(cx - h, cy - h);
        glVertex2f(cx + h, cy - h);
        glVertex2f(cx + h, cy + h);
        glVertex2f(cx - h, cy + h);
    }
}
#if F_DDA

/* Simple DDA: steps = max(|dx|, |dy|); add dx/steps and dy/steps each step. */
void simpleDDA(float xa, float ya, float xb, float yb) {
    float dx = xb - xa, dy = yb - ya;
    /* ceil: with fractional endpoints, truncating would make each step
       slightly longer than one pixel and leave gaps in the line */
    int steps = (int)ceilf(std::max(std::fabs(dx), std::fabs(dy)));
    if (steps == 0) { if (patternOn(0)) plotPixel(xa, ya); return; }
    float xinc = dx / steps, yinc = dy / steps, x = xa, y = ya;
    for (int i = 0; i <= steps; i++) {
        if (patternOn(i)) plotPixel(x, y);
        x += xinc; y += yinc;
    }
}
#endif
#if F_SDDA

/* Symmetric DDA: n = 2^k >= max(|dx|, |dy|); add dx/n and dy/n each step. */
void symmetricDDA(float xa, float ya, float xb, float yb) {
    float dx = xb - xa, dy = yb - ya;
    float maxVal = std::max(std::fabs(dx), std::fabs(dy));
    if (maxVal == 0) { if (patternOn(0)) plotPixel(xa, ya); return; }
    int n = 1; while (n < maxVal) n = n << 1;
    float xinc = dx / n, yinc = dy / n, x = xa, y = ya;
    for (int i = 0; i <= n; i++) {
        if (patternOn(i)) plotPixel(x, y);
        x += xinc; y += yinc;
    }
}
#endif
#if F_BRES

/* Bresenham: integer error term decides when the minor axis steps. */
void bresenham(float fxa, float fya, float fxb, float fyb) {
    int x0 = (int)floor(fxa + 0.5f), y0 = (int)floor(fya + 0.5f);
    int x1 = (int)floor(fxb + 0.5f), y1 = (int)floor(fyb + 0.5f);
    int dx = abs(x1 - x0), dy = abs(y1 - y0);
    int sxs = (x0 < x1) ? 1 : -1, sys = (y0 < y1) ? 1 : -1;
    int err = dx - dy, i = 0;
    while (true) {
        if (patternOn(i)) plotPixel((float)x0, (float)y0);
        i++;
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sxs; }
        if (e2 < dx) { err += dx; y0 += sys; }
    }
}
#endif
#if F_CIRCLE

/* Midpoint circle: one octant is computed, the other 7 are mirrored. */
int plotOctant(float cx, float cy, int x, int y, int i) {
    int n = 0;
    struct { int dx, dy; } oct[8] = {
        { x,  y}, {-x,  y}, { x, -y}, {-x, -y},
        { y,  x}, {-y,  x}, { y, -x}, {-y, -x}
    };
    if (!patternOn(i)) return 0;
    for (int k = 0; k < 8; k++) {
        if (k > 0) {
            bool dup = false;
            for (int j = 0; j < k; j++)
                if (oct[j].dx == oct[k].dx && oct[j].dy == oct[k].dy) { dup = true; break; }
            if (dup) continue;
        }
        plotPixel(cx + oct[k].dx, cy + oct[k].dy);
        n++;
    }
    return n;
}
void midpointCircle(float cx, float cy, float ex, float ey) {
    int r = (int)floor(sqrtf((ex - cx) * (ex - cx) + (ey - cy) * (ey - cy)) + 0.5f);
    if (r <= 0) { if (patternOn(0)) plotPixel(cx, cy); return; }
    int x = 0, y = r;
    int p = 1 - r;
    int i = 0;
    plotOctant(cx, cy, x, y, i++);
    while (x < y) {
        x++;
        if (p < 0) p += 2 * x + 1;
        else { y--; p += 2 * (x - y) + 1; }
        plotOctant(cx, cy, x, y, i++);
    }
}
#endif
#if F_ELLIPSE

/* Midpoint ellipse: region 1 steps in x, region 2 steps in y; each point is
   mirrored to the 4 quadrants (and rotated if the ellipse is tilted). */
void plotRotated(float cx, float cy, float px, float py) {
    if (plotAngle == 0.0f) { plotPixel(cx + px, cy + py); return; }
    float cosA = cosf(plotAngle), sinA = sinf(plotAngle);
    plotPixel(cx + px * cosA - py * sinA, cy + px * sinA + py * cosA);
}
void plotQuadrant(float cx, float cy, int x, int y, int i) {
    if (!patternOn(i)) return;
    plotRotated(cx, cy, (float)x, (float)y);
    if (x != 0) plotRotated(cx, cy, (float)-x, (float)y);
    if (y != 0) plotRotated(cx, cy, (float)x, (float)-y);
    if (x != 0 && y != 0) plotRotated(cx, cy, (float)-x, (float)-y);
}
void midpointEllipse(float cx, float cy, float px, float py) {
    int rx = (int)floor(fabs(px - cx) + 0.5f);
    int ry = (int)floor(fabs(py - cy) + 0.5f);
    if (rx <= 0 || ry <= 0) { if (patternOn(0)) plotPixel(cx, cy); return; }

    long long rx2 = (long long)rx * rx, ry2 = (long long)ry * ry;
    int x = 0, y = ry;
    long long dx = 2 * ry2 * x, dy = 2 * rx2 * y;
    int i = 0;

    double p1 = ry2 - rx2 * ry + 0.25 * rx2;
    while (dx < dy) {
        plotQuadrant(cx, cy, x, y, i++);
        x++; dx += 2 * ry2;
        if (p1 < 0) p1 += ry2 + dx;
        else { y--; dy -= 2 * rx2; p1 += ry2 + dx - dy; }
    }
    double p2 = ry2 * (x + 0.5) * (x + 0.5) + rx2 * (y - 1) * (y - 1) - rx2 * ry2;
    while (y >= 0) {
        plotQuadrant(cx, cy, x, y, i++);
        y--; dy -= 2 * rx2;
        if (p2 > 0) p2 += rx2 - dy;
        else { x++; dx += 2 * ry2; p2 += rx2 - dy + dx; }
    }
}
#endif

/* Runs algorithm a between (xa,ya) and (xb,yb). */
void runAlgorithm(int a, float xa, float ya, float xb, float yb) {
#if F_CIRCLE
    if (a == ALGO_MID_CIRCLE)  { midpointCircle(xa, ya, xb, yb); return; }
#endif
#if F_ELLIPSE
    if (a == ALGO_MID_ELLIPSE) { midpointEllipse(xa, ya, xb, yb); return; }
#endif
#if F_LINEALGO
#if F_DDA
    if (a == ALGO_SIMPLE)      { simpleDDA(xa, ya, xb, yb); return; }
#endif
#if F_SDDA
    if (a == ALGO_SYMMETRIC)   { symmetricDDA(xa, ya, xb, yb); return; }
#endif
#if F_BRES
    bresenham(xa, ya, xb, yb);
#elif F_SDDA
    symmetricDDA(xa, ya, xb, yb);
#elif F_DDA
    simpleDDA(xa, ya, xb, yb);
#endif
#endif
    (void)a; (void)xa; (void)ya; (void)xb; (void)yb;
}

/* Draws one shape with its own algorithm.
#if F_POLY
   Polygon-family shapes: one line-algorithm call per edge.
#endif
*/
void traceShape(const LineObj& L) {
#if F_POLY
    if (isPolyShape(L.shape)) {
        size_t n = L.pts.size();
        if (n == 0) return;
        if (n == 1) { runAlgorithm(L.algo, L.pts[0].x, L.pts[0].y, L.pts[0].x, L.pts[0].y); return; }
        for (size_t k = 0; k < n; k++) {
            const Pt& a = L.pts[k];
            const Pt& b = L.pts[(k + 1) % n];
            runAlgorithm(L.algo, a.x, a.y, b.x, b.y);
        }
        return;
    }
#endif
#if F_ELLIPSE
    float savedAngle = plotAngle;
    plotAngle = (L.shape == SHAPE_ELLIPSE) ? L.angle : 0.0f;
#endif
    runAlgorithm(L.algo, L.x0, L.y0, L.x1, L.y1);
#if F_ELLIPSE
    plotAngle = savedAngle;
#endif
}

void beginPlot() {
    if (viewScale <= 1.0f) { glPointSize(plotThickness); glBegin(GL_POINTS); }
    else                   glBegin(GL_QUADS);
}
void endPlot() {
    glEnd();
    glPointSize(1.0f);
}
void traceShapeBatched(const LineObj& L) {
    beginPlot();
    traceShape(L);
    endPlot();
}

/* Constant-size (screen pixels) square marker, independent of zoom. */
void drawMarker(float wx, float wy, float sizePx) {
    float cx = sx(wx), cy = sy(wy), h = sizePx / 2.0f;
    rect(cx - h, cy - h, sizePx, sizePx);
}
#if F_LINE

/* number of steps the line algorithm takes (shown in the status log) */
int stepCountOf(int a, float xa, float ya, float xb, float yb) {
    float m = std::max(std::fabs(xb - xa), std::fabs(yb - ya));
#if F_SDDA
    if (a == ALGO_SYMMETRIC) { int n = 1; while (n < m) n <<= 1; return n; }
#endif
    (void)a;
    return (int)m;
}
#endif
#if F_ANIM

/* ===========================================================================
   5b. ANIMATED REVEAL
   =========================================================================== */
bool animateShapes = true;
const int ANIM_TICK_MS = 16;
const int ANIM_FRAMES = 36;

int animObjIndex = -1;
std::vector<std::pair<float, float>> animPoints;
size_t animRevealCount = 0;
size_t animStepPerTick = 1;

std::vector<std::pair<float, float>> captureShapePoints(const LineObj& L) {
    unsigned int savedPattern = plotPattern;
    int savedBits = plotBits, savedScale = plotScale;
    plotPattern = L.pattern; plotBits = L.patternBits; plotScale = L.patternScale;
    animRecording = true; animRecordBuf.clear();
    traceShape(L);
    animRecording = false;
    plotPattern = savedPattern; plotBits = savedBits; plotScale = savedScale;
    std::vector<std::pair<float, float>> result = animRecordBuf;
    animRecordBuf.clear();
    return result;
}

void animTick(int) {
    if (animObjIndex < 0) return;
    animRevealCount += animStepPerTick;
    if (animRevealCount >= animPoints.size()) {
        animRevealCount = animPoints.size();
        animObjIndex = -1;
    }
    glutPostRedisplay();
    if (animObjIndex >= 0) glutTimerFunc(ANIM_TICK_MS, animTick, 0);
}

void startAnimation(int idx, const std::vector<std::pair<float, float>>& pts) {
    animObjIndex = idx;
    animPoints = pts;
    animRevealCount = 0;
    size_t total = std::max((size_t)1, pts.size());
    animStepPerTick = std::max((size_t)1, total / (size_t)ANIM_FRAMES);
    glutTimerFunc(ANIM_TICK_MS, animTick, 0);
}
#endif
#if F_FILE

/* ===========================================================================
   6.  FILE STORAGE
   =========================================================================== */
const char* DATA_FILE = "lines.dat";

void saveLines() {
    FILE* f = fopen(DATA_FILE, "w");
    if (!f) { logMsg("Cannot write lines.dat"); return; }
    fprintf(f, "# shape x0 y0 x1 y1 algo r g b thick pattern bits scale deleted angle group npts [x y]...\n");
    for (size_t i = 0; i < lines.size(); i++) {
        const LineObj& L = lines[i];
        fprintf(f, "%d %.2f %.2f %.2f %.2f %d %d %d %d %d %u %d %d %d %.6f %d %d",
            L.shape, L.x0, L.y0, L.x1, L.y1, L.algo, L.r, L.g, L.b, L.thickness,
            L.pattern, L.patternBits, L.patternScale, L.deleted ? 1 : 0, L.angle,
            L.group, (int)L.pts.size());
        for (size_t k = 0; k < L.pts.size(); k++)
            fprintf(f, " %.2f %.2f", L.pts[k].x, L.pts[k].y);
        fprintf(f, "\n");
    }
    fclose(f);
}
#if F_RECT || F_TRIANGLE

/* Older builds stored rectangles/triangles as centre + rx/ry + angle. */
void convertLegacyPoly(LineObj& L) {
    float cx = L.x0, cy = L.y0;
    float rx = fabsf(L.x1 - L.x0), ry = fabsf(L.y1 - L.y0);
    float c = cosf(L.angle), s = sinf(L.angle);
    auto add = [&](float x, float y) { Pt p; p.x = cx + x * c - y * s; p.y = cy + x * s + y * c; L.pts.push_back(p); };
    L.pts.clear();
    if (L.shape == SHAPE_RECTANGLE) { add(rx, ry); add(-rx, ry); add(-rx, -ry); add(rx, -ry); }
    else                            { add(rx, 0); add(-rx, ry); add(-rx, -ry); }
    L.angle = 0.0f;
    syncPolyBox(L);
}
#endif

void loadLines() {
    FILE* f = fopen(DATA_FILE, "r");
    if (!f) return;
    static char buf[32768]; lines.clear();
    while (fgets(buf, sizeof(buf), f)) {
        if (buf[0] == '#' || buf[0] == '\n') continue;
        LineObj L; int del = 0, used = 0; L.angle = 0.0f;
        int got = sscanf(buf, "%d %f %f %f %f %d %d %d %d %d %u %d %d %d %f%n",
            &L.shape, &L.x0, &L.y0, &L.x1, &L.y1, &L.algo, &L.r, &L.g, &L.b,
            &L.thickness, &L.pattern, &L.patternBits, &L.patternScale, &del, &L.angle, &used);
        if (got < 14) continue;
        L.deleted = (del != 0);
        if (!shapeEnabled(L.shape)) continue;          /* shape not in this program */
        if (got == 15) {
            char* p = buf + used;
            char* end = p;
            L.group = (int)strtol(p, &end, 10);
            if (end != p) {
                p = end;
                int n = (int)strtol(p, &end, 10);
                p = end;
                for (int k = 0; k < n; k++) {
                    Pt q;
                    q.x = strtof(p, &end); if (end == p) break; p = end;
                    q.y = strtof(p, &end); if (end == p) break; p = end;
                    L.pts.push_back(q);
                }
            }
            else L.group = 0;
        }
#if F_POLY
        if (isPolyShape(L.shape)) {
            if (L.pts.empty()) {
#if F_RECT || F_TRIANGLE
                if (L.shape == SHAPE_RECTANGLE || L.shape == SHAPE_TRIANGLE) convertLegacyPoly(L);
                else continue;
#else
                continue;
#endif
            }
            syncPolyBox(L);
        }
#endif
#if F_UNDO
        L.seq = nextSeq++;
#endif
        lines.push_back(L);
    }
    fclose(f);
}
#if F_FILL

const char* FILL_FILE = "fills.dat";

void saveFills() {
    FILE* f = fopen(FILL_FILE, "w");
    if (!f) return;
    fprintf(f, "# sx sy r g b algo conn del count\n");
    for (size_t i = 0; i < fillRegions.size(); i++) {
        const FillRegion& F = fillRegions[i];
        fprintf(f, "%d %d %d %d %d %d %d %d %d\n",
            F.seedX, F.seedY, F.r, F.g, F.b, F.algorithm,
            F.connectivity, F.deleted ? 1 : 0, (int)F.pixels.size());
        for (size_t k = 0; k < F.pixels.size(); k++)
            fprintf(f, "%d %d\n", F.pixels[k].first, F.pixels[k].second);
    }
    fclose(f);
}
void loadFills() {
    FILE* f = fopen(FILL_FILE, "r");
    if (!f) return;
    fillRegions.clear();
    char buf[256];
    while (fgets(buf, sizeof(buf), f)) {
        if (buf[0] == '#' || buf[0] == '\n') continue;
        FillRegion F; int del = 0, cnt = 0;
        if (sscanf(buf, "%d %d %d %d %d %d %d %d %d",
            &F.seedX, &F.seedY, &F.r, &F.g, &F.b,
            &F.algorithm, &F.connectivity, &del, &cnt) != 9) continue;
        F.deleted = (del != 0);
        for (int k = 0; k < cnt; k++) {
            if (!fgets(buf, sizeof(buf), f)) break;
            int px, py;
            if (sscanf(buf, "%d %d", &px, &py) == 2)
                F.pixels.push_back(std::make_pair(px, py));
        }
#if F_UNDO
        F.seq = nextSeq++;
#endif
        fillRegions.push_back(F);
    }
    fclose(f);
}
#endif
#endif

/* ===========================================================================
   7.  SHAPE OPERATIONS
   =========================================================================== */
int activeCount() {
    int c = 0;
    for (size_t i = 0; i < lines.size(); i++) if (!lines[i].deleted) c++;
    return c;
}
#if F_POLYBUILD

/* Builds the vertex list for a polygon-family shape from two points.
   Corner shapes: A and B are opposite corners of the bounding box.
   Centre shapes (polygon, star): A is the centre and B is the first
   vertex, which also sets the rotation. */
std::vector<Pt> buildPolyVerts(int shape, float ax, float ay, float bx, float by) {
    std::vector<Pt> v;
    /* vertices snap to whole pixels, as in a raster lab */
    auto add = [&](float x, float y) { Pt p; p.x = floorf(x + 0.5f); p.y = floorf(y + 0.5f); v.push_back(p); };
#if F_SQUARE
    if (shape == SHAPE_SQUARE) {
        float dx = bx - ax, dy = by - ay;
        float s = std::max(fabsf(dx), fabsf(dy));
        bx = ax + (dx < 0 ? -s : s);
        by = ay + (dy < 0 ? -s : s);
    }
#endif
    float mx = (ax + bx) / 2.0f, my = (ay + by) / 2.0f;
    (void)mx; (void)my;
    switch (shape) {
#if F_RECT
    case SHAPE_RECTANGLE:
        add(ax, ay); add(bx, ay); add(bx, by); add(ax, by);
        break;
#endif
#if F_SQUARE
    case SHAPE_SQUARE:
        add(ax, ay); add(bx, ay); add(bx, by); add(ax, by);
        break;
#endif
#if F_TRIANGLE
    case SHAPE_TRIANGLE:   /* isosceles: base on A's row, apex on B's row */
        add(ax, ay); add(bx, ay); add(mx, by);
        break;
#endif
#if F_DIAMOND
    case SHAPE_DIAMOND:
        add(mx, ay); add(bx, my); add(mx, by); add(ax, my);
        break;
#endif
#if F_POLYGON
    case SHAPE_POLYGON: {
        float r = sqrtf((bx - ax) * (bx - ax) + (by - ay) * (by - ay));
        float t0 = atan2f(by - ay, bx - ax);
        for (int i = 0; i < polySides; i++) {
            float t = t0 + 2.0f * 3.14159265f * i / polySides;
            add(ax + r * cosf(t), ay + r * sinf(t));
        }
        break;
    }
#endif
#if F_STAR
    case SHAPE_STAR: {     /* points alternate between outer and inner radius */
        float r = sqrtf((bx - ax) * (bx - ax) + (by - ay) * (by - ay));
        float t0 = atan2f(by - ay, bx - ax);
        int n = starPoints * 2;
        for (int i = 0; i < n; i++) {
            float t = t0 + 2.0f * 3.14159265f * i / n;
            float rr = (i % 2) ? r * STAR_INNER : r;
            add(ax + rr * cosf(t), ay + rr * sinf(t));
        }
        break;
    }
#endif
    default: break;
    }
    return v;
}
#endif
#if F_POLY

bool polyIsDegenerate(const std::vector<Pt>& v) {
    if (v.size() < 2) return true;
    float minx = v[0].x, maxx = v[0].x, miny = v[0].y, maxy = v[0].y;
    for (size_t k = 1; k < v.size(); k++) {
        minx = std::min(minx, v[k].x); maxx = std::max(maxx, v[k].x);
        miny = std::min(miny, v[k].y); maxy = std::max(maxy, v[k].y);
    }
    return (maxx - minx) < 1.0f && (maxy - miny) < 1.0f;
}
#endif

LineObj makeShapeWithCurrentStyle(int shape) {
    LineObj L;
    L.x0 = L.y0 = L.x1 = L.y1 = 0;
    L.shape = shape;
    L.algo = currentAlgo; L.r = curR; L.g = curG; L.b = curB;
    L.thickness = curThickness; L.pattern = curPattern;
    L.patternBits = curPatternBits; L.patternScale = curPatternScale;
    L.deleted = false;
    L.angle = 0.0f;
    L.group = 0;
    return L;
}

void commitNewShape(const LineObj& L) {
    lines.push_back(L);
#if F_UNDO
    lines.back().seq = nextSeq++;
    redoStack.clear();
#endif
#if F_FILE
    saveLines();
#endif
#if F_ANIM
    int newIndex = (int)lines.size() - 1;
    if (animateShapes && L.shape != SHAPE_LINE)
        startAnimation(newIndex, captureShapePoints(L));
#endif
}
#if F_TWOPT

/* Adds a shape given by two points:
#if F_LINE
     line    : end points (xa,ya) and (xb,yb)
#endif
#if F_CIRCLE
     circle  : centre (xa,ya) and a point (xb,yb) on the circle
#endif
#if F_ELLIPSE
     ellipse : centre (xa,ya) and corner (xb,yb) of the bounding box
#endif
*/
void addLine(float xa, float ya, float xb, float yb) {
    LineObj L = makeShapeWithCurrentStyle(currentShape);
    L.x0 = xa; L.y0 = ya; L.x1 = xb; L.y1 = yb;
#if F_ELLIPSE
    L.angle = (currentShape == SHAPE_ELLIPSE) ? currentAngle : 0.0f;
#endif
    commitNewShape(L);

    char m[160];
    sprintf(m, "Added %s %d (%s)", shapeName[currentShape], (int)lines.size(), algoName[currentAlgo]);
    logMsg(m);
#if F_LINE
    if (currentShape == SHAPE_LINE) {
        sprintf(m, "  (%d,%d) to (%d,%d)  steps/n = %d",
            (int)xa, (int)ya, (int)xb, (int)yb,
            stepCountOf(currentAlgo, xa, ya, xb, yb));
        logMsg(m);
    }
#endif
#if F_CIRCLE
    if (currentShape == SHAPE_CIRCLE) {
        int r = (int)floor(sqrtf((xb - xa) * (xb - xa) + (yb - ya) * (yb - ya)) + 0.5f);
        sprintf(m, "  centre (%d,%d)  radius %d", (int)xa, (int)ya, r);
        logMsg(m);
    }
#endif
#if F_ELLIPSE
    if (currentShape == SHAPE_ELLIPSE) {
        int rx = (int)floor(fabs(xb - xa) + 0.5f), ry = (int)floor(fabs(yb - ya) + 0.5f);
        sprintf(m, "  centre (%d,%d)  rx=%d  ry=%d", (int)xa, (int)ya, rx, ry);
        logMsg(m);
    }
#endif
}
#endif
#if F_POLY

void addPolygon(int shape, const std::vector<Pt>& verts) {
    if (verts.size() < 3 || polyIsDegenerate(verts)) { logMsg("Shape too small - not added"); return; }
    LineObj L = makeShapeWithCurrentStyle(shape);
    L.pts = verts;
    syncPolyBox(L);
    commitNewShape(L);

    char m[160];
    sprintf(m, "Added %s %d (%s edges)", shapeName[shape], (int)lines.size(), algoName[L.algo]);
    logMsg(m);
    sprintf(m, "  %d vertices  box (%d,%d)-(%d,%d)", (int)verts.size(),
        (int)L.x0, (int)L.y0, (int)L.x1, (int)L.y1);
    logMsg(m);
}
#endif
#if F_UNDO

void undo() {
    int bestShape = -1, bestSeq = -1;
    for (int i = 0; i < (int)lines.size(); i++)
        if (!lines[i].deleted && lines[i].seq > bestSeq) { bestSeq = lines[i].seq; bestShape = i; }
    char m[80];
#if F_FILL
    int bestFill = -1;
    for (int i = 0; i < (int)fillRegions.size(); i++)
        if (!fillRegions[i].deleted && fillRegions[i].seq > bestSeq) { bestSeq = fillRegions[i].seq; bestFill = i; bestShape = -1; }
    if (bestFill >= 0) {
        fillRegions[bestFill].deleted = true;
        leakAlertOn = false;
        redoStack.push_back(std::make_pair((int)ITEM_FILL, bestFill));
#if F_ANIM
        if (fillAnimIndex == bestFill) fillAnimIndex = -1;
#endif
#if F_FILE
        saveFills();
#endif
        sprintf(m, "Undo: fill %d removed", bestFill + 1);
        logMsg(m);
        return;
    }
#endif
    if (bestShape >= 0) {
        lines[bestShape].deleted = true;
        redoStack.push_back(std::make_pair((int)ITEM_SHAPE, bestShape));
#if F_SELECT
        deselect(bestShape);
#endif
#if F_ANIM
        if (animObjIndex == bestShape) animObjIndex = -1;
#endif
#if F_FILE
        saveLines();
#endif
        sprintf(m, "Undo: %s %d removed", shapeName[lines[bestShape].shape], bestShape + 1);
    }
    else sprintf(m, "Nothing to undo");
    logMsg(m);
}
void redo() {
    if (redoStack.empty()) { logMsg("Nothing to redo"); return; }
    std::pair<int, int> e = redoStack.back(); redoStack.pop_back();
    char m[80];
#if F_FILL
    if (e.first == ITEM_FILL) {
        if (e.second >= (int)fillRegions.size()) { logMsg("Nothing to redo"); return; }
        fillRegions[e.second].deleted = false;
#if F_FILE
        saveFills();
#endif
        sprintf(m, "Redo: fill %d restored", e.second + 1);
        logMsg(m);
        return;
    }
#endif
    if (e.second >= (int)lines.size()) { logMsg("Nothing to redo"); return; }
    lines[e.second].deleted = false;
#if F_FILE
    saveLines();
#endif
    sprintf(m, "Redo: shape %d restored", e.second + 1);
    logMsg(m);
}
#endif
void clearAll() {
    lines.clear();
#if F_UNDO
    redoStack.clear();
#endif
#if F_SELECT
    selection.clear();
#endif
#if F_ANIM
    animObjIndex = -1;
#endif
#if F_FILL
    fillRegions.clear(); leakAlertOn = false;
#if F_ANIM
    fillAnimIndex = -1;
#endif
#endif
#if F_VERTS
    polyVerts.clear();
#endif
    clickState = 0;
#if F_TRANSFORM
    tfConfirmActive = false; tfOldIndices.clear(); tfNewIndices.clear();
#endif
#if F_FILE
    saveLines();
#if F_FILL
    saveFills();
#endif
#endif
    logMsg("All shapes cleared");
}
#if F_SELECT || F_FILL

/* ---- geometry: distance to a shape's outline, and inside tests ---- */
float distToSegment(float px, float py, float x1, float y1, float x2, float y2) {
    float vx = x2 - x1, vy = y2 - y1;
    float len2 = vx * vx + vy * vy;
    if (len2 == 0) return sqrtf((px - x1) * (px - x1) + (py - y1) * (py - y1));
    float t = ((px - x1) * vx + (py - y1) * vy) / len2;
    t = std::min(std::max(t, 0.0f), 1.0f);
    float qx = x1 + t * vx, qy = y1 + t * vy;
    return sqrtf((px - qx) * (px - qx) + (py - qy) * (py - qy));
}
#if F_CIRCLE
float distToCircle(float px, float py, float cx, float cy, float ex, float ey) {
    float r = sqrtf((ex - cx) * (ex - cx) + (ey - cy) * (ey - cy));
    float d = sqrtf((px - cx) * (px - cx) + (py - cy) * (py - cy));
    return fabsf(d - r);
}
#endif
#if F_ELLIPSE
/* Point expressed in the (rotated) ellipse's own frame. */
void toEllipseFrame(const LineObj& L, float px, float py, float& ex, float& ey) {
    float c = cosf(-L.angle), s = sinf(-L.angle);
    float dx = px - L.x0, dy = py - L.y0;
    ex = dx * c - dy * s; ey = dx * s + dy * c;
}
float distToEllipse(const LineObj& L, float px, float py) {
    float rx = std::max(1.0f, fabsf(L.x1 - L.x0)), ry = std::max(1.0f, fabsf(L.y1 - L.y0));
    float dx, dy; toEllipseFrame(L, px, py, dx, dy);
    float t = sqrtf((dx * dx) / (rx * rx) + (dy * dy) / (ry * ry));
    if (t < 1e-6f) return std::min(rx, ry);
    float nx = dx / t, ny = dy / t;
    return sqrtf((dx - nx) * (dx - nx) + (dy - ny) * (dy - ny));
}
#endif
float distToShape(const LineObj& L, float px, float py) {
#if F_CIRCLE
    if (L.shape == SHAPE_CIRCLE)  return distToCircle(px, py, L.x0, L.y0, L.x1, L.y1);
#endif
#if F_ELLIPSE
    if (L.shape == SHAPE_ELLIPSE) return distToEllipse(L, px, py);
#endif
#if F_POLY
    if (isPolyShape(L.shape)) {
        float best = 1e30f; size_t n = L.pts.size();
        for (size_t k = 0; k < n; k++) {
            const Pt& a = L.pts[k]; const Pt& b = L.pts[(k + 1) % n];
            best = std::min(best, distToSegment(px, py, a.x, a.y, b.x, b.y));
        }
        return best;
    }
#endif
    return distToSegment(px, py, L.x0, L.y0, L.x1, L.y1);
}
/* Inside test and area for closed shapes. */
bool shapeContains(const LineObj& L, float px, float py, float& area) {
#if F_CIRCLE
    if (L.shape == SHAPE_CIRCLE) {
        float r2 = (L.x1 - L.x0) * (L.x1 - L.x0) + (L.y1 - L.y0) * (L.y1 - L.y0);
        area = 3.14159265f * r2;
        return (px - L.x0) * (px - L.x0) + (py - L.y0) * (py - L.y0) <= r2;
    }
#endif
#if F_ELLIPSE
    if (L.shape == SHAPE_ELLIPSE) {
        float rx = std::max(1.0f, fabsf(L.x1 - L.x0)), ry = std::max(1.0f, fabsf(L.y1 - L.y0));
        float dx, dy; toEllipseFrame(L, px, py, dx, dy);
        area = 3.14159265f * rx * ry;
        return (dx * dx) / (rx * rx) + (dy * dy) / (ry * ry) <= 1.0f;
    }
#endif
#if F_POLY
    if (isPolyShape(L.shape)) {          /* ray casting (even-odd rule) */
        bool in = false; float a2 = 0; size_t n = L.pts.size();
        for (size_t i = 0, j = n - 1; i < n; j = i++) {
            const Pt& a = L.pts[i]; const Pt& b = L.pts[j];
            if (((a.y > py) != (b.y > py)) && (px < (b.x - a.x) * (py - a.y) / (b.y - a.y) + a.x)) in = !in;
            a2 += (b.x * a.y - a.x * b.y);
        }
        area = fabsf(a2) / 2.0f;
        return in;
    }
#endif
    (void)px; (void)py;
    area = 0; return false;
}
#endif
#if F_SELECT || F_ZOOM

/* axis-aligned bounding box of a shape */
void shapeBBox(const LineObj& L, float& minx, float& miny, float& maxx, float& maxy) {
#if F_POLY
    if (isPolyShape(L.shape)) { minx = L.x0; miny = L.y0; maxx = L.x1; maxy = L.y1; return; }
#endif
#if F_CIRCLE
    if (L.shape == SHAPE_CIRCLE) {
        float r = sqrtf((L.x1 - L.x0) * (L.x1 - L.x0) + (L.y1 - L.y0) * (L.y1 - L.y0));
        minx = L.x0 - r; maxx = L.x0 + r; miny = L.y0 - r; maxy = L.y0 + r; return;
    }
#endif
#if F_ELLIPSE
    if (L.shape == SHAPE_ELLIPSE) {
        float rx = fabsf(L.x1 - L.x0), ry = fabsf(L.y1 - L.y0);
        float c = cosf(L.angle), s = sinf(L.angle);
        float hx = sqrtf(rx * rx * c * c + ry * ry * s * s);
        float hy = sqrtf(rx * rx * s * s + ry * ry * c * c);
        minx = L.x0 - hx; maxx = L.x0 + hx; miny = L.y0 - hy; maxy = L.y0 + hy; return;
    }
#endif
    minx = std::min(L.x0, L.x1); maxx = std::max(L.x0, L.x1);
    miny = std::min(L.y0, L.y1); maxy = std::max(L.y0, L.y1);
}
#endif
#if F_SELECT

void deleteSelected() {
    if (selection.empty()) { logMsg("No shape selected"); return; }
    int fillsRemoved = 0;
    for (size_t s = 0; s < selection.size(); s++) {
        int i = selection[s];
        if (i < 0 || i >= (int)lines.size() || lines[i].deleted) continue;
        lines[i].deleted = true;
#if F_UNDO
        redoStack.push_back(std::make_pair((int)ITEM_SHAPE, i));
#endif
#if F_ANIM
        if (animObjIndex == i) animObjIndex = -1;
#endif
#if F_FILL
        /* fills that were seeded inside a deleted shape go with it */
        for (size_t f = 0; f < fillRegions.size(); f++) {
            float area;
            if (!fillRegions[f].deleted &&
                shapeContains(lines[i], (float)fillRegions[f].seedX, (float)fillRegions[f].seedY, area)) {
                fillRegions[f].deleted = true;
#if F_UNDO
                redoStack.push_back(std::make_pair((int)ITEM_FILL, (int)f));
#endif
#if F_ANIM
                if (fillAnimIndex == (int)f) fillAnimIndex = -1;
#endif
                fillsRemoved++;
            }
        }
#endif
    }
    char m[80]; sprintf(m, "Deleted %d shape(s), %d fill(s)", (int)selection.size(), fillsRemoved); logMsg(m);
    selection.clear();
#if F_FILE
    saveLines();
#if F_FILL
    saveFills();
#endif
#endif
}

bool selectionBBox(float& minx, float& miny, float& maxx, float& maxy) {
    bool any = false;
    for (size_t s = 0; s < selection.size(); s++) {
        const LineObj& L = lines[selection[s]];
        if (L.deleted) continue;
        float a, b, c, d; shapeBBox(L, a, b, c, d);
        if (!any) { minx = a; miny = b; maxx = c; maxy = d; any = true; }
        else { minx = std::min(minx, a); miny = std::min(miny, b); maxx = std::max(maxx, c); maxy = std::max(maxy, d); }
    }
    return any;
}

/* Nearest outline within tolerance wins; otherwise the smallest closed shape
   containing the point. */
int hitTest(float wx, float wy) {
    int best = -1; float bestD = 10.0f / viewScale;
    for (size_t i = 0; i < lines.size(); i++) {
        if (lines[i].deleted) continue;
        float d = distToShape(lines[i], wx, wy);
        if (d < bestD) { bestD = d; best = (int)i; }
    }
    if (best >= 0) return best;
    float bestArea = 1e30f;
    for (size_t i = 0; i < lines.size(); i++) {
        if (lines[i].deleted) continue;
        float area;
        if (shapeContains(lines[i], wx, wy, area) && area < bestArea) { bestArea = area; best = (int)i; }
    }
    return best;
}
std::vector<int> groupMembers(int idx) {
    std::vector<int> out;
    int g = lines[idx].group;
    if (g == 0) { out.push_back(idx); return out; }
    for (size_t i = 0; i < lines.size(); i++)
        if (!lines[i].deleted && lines[i].group == g) out.push_back((int)i);
    return out;
}
void selectShape(int idx, bool additive) {
    if (idx < 0) { if (!additive) selection.clear(); return; }
    std::vector<int> mem = groupMembers(idx);
    if (additive && isSelected(idx)) {
        for (size_t k = 0; k < mem.size(); k++) deselect(mem[k]);
    }
    else {
        if (!additive) selection.clear();
        for (size_t k = 0; k < mem.size(); k++) if (!isSelected(mem[k])) selection.push_back(mem[k]);
    }
    char m[96];
    if (mem.size() > 1) sprintf(m, "Selected group of %d shapes (%d total)", (int)mem.size(), (int)selection.size());
    else sprintf(m, "Selected %s %d  (%d total)", shapeName[lines[idx].shape], idx + 1, (int)selection.size());
    logMsg(m);
}
void selectAll() {
    selection.clear();
    for (size_t i = 0; i < lines.size(); i++) if (!lines[i].deleted) selection.push_back((int)i);
    char m[64]; sprintf(m, "Selected all (%d shapes)", (int)selection.size()); logMsg(m);
}
void groupSelection() {
    if (selection.size() < 2) { logMsg("Select 2+ shapes to group (Shift+click or box drag)"); return; }
    int g = nextGroupId();
    for (size_t s = 0; s < selection.size(); s++) lines[selection[s]].group = g;
#if F_FILE
    saveLines();
#endif
    char m[64]; sprintf(m, "Grouped %d shapes (group %d)", (int)selection.size(), g); logMsg(m);
}
void ungroupSelection() {
    if (selection.empty()) { logMsg("No shape selected"); return; }
    for (size_t s = 0; s < selection.size(); s++) lines[selection[s]].group = 0;
#if F_FILE
    saveLines();
#endif
    logMsg("Ungrouped selection");
}
void translateShapeInPlace(LineObj& L, float dx, float dy) {
    L.x0 += dx; L.y0 += dy; L.x1 += dx; L.y1 += dy;
    for (size_t k = 0; k < L.pts.size(); k++) { L.pts[k].x += dx; L.pts[k].y += dy; }
}
/* Box select: every shape whose bounding box lies fully inside the box. */
void boxSelect(float wx0, float wy0, float wx1, float wy1, bool additive) {
    float minx = std::min(wx0, wx1), maxx = std::max(wx0, wx1);
    float miny = std::min(wy0, wy1), maxy = std::max(wy0, wy1);
    if (!additive) selection.clear();
    for (size_t i = 0; i < lines.size(); i++) {
        if (lines[i].deleted) continue;
        float a, b, c, d; shapeBBox(lines[i], a, b, c, d);
        if (a >= minx && c <= maxx && b >= miny && d <= maxy) {
            std::vector<int> mem = groupMembers((int)i);
            for (size_t k = 0; k < mem.size(); k++) if (!isSelected(mem[k])) selection.push_back(mem[k]);
        }
    }
    char m[64]; sprintf(m, "Box selected %d shape(s)", (int)selection.size()); logMsg(m);
}
#endif
#if F_TRANSFORM
/* where the [OLD] / [NEW] tags are drawn */
void anchorOf(const LineObj& L, float& ax, float& ay) {
    if (isPolyShape(L.shape) && !L.pts.empty()) { ax = L.pts[0].x; ay = L.pts[0].y; }
    else { ax = L.x0; ay = L.y0; }
}
#endif
