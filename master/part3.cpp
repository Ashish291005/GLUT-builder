#if F_FILL

/* ===========================================================================
   8.  FILL ALGORITHMS
   =========================================================================== */
struct PairHash {
    size_t operator()(const std::pair<int, int>& p) const {
        return std::hash<long long>()(((long long)p.first << 32) ^ (unsigned)p.second);
    }
};
typedef std::unordered_set<std::pair<int, int>, PairHash> PixelSet;
typedef std::vector<std::pair<int, int>> PixelList;

/* Runs every shape's own algorithm (solid pattern) and collects the pixels:
   these are the boundary pixels a fill must not cross. */
void rasterizeBoundaries(PixelSet& out) {
    unsigned int savedPat = plotPattern;
    int savedBits = plotBits, savedScale = plotScale;
    plotPattern = 0xFFFF; plotBits = 16; plotScale = 1;

    bool savedRec = animRecording;
    animRecording = true;

    for (size_t i = 0; i < lines.size(); i++) {
        const LineObj& L = lines[i];
        if (L.deleted) continue;
        animRecordBuf.clear();
        traceShape(L);
        for (size_t k = 0; k < animRecordBuf.size(); k++) {
            int ix = (int)floor(animRecordBuf[k].first + 0.5f);
            int iy = (int)floor(animRecordBuf[k].second + 0.5f);
            out.insert(std::make_pair(ix, iy));
        }
    }
    animRecording = savedRec;
    animRecordBuf.clear();
    plotPattern = savedPat; plotBits = savedBits; plotScale = savedScale;
}

struct Bounds { int minx = 0, miny = 0, maxx = -1, maxy = -1; };
Bounds boundsOf(const PixelSet& b, int margin) {
    Bounds r = { 0, 0, -1, -1 };
    bool first = true;
    for (PixelSet::const_iterator it = b.begin(); it != b.end(); ++it) {
        if (first) { r.minx = r.maxx = it->first; r.miny = r.maxy = it->second; first = false; }
        r.minx = std::min(r.minx, it->first);  r.maxx = std::max(r.maxx, it->first);
        r.miny = std::min(r.miny, it->second); r.maxy = std::max(r.maxy, it->second);
    }
    r.minx -= margin; r.miny -= margin; r.maxx += margin; r.maxy += margin;
    return r;
}
bool outside(const Bounds& bb, int x, int y) {
    return x < bb.minx || x > bb.maxx || y < bb.miny || y > bb.maxy;
}

/* A fill that escapes the shape is stopped this many pixels later. */
const size_t LEAK_EXTRA_PIXELS = 2500;

typedef bool (*EscapeTest)(int x, int y);
#if F_B4 || F_B8

/* Classic recursive boundary fill:

       boundaryFill(x, y):
           if (x,y) is the boundary colour or already filled: return
           colour (x,y)
           boundaryFill(x+1, y); boundaryFill(x-1, y)
           boundaryFill(x, y+1); boundaryFill(x, y-1)
#if F_B8
           [8-connected also: (x+1,y+1) (x-1,y+1) (x-1,y-1) (x+1,y-1)]
#endif

   Real recursion would overflow the call stack on large regions, so the
   call stack is kept explicitly: each frame remembers which neighbour it
   tries next. Pixels are appended to `order` exactly when the recursive
   version would colour them, so the animation shows the true depth-first
   path (one direction first, then backtracking). `aux` gets the stack
   depth at each pixel (shown live while animating).
#if F_B8

   An 8-connected fill escapes through the diagonal steps of an 8-connected
   outline (any line algorithm's output) - the classic failure.
#endif
   The first escaped pixel is recorded in `leakAt`; the fill then runs for
   LEAK_EXTRA_PIXELS more and stops, and never goes past `bb`. */
void recursiveFill(int x, int y, const PixelSet& boundary, const Bounds& bb,
    PixelList& order, std::vector<int>* aux, int connectivity, EscapeTest escaped, long& leakAt) {
    static const int dx8[8] = { 1,-1, 0, 0, 1,-1,-1, 1 };
    static const int dy8[8] = { 0, 0, 1,-1, 1, 1,-1,-1 };
    const int n = (connectivity == 8) ? 8 : 4;

    PixelSet filled;
    struct Frame { int x = 0, y = 0, next = 0; };
    std::vector<Frame> stack;
    leakAt = -1;

    auto tryVisit = [&](int px, int py) {
        std::pair<int, int> q(px, py);
        if (boundary.count(q) || filled.count(q) || outside(bb, px, py)) return;
        filled.insert(q);
        Frame f = { px, py, 0 };
        stack.push_back(f);
        order.push_back(q);
        if (aux) aux->push_back((int)stack.size());
        if (leakAt < 0 && escaped(px, py)) leakAt = (long)order.size() - 1;
    };

    tryVisit(x, y);
    while (!stack.empty()) {
        if (leakAt >= 0 && order.size() >= (size_t)leakAt + LEAK_EXTRA_PIXELS) break;
        Frame& f = stack.back();
        if (f.next >= n) { stack.pop_back(); continue; }
        int k = f.next++;
        int px = f.x + dx8[k], py = f.y + dy8[k];
        tryVisit(px, py);   /* may push: f is not used after this */
    }
}
#endif
#if F_FLOOD

/* ---- Flood fill: replace the seed's colour, spreading as a wave ----
   The colour of a pixel is the colour of the top-most fill covering it,
   or the background (-1). Outline pixels never match. */
typedef std::unordered_map<std::pair<int, int>, int, PairHash> ColourMap;

int packRGB(int r, int g, int b) { return (r << 16) | (g << 8) | b; }

ColourMap buildColourMap() {
    ColourMap cm;
    for (size_t i = 0; i < fillRegions.size(); i++) {      /* later fills paint over earlier ones */
        const FillRegion& F = fillRegions[i];
        if (F.deleted) continue;
        int c = packRGB(F.r, F.g, F.b);
        for (size_t k = 0; k < F.pixels.size(); k++) cm[F.pixels[k]] = c;
    }
    return cm;
}
int colourAt(const ColourMap& cm, int x, int y) {
    ColourMap::const_iterator it = cm.find(std::make_pair(x, y));
    return (it == cm.end()) ? -1 : it->second;
}

/*     floodFill(seed):
           old = colour(seed)
           queue <- seed
           while queue not empty:
               p = dequeue; colour p
               for each 4-neighbour q with colour(q) == old: enqueue q

   A queue (first in, first out) colours pixels in order of their distance
   from the seed, so the colour spreads outward in rings like water.
   `aux` gets the queue length at each pixel. */
void floodFill(int x, int y, const PixelSet& boundary, const ColourMap& colours, int oldColour,
    const Bounds& bb, PixelList& order, std::vector<int>* aux, EscapeTest escaped, long& leakAt) {
    static const int dx4[4] = { 1,-1, 0, 0 };
    static const int dy4[4] = { 0, 0, 1,-1 };
    PixelSet queued;
    std::vector<std::pair<int, int>> queue;
    size_t head = 0;
    leakAt = -1;

    auto matches = [&](int px, int py) {
        std::pair<int, int> q(px, py);
        return !outside(bb, px, py) && !boundary.count(q) && !queued.count(q)
            && colourAt(colours, px, py) == oldColour;
    };
    if (!matches(x, y)) return;
    queue.push_back(std::make_pair(x, y)); queued.insert(queue.back());

    while (head < queue.size()) {
        if (leakAt >= 0 && order.size() >= (size_t)leakAt + LEAK_EXTRA_PIXELS) break;
        std::pair<int, int> p = queue[head++];
        order.push_back(p);
        if (aux) aux->push_back((int)(queue.size() - head));
        if (leakAt < 0 && escaped(p.first, p.second)) leakAt = (long)order.size() - 1;
        for (int k = 0; k < 4; k++) {
            int qx = p.first + dx4[k], qy = p.second + dy4[k];
            if (matches(qx, qy)) { queue.push_back(std::make_pair(qx, qy)); queued.insert(queue.back()); }
        }
    }
}
#endif
#if F_SCAN

/* ---- Scan-line polygon fill (textbook) ----
   For each scan line y, from the top of the shape to the bottom:
     1. find the x where the line crosses the shape's edges,
     2. sort the crossings,
     3. fill between crossing pairs: (x1,x2), (x3,x4), ...
   An edge counts for y in [ymin, ymax) only, so a vertex is not counted
   twice. Circles and ellipses use their exact equations instead of edges. */
std::vector<float> scanCrossings(const LineObj& L, float y) {
    std::vector<float> xs;
#if F_POLY || F_LINE
    if (isPolyShape(L.shape)) {
        size_t n = L.pts.size();
        for (size_t i = 0; i < n; i++) {
            const Pt& a = L.pts[i]; const Pt& b = L.pts[(i + 1) % n];
            if (a.y == b.y) continue;                              /* horizontal edge */
            float ylo = std::min(a.y, b.y), yhi = std::max(a.y, b.y);
            if (y >= ylo && y < yhi) xs.push_back(a.x + (y - a.y) * (b.x - a.x) / (b.y - a.y));
        }
    }
#endif
#if F_CIRCLE
    if (L.shape == SHAPE_CIRCLE) {                                 /* (x-cx)^2 + (y-cy)^2 = r^2 */
        float r2 = (L.x1 - L.x0) * (L.x1 - L.x0) + (L.y1 - L.y0) * (L.y1 - L.y0);
        float dy = y - L.y0, d = r2 - dy * dy;
        if (d >= 0) { float h = sqrtf(d); xs.push_back(L.x0 - h); xs.push_back(L.x0 + h); }
    }
#endif
#if F_ELLIPSE
    if (L.shape == SHAPE_ELLIPSE) {
        /* rotated ellipse: solve A*x^2 + B*x + C = 0 for x (relative to the centre) */
        float rx = std::max(1.0f, fabsf(L.x1 - L.x0)), ry = std::max(1.0f, fabsf(L.y1 - L.y0));
        float c = cosf(L.angle), s = sinf(L.angle), v = y - L.y0;
        float A = c * c / (rx * rx) + s * s / (ry * ry);
        float B = 2.0f * v * c * s * (1.0f / (rx * rx) - 1.0f / (ry * ry));
        float C = v * v * (s * s / (rx * rx) + c * c / (ry * ry)) - 1.0f;
        float disc = B * B - 4 * A * C;
        if (disc >= 0) {
            float q = sqrtf(disc);
            xs.push_back(L.x0 + (-B - q) / (2 * A));
            xs.push_back(L.x0 + (-B + q) / (2 * A));
        }
    }
#endif
    std::sort(xs.begin(), xs.end());
    return xs;
}

#if F_LINE
/* Separate lines whose ends meet (within a few pixels) form a closed loop.
   The loop around the seed is returned as one polygon, so scan-line fill
   can use edge crossings even when the shape was drawn line by line. */
bool findLineLoop(int seedX, int seedY, LineObj& out) {
    std::vector<int> segs;
    for (size_t i = 0; i < lines.size(); i++)
        if (!lines[i].deleted && lines[i].shape == SHAPE_LINE) segs.push_back((int)i);
    const float tol = 8.0f / viewScale;          /* "meeting" distance, 8 screen pixels */
    auto meets = [&](float ax, float ay, float bx, float by) {
        return fabsf(ax - bx) <= tol && fabsf(ay - by) <= tol;
    };
    bool found = false;
    float bestArea = 1e30f;
    for (size_t s = 0; s < segs.size(); s++) {
        const LineObj& L0 = lines[segs[s]];
        std::vector<Pt> v;
        Pt a; a.x = L0.x0; a.y = L0.y0; v.push_back(a);
        Pt b; b.x = L0.x1; b.y = L0.y1; v.push_back(b);
        std::vector<bool> used(segs.size(), false);
        used[s] = true;
        bool closed = false;
        for (size_t step = 0; step < segs.size() && !closed; step++) {
            Pt end = v.back();
            int next = -1; Pt other;
            for (size_t k = 0; k < segs.size(); k++) {
                if (used[k]) continue;
                const LineObj& L = lines[segs[k]];
                if (meets(L.x0, L.y0, end.x, end.y)) { next = (int)k; other.x = L.x1; other.y = L.y1; break; }
                if (meets(L.x1, L.y1, end.x, end.y)) { next = (int)k; other.x = L.x0; other.y = L.y0; break; }
            }
            if (next < 0) break;                 /* chain is open */
            used[next] = true;
            if (v.size() >= 3 && meets(other.x, other.y, v[0].x, v[0].y)) closed = true;
            else v.push_back(other);
        }
        if (!closed || v.size() < 3) continue;
        /* is the seed inside this loop? (ray casting) and how big is it? */
        bool in = false; float a2 = 0;
        for (size_t i = 0, j = v.size() - 1; i < v.size(); j = i++) {
            if (((v[i].y > seedY) != (v[j].y > seedY)) &&
                (seedX < (v[j].x - v[i].x) * (seedY - v[i].y) / (v[j].y - v[i].y) + v[i].x)) in = !in;
            a2 += v[j].x * v[i].y - v[i].x * v[j].y;
        }
        float area = fabsf(a2) / 2.0f;
        if (!in || area >= bestArea) continue;
        bestArea = area; found = true;
        out = LineObj();
        out.shape = SHAPE_FREEPOLY;
        out.pts = v;
        out.x0 = out.x1 = v[0].x; out.y0 = out.y1 = v[0].y;   /* bounding box */
        for (size_t i = 1; i < v.size(); i++) {
            out.x0 = std::min(out.x0, v[i].x); out.x1 = std::max(out.x1, v[i].x);
            out.y0 = std::min(out.y0, v[i].y); out.y1 = std::max(out.y1, v[i].y);
        }
    }
    return found;
}
#endif

void scanlinePolygonFill(const LineObj& L, PixelList& order, std::vector<ScanRow>& rows) {
    float ymin = L.y0, ymax = L.y1;                                /* polygons: bounding box */
#if F_CIRCLE
    if (L.shape == SHAPE_CIRCLE) {
        float r = sqrtf((L.x1 - L.x0) * (L.x1 - L.x0) + (L.y1 - L.y0) * (L.y1 - L.y0));
        ymin = L.y0 - r; ymax = L.y0 + r;
    }
#endif
#if F_ELLIPSE
    if (L.shape == SHAPE_ELLIPSE) {
        float rx = fabsf(L.x1 - L.x0), ry = fabsf(L.y1 - L.y0);
        float c = cosf(L.angle), s = sinf(L.angle);
        float hy = sqrtf(rx * rx * s * s + ry * ry * c * c);
        ymin = L.y0 - hy; ymax = L.y0 + hy;
    }
#endif
    for (int y = (int)floorf(ymax); y >= (int)ceilf(ymin); y--) {   /* top to bottom */
        ScanRow row;
        row.y = y;
        row.start = order.size();
        row.xs = scanCrossings(L, (float)y);
        for (size_t i = 0; i + 1 < row.xs.size(); i += 2)
            for (int x = (int)ceilf(row.xs[i]); x <= (int)floorf(row.xs[i + 1]); x++)
                order.push_back(std::make_pair(x, y));
        row.end = order.size();
        rows.push_back(row);
    }
}

/* Region closed only by separate lines (no single shape around the seed):
   seed-based span fill - fill the whole run left/right of a pixel, then
   look for new runs in the rows above and below. */
void spanFill(int x, int y, const PixelSet& boundary, const Bounds& bb, PixelList& order) {
    PixelSet filled;
    auto isFree = [&](int px, int py) {
        std::pair<int, int> q(px, py);
        return !outside(bb, px, py) && !boundary.count(q) && !filled.count(q);
        };
    if (!isFree(x, y)) return;

    std::vector<std::pair<int, int>> stack;
    stack.push_back(std::make_pair(x, y));
    while (!stack.empty() && order.size() < 2000000) {
        std::pair<int, int> p = stack.back(); stack.pop_back();
        int px = p.first, py = p.second;
        if (!isFree(px, py)) continue;
        int xl = px;
        while (isFree(xl - 1, py)) xl--;
        int xr = px;
        while (isFree(xr + 1, py)) xr++;
        for (int xx = xl; xx <= xr; xx++) {
            filled.insert(std::make_pair(xx, py));
            order.push_back(std::make_pair(xx, py));
        }
        for (int ny = py - 1; ny <= py + 1; ny += 2) {
            bool inRun = false;
            for (int xx = xl; xx <= xr; xx++) {
                if (isFree(xx, ny)) {
                    if (!inRun) { stack.push_back(std::make_pair(xx, ny)); inRun = true; }
                }
                else inRun = false;
            }
        }
    }
}

/* Re-orders span-fill pixels top to bottom and builds one ScanRow per row
   (crossings = the ends of each run), so it animates like a scan-line. */
void groupIntoRows(PixelList& order, std::vector<ScanRow>& rows) {
    std::sort(order.begin(), order.end(), [](const std::pair<int, int>& a, const std::pair<int, int>& b) {
        return a.second != b.second ? a.second > b.second : a.first < b.first; });
    for (size_t i = 0; i < order.size();) {
        ScanRow row; row.y = order[i].second; row.start = i;
        size_t j = i;
        while (j < order.size() && order[j].second == row.y) {
            size_t k = j;
            while (k + 1 < order.size() && order[k + 1].second == row.y && order[k + 1].first == order[k].first + 1) k++;
            row.xs.push_back((float)order[j].first);
            row.xs.push_back((float)order[k].first);
            j = k + 1;
        }
        row.end = j;
        rows.push_back(row);
        i = j;
    }
}
#endif

/* ---- Leak detection: has a filled pixel left the region it started in? ----
   If the seed lies inside a closed shape, a pixel has escaped once it is
   outside that shape by more than 1.5 px (the slack absorbs rasterisation).
   For regions closed only by separate lines, a pixel has escaped once it
   is past the outlines' bounding box. */
int     leakShape = -1;
Bounds  leakBox;
bool escapedPixel(int x, int y) {
    if (leakShape >= 0) {
        const LineObj& L = lines[leakShape];
        float area;
        if (shapeContains(L, (float)x, (float)y, area)) return false;
        return distToShape(L, (float)x, (float)y) > 1.5f;
    }
    return outside(leakBox, x, y);
}

/* ---- Leak alert shown on the canvas ---- */
void  leakAlertExpire(int gen) { if (gen == leakAlertGen) { leakAlertOn = false; glutPostRedisplay(); } }
void  raiseLeakAlert(const FillRegion& F) {
    leakAlertOn = true;
    leakAlertX = F.pixels[F.leakAt].first; leakAlertY = F.pixels[F.leakAt].second;
    leakAlertAlgo = F.algorithm;
    glutTimerFunc(6000, leakAlertExpire, ++leakAlertGen);
    char m[96]; sprintf(m, "LEAK! colour escaped at (%d,%d) - fill stopped", leakAlertX, leakAlertY);
    logMsg(m);
}
#if F_ANIM

/* ---- Fill animation ----
   Progress is counted in pixels, or in scan lines for scan-line fill. */
int    fillAnimGen = 0;   /* stale timers (from an earlier fill) are ignored */

size_t fillUnits(const FillRegion& F) {
#if F_SCAN
    if (!F.rows.empty()) return F.rows.size();
#endif
    return F.pixels.size();
}
/* how many pixels are visible at progress `pos` */
size_t pixelsShownAt(const FillRegion& F, double pos) {
    size_t u = std::min((size_t)pos, fillUnits(F));
#if F_SCAN
    if (!F.rows.empty()) return (u == 0) ? 0 : F.rows[u - 1].end;
#endif
    return u;
}

void advanceFill(double units) {
    const FillRegion& F = fillRegions[fillAnimIndex];
    size_t before = pixelsShownAt(F, fillAnimPos);
    fillAnimPos += units;
    size_t after = pixelsShownAt(F, fillAnimPos);
    if (F.leakAt >= 0 && before <= (size_t)F.leakAt && after > (size_t)F.leakAt) raiseLeakAlert(F);
    if (fillAnimPos >= (double)fillUnits(F)) { fillAnimIndex = -1; fillAnimPaused = false; }
}

void fillAnimTick(int gen) {
    if (gen != fillAnimGen || fillAnimPaused) return;
    if (fillAnimIndex < 0 || fillAnimIndex >= (int)fillRegions.size()) { fillAnimIndex = -1; return; }
    advanceFill(fillAnimRate);
    glutPostRedisplay();
    if (fillAnimIndex >= 0) glutTimerFunc(ANIM_TICK_MS, fillAnimTick, gen);
}

void startFillAnimation(int idx) {
    /* speed: pixels (or scan lines) per tick, but never longer than maxSec */
    static const double pxRate[4]  = { 1, 8, 40, 250 };
    static const double rowRate[4] = { 1.0 / 6, 0.5, 1, 4 };
    static const double maxSec[4]  = { 1e9, 30, 8, 2 };
    const FillRegion& F = fillRegions[idx];
    bool byRow = false;
#if F_SCAN
    byRow = !F.rows.empty();
#endif
    double units = (double)fillUnits(F);
    double rate = byRow ? rowRate[fillSpeed] : pxRate[fillSpeed];
    fillAnimRate = std::max(rate, units / (maxSec[fillSpeed] * 1000.0 / ANIM_TICK_MS));
    fillAnimIndex = idx;
    fillAnimPos = 0;
    fillAnimPaused = false;
    glutTimerFunc(ANIM_TICK_MS, fillAnimTick, ++fillAnimGen);
}

void toggleFillPause() {
    if (fillAnimIndex < 0) return;
    fillAnimPaused = !fillAnimPaused;
    logMsg(fillAnimPaused ? "Fill paused - Space resumes, Right arrow steps" : "Fill resumed");
    if (!fillAnimPaused) glutTimerFunc(ANIM_TICK_MS, fillAnimTick, ++fillAnimGen);
}

void stepFill() {                 /* while paused: one scan line, or a few pixels */
    if (fillAnimIndex < 0 || !fillAnimPaused) return;
    bool byRow = false;
#if F_SCAN
    byRow = !fillRegions[fillAnimIndex].rows.empty();
#endif
    advanceFill(byRow ? 1.0 : std::max(1.0, std::min(fillAnimRate, 10.0)));
}
#endif

void performFill(float wx, float wy) {
    PixelSet boundary;
    rasterizeBoundaries(boundary);
    if (boundary.empty()) { logMsg("Nothing to fill - draw a closed shape first"); return; }
    Bounds bb = boundsOf(boundary, 25);   /* a leaking fill may spread 25 px past the outlines */

    int seedX = (int)floor(wx + 0.5f), seedY = (int)floor(wy + 0.5f);
    if (boundary.count(std::make_pair(seedX, seedY))) { logMsg("Seed is on an outline - click inside the shape"); return; }
    if (outside(bb, seedX, seedY)) { logMsg("Click inside a closed shape to fill it"); return; }

    /* which closed shape is the seed in? (smallest one) */
    leakShape = -1;
    float bestArea = 1e30f;
    for (size_t i = 0; i < lines.size(); i++) {
        float area;
        if (!lines[i].deleted && shapeContains(lines[i], (float)seedX, (float)seedY, area) && area < bestArea) {
            bestArea = area; leakShape = (int)i;
        }
    }
    leakBox = boundsOf(boundary, 0);

    FillRegion fr;
    PixelList& order = fr.pixels;
    std::vector<int>* aux = NULL;
#if F_ANIM
    aux = &fr.aux;
#endif
    long leakAt = -1;
#if F_B4
    if (fillAlgorithm == FILL_BOUNDARY_4) recursiveFill(seedX, seedY, boundary, bb, order, aux, 4, escapedPixel, leakAt);
#endif
#if F_B8
    if (fillAlgorithm == FILL_BOUNDARY_8) recursiveFill(seedX, seedY, boundary, bb, order, aux, 8, escapedPixel, leakAt);
#endif
#if F_FLOOD
    if (fillAlgorithm == FILL_FLOOD) {
        ColourMap colours = buildColourMap();
        int oldColour = colourAt(colours, seedX, seedY);
        if (oldColour == packRGB(fillR, fillG, fillB)) { logMsg("That area already has the fill colour"); return; }
        floodFill(seedX, seedY, boundary, colours, oldColour, bb, order, aux, escapedPixel, leakAt);
    }
#endif
#if F_SCAN
    if (fillAlgorithm == FILL_SCANLINE) {
        if (leakShape >= 0) {
            scanlinePolygonFill(lines[leakShape], order, fr.rows);   /* textbook: edge crossings */
        }
        else {
#if F_LINE
            LineObj loop;
            if (findLineLoop(seedX, seedY, loop)) {
                scanlinePolygonFill(loop, order, fr.rows);           /* lines joined into a polygon */
                logMsg("  separate lines joined into one closed polygon");
            }
            else
#endif
            {
                spanFill(seedX, seedY, boundary, bb, order);         /* region made of separate lines */
                groupIntoRows(order, fr.rows);
                for (size_t k = 0; k < order.size(); k++)
                    if (escapedPixel(order[k].first, order[k].second)) { leakAt = (long)k; break; }
            }
        }
    }
#endif
    (void)aux;
    leakAlertOn = false;

    if (order.empty()) { logMsg("Fill produced no pixels"); return; }

    fr.seedX = seedX; fr.seedY = seedY;
    fr.r = fillR; fr.g = fillG; fr.b = fillB;
    fr.algorithm = fillAlgorithm;
    fr.connectivity = fillConnectivity;
    fr.deleted = false;
#if F_UNDO
    fr.seq = nextSeq++;
#endif
    fr.leakAt = leakAt;
    fillRegions.push_back(fr);
#if F_UNDO
    redoStack.clear();
#endif
#if F_FILE
    saveFills();
#endif
#if F_ANIM
    if (animateShapes) startFillAnimation((int)fillRegions.size() - 1);
#endif

    char m[120];
    sprintf(m, "%s from seed (%d,%d): %d px", fillAlgoName[fillAlgorithm], seedX, seedY,
        (int)fillRegions.back().pixels.size());
    logMsg(m);
#if F_ANIM
    if (animateShapes) logMsg("  Space = pause / resume,  Right arrow = step");
#endif
    if (leakAt >= 0) {
#if F_ANIM
        if (!animateShapes) raiseLeakAlert(fillRegions.back());
#else
        raiseLeakAlert(fillRegions.back());
#endif
        logMsg(fillAlgorithm == FILL_BOUNDARY_8 ? "  8-fill slips through diagonal steps of the edges"
                                                : "  region is not closed");
    }
}
#endif
#if F_INPUTBOX

/* ===========================================================================
   9.  TYPED INPUT (entry box)
   =========================================================================== */
#if F_TYPE
void typeCoords() {
    switch (currentShape) {
#if F_LINE
    case SHAPE_LINE:
        beginInput(IN_COORDS, "Enter x1 y1 x2 y2 (line endpoints)",
            "e.g.  -100 -50 200 150      Enter = draw, Esc = cancel"); break;
#endif
#if F_CIRCLE
    case SHAPE_CIRCLE:
        beginInput(IN_COORDS, "Enter cx cy ex ey (centre, point on circle)",
            "e.g.  0 0 100 0   radius = distance between the two points"); break;
#endif
#if F_ELLIPSE
    case SHAPE_ELLIPSE:
        beginInput(IN_COORDS, "Enter cx cy px py [angle] (centre, box corner)",
            "e.g.  0 0 150 80 30   rx = |px-cx|, ry = |py-cy|, angle in deg"); break;
#endif
#if F_RECT
    case SHAPE_RECTANGLE:
#endif
#if F_SQUARE
    case SHAPE_SQUARE:
#endif
#if F_DIAMOND
    case SHAPE_DIAMOND:
#endif
#if F_RECT || F_SQUARE || F_DIAMOND
        beginInput(IN_COORDS, "Enter x1 y1 x2 y2 (opposite corners)",
            "e.g.  -100 -50 120 80"); break;
#endif
#if F_TRIANGLE
    case SHAPE_TRIANGLE:
        beginInput(IN_COORDS, "Enter x1 y1 x2 y2 x3 y3 (vertices)",
            "or 4 numbers x1 y1 x2 y2 = isosceles in that box"); break;
#endif
#if F_POLYGON
    case SHAPE_POLYGON:
#endif
#if F_STAR
    case SHAPE_STAR:
#endif
#if F_POLYGON || F_STAR
        beginInput(IN_COORDS, "Enter cx cy vx vy [n] (centre, first vertex)",
            "n = sides / star points (optional, 3..24)"); break;
#endif
#if F_FREEPOLY
    case SHAPE_FREEPOLY:
        beginInput(IN_COORDS, "Enter x1 y1 x2 y2 x3 y3 ... (3+ vertices)",
            "e.g.  0 0 100 0 120 80 20 120"); break;
#endif
    }
}
#endif
#if F_COLOUR
void askLineRGB() { beginInput(IN_LINE_RGB, "Line colour - enter R G B", "each 0 to 255"); }
#endif
#if F_MARKERS
void askEndpointRGB() { beginInput(IN_EP_RGB, "Endpoint marker colour - enter R G B", "each 0 to 255"); }
#endif
#if F_FILL
void askFillRGB() { beginInput(IN_FILL_RGB, "Fill colour - enter R G B", "each 0 to 255"); }
#endif
#if F_STYLE
void customPattern() {
    beginInput(IN_HEX, "Enter hex pattern",
        "A=1010  B=1011  C=1100  F0=11110000  FFFF=solid");
}
#endif
#if F_TYPE || F_TRANSFORM

/* all numbers in the typed text (commas, brackets etc. are skipped) */
std::vector<float> parseNumbers(const char* t) {
    std::vector<float> v;
    const char* p = t;
    char* end;
    while (*p) {
        float f = strtof(p, &end);
        if (end == p) { p++; continue; }
        v.push_back(f);
        p = end;
    }
    return v;
}
#endif
#if F_COLOUR || F_MARKERS || F_FILL

/* "R G B" -> three values clamped to 0..255 */
bool parseRGB(const char* t, int& r, int& g, int& b) {
    if (sscanf(t, "%d %d %d", &r, &g, &b) != 3) { logMsg("Need three numbers: R G B"); return false; }
    r = std::min(std::max(r, 0), 255);
    g = std::min(std::max(g, 0), 255);
    b = std::min(std::max(b, 0), 255);
    return true;
}
#endif
#if F_TRANSFORM

/* Applies a transform to the selection (or queues it in composite mode). */
void submitTransform(int type, const Mat3& M, const char* desc) {
#if F_COMPOSITE
    if (currentTransform == TF_COMPOSITE) {
        CompositeEntry e; e.type = type; e.matrix = M; e.desc = desc;
        compositeQueue.push_back(e);
        logMsg(std::string("Queued: ") + desc);
        return;
    }
#endif
    (void)type;
    applyTransformToSelection(M, desc);
}
#endif

/* Enter pressed in the entry box. */
void commitInput() {
    const char* t = inputBuf.c_str();
    char m[160];
    (void)t; (void)m;
#if F_TYPE || F_TRANSFORM
    std::vector<float> n = parseNumbers(t);
#endif
#if F_PIVOT
    float pcx = tfPivotX, pcy = tfPivotY;   /* origin unless a pivot was set */

    if (inputTarget == IN_TF_PIVOT) {
        if (n.size() >= 2) {
            tfPivotX = n[0]; tfPivotY = n[1]; tfPivotSet = true;
            sprintf(m, "Pivot set to (%g, %g)", n[0], n[1]);
        }
        else { tfPivotX = tfPivotY = 0.0f; tfPivotSet = false; sprintf(m, "Pivot reset to origin (0,0)"); }
        logMsg(m);
        /* back to the transform entry, keeping what was typed */
        inputTarget = pivotReturnTarget; inputBuf = pivotReturnBuf;
        inputPrompt = pivotReturnPrompt; inputHint = pivotReturnHint;
        return;
    }
#endif
#if F_TYPE

    if (inputTarget == IN_COORDS) {
        int s = currentShape;
        bool done = false;
        (void)s;
#if F_TWOPT
        if (s == SHAPE_LINE || s == SHAPE_CIRCLE || s == SHAPE_ELLIPSE) {
            if (n.size() >= 4) {
#if F_ELLIPSE
                currentAngle = (s == SHAPE_ELLIPSE && n.size() >= 5) ? n[4] * 3.14159265f / 180.0f : 0.0f;
#endif
                addLine(n[0], n[1], n[2], n[3]);
            }
            else logMsg("Need four numbers");
            done = true;
        }
#endif
#if F_VERTS
        if (!done && (s == SHAPE_FREEPOLY || (s == SHAPE_TRIANGLE && n.size() >= 6))) {
            if (n.size() >= 6) {
                std::vector<Pt> v;
                size_t cnt = (s == SHAPE_TRIANGLE) ? 6 : (n.size() / 2) * 2;
                for (size_t k = 0; k + 1 < cnt; k += 2) { Pt p; p.x = n[k]; p.y = n[k + 1]; v.push_back(p); }
                addPolygon(s, v);
            }
            else logMsg("Need at least three x y pairs");
            done = true;
        }
#endif
#if F_POLYBUILD
        if (!done) {
            if (n.size() >= 4) {
#if F_POLYGON
                if (s == SHAPE_POLYGON && n.size() >= 5) polySides = std::min(std::max((int)n[4], 3), 24);
#endif
#if F_STAR
                if (s == SHAPE_STAR && n.size() >= 5) starPoints = std::min(std::max((int)n[4], 3), 24);
#endif
                addPolygon(s, buildPolyVerts(s, n[0], n[1], n[2], n[3]));
            }
            else logMsg("Need four numbers");
        }
#endif
    }
#endif
#if F_COLOUR
    if (inputTarget == IN_LINE_RGB) {
        int r, g, b;
        if (parseRGB(t, r, g, b)) {
            curR = r; curG = g; curB = b; colourIsCustom = true;
            sprintf(m, "Line colour RGB(%d,%d,%d)", r, g, b); logMsg(m);
        }
    }
#endif
#if F_MARKERS
    if (inputTarget == IN_EP_RGB) {
        int r, g, b;
        if (parseRGB(t, r, g, b)) {
            epR = r; epG = g; epB = b;
            sprintf(m, "Endpoint colour RGB(%d,%d,%d)", r, g, b); logMsg(m);
        }
    }
#endif
#if F_FILL
    if (inputTarget == IN_FILL_RGB) {
        int r, g, b;
        if (parseRGB(t, r, g, b)) {
            fillR = r; fillG = g; fillB = b;
            sprintf(m, "Fill colour RGB(%d,%d,%d)", r, g, b); logMsg(m);
        }
    }
#endif
#if F_STYLE
    if (inputTarget == IN_HEX) {
        std::string hex = inputBuf;
        if (hex.size() > 2 && hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X')) hex = hex.substr(2);
        if (hex.empty() || hex.size() > 8) logMsg("Use 1 to 8 hex digits");
        else {
            unsigned int v = 0; bool ok = true;
            for (size_t i = 0; i < hex.size(); i++) {
                char c = hex[i]; int dgt;
                if (c >= '0' && c <= '9') dgt = c - '0';
                else if (c >= 'a' && c <= 'f') dgt = c - 'a' + 10;
                else if (c >= 'A' && c <= 'F') dgt = c - 'A' + 10;
                else { ok = false; break; }
                v = (v << 4) | dgt;
            }
            if (!ok) logMsg("Invalid hex digit");
            else if (v == 0) logMsg("Pattern 0 would draw nothing");
            else {
                curPattern = v; curPatternBits = (int)hex.size() * 4; styleIsCustom = true;
                char bin[40]; patternToBinary(curPattern, curPatternBits, bin);
                sprintf(m, "Pattern 0x%X = %s", curPattern, bin); logMsg(m);
            }
        }
    }
#endif
#if F_TRANSLATE
    if (inputTarget == IN_TF_TRANSLATE) {
        if (n.size() >= 2) {
            sprintf(m, "Translate (%g, %g)", n[0], n[1]);
            submitTransform(TF_TRANSLATE, mat3Translation(n[0], n[1]), m);
        }
        else logMsg("Need two numbers: tx ty");
    }
#endif
#if F_SCALE
    if (inputTarget == IN_TF_SCALE) {
        if (n.size() >= 1) {
            float scx = n[0], scy = (n.size() >= 2) ? n[1] : n[0];
            float px = (n.size() >= 4) ? n[2] : pcx, py = (n.size() >= 4) ? n[3] : pcy;
            sprintf(m, "Scale (%g,%g) about (%g,%g)", scx, scy, px, py);
            submitTransform(TF_SCALE, mat3ScaleAbout(scx, scy, px, py), m);
        }
        else logMsg("Need: sx [sy]");
    }
#endif
#if F_ROTATE
    if (inputTarget == IN_TF_ROTATE) {
        if (n.size() >= 1) {
            float px = (n.size() >= 3) ? n[1] : pcx, py = (n.size() >= 3) ? n[2] : pcy;
            sprintf(m, "Rotate %g deg about (%g,%g)", n[0], px, py);
            submitTransform(TF_ROTATE, mat3RotateAbout(n[0] * 3.14159265f / 180.0f, px, py), m);
        }
        else logMsg("Need: angle_deg");
    }
#endif
#if F_REFLECT
    if (inputTarget == IN_TF_REFLECT) {
        std::string up = inputBuf;
        for (size_t i = 0; i < up.size(); i++) up[i] = (char)toupper((unsigned char)up[i]);
        float x1 = 0, y1 = 0, x2 = 0, y2 = 0; bool ok = true;
        if (up == "X")       { x1 = 0; y1 = 0; x2 = 1; y2 = 0; }
        else if (up == "Y")  { x1 = 0; y1 = 0; x2 = 0; y2 = 1; }
        else if (up == "XY" || up == "Y=X") { x1 = 0; y1 = 0; x2 = 1; y2 = 1; }
        else if (n.size() >= 4) { x1 = n[0]; y1 = n[1]; x2 = n[2]; y2 = n[3]; }
        else ok = false;
        if (ok) {
            sprintf(m, "Reflect about (%g,%g)-(%g,%g)", x1, y1, x2, y2);
            submitTransform(TF_REFLECT_AXIS, mat3ReflectAboutLine(x1, y1, x2, y2), m);
        }
        else logMsg("Need x1 y1 x2 y2, or X / Y / XY");
    }
#endif
#if F_SHEAR
    if (inputTarget == IN_TF_SHEAR) {
        if (n.size() >= 2) {
            float px = (n.size() >= 4) ? n[2] : pcx, py = (n.size() >= 4) ? n[3] : pcy;
            sprintf(m, "Shear (%g,%g) about (%g,%g)", n[0], n[1], px, py);
            submitTransform(TF_SHEAR, mat3ShearAbout(n[0], n[1], px, py), m);
        }
        else logMsg("Need: shx shy");
    }
#endif

    inputTarget = IN_NONE; inputBuf.clear();
}
#endif
