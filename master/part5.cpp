
/* ===========================================================================
   11. CANVAS
   =========================================================================== */
/* Smallest 1/2/5 x 10^k step that is at least minWorld. */
float niceStep(float minWorld) {
    float p = powf(10.0f, floorf(log10f(minWorld)));
    float m = minWorld / p;
    float s = (m <= 1.0f) ? 1.0f : (m <= 2.0f) ? 2.0f : (m <= 5.0f) ? 5.0f : 10.0f;
    return std::max(1.0f, s * p);
}

void drawGrid() {
    float cw = canvasW();
    float step = niceStep(40.0f / viewScale);
    float lstep = niceStep(90.0f / viewScale);

    float wxMin = screenToWorldX(0);
    float wxMax = screenToWorldX(cw);
    float wyMin = screenToWorldY(0);
    float wyMax = screenToWorldY((float)winH);

    /* pixel grid when zoomed far enough in to see individual raster pixels */
    if (viewScale >= 8.0f) {
        if (darkMode) glColor3f(0.17f, 0.18f, 0.20f);
        else          glColor3f(0.95f, 0.95f, 0.97f);
        glBegin(GL_LINES);
        for (float x = floorf(wxMin) - 0.5f; x <= wxMax; x += 1.0f) {
            glVertex2f(sx(x), 0); glVertex2f(sx(x), (float)winH);
        }
        for (float y = floorf(wyMin) - 0.5f; y <= wyMax; y += 1.0f) {
            glVertex2f(0, sy(y)); glVertex2f(cw, sy(y));
        }
        glEnd();
    }

    if (darkMode) glColor3f(0.22f, 0.23f, 0.25f);
    else          glColor3f(0.90f, 0.91f, 0.93f);
    glBegin(GL_LINES);
    for (float x = floorf(wxMin / step) * step; x <= wxMax; x += step) {
        glVertex2f(sx(x), 0); glVertex2f(sx(x), (float)winH);
    }
    for (float y = floorf(wyMin / step) * step; y <= wyMax; y += step) {
        glVertex2f(0, sy(y)); glVertex2f(cw, sy(y));
    }
    glEnd();

    if (darkMode) glColor3f(0.65f, 0.65f, 0.68f);
    else          glColor3f(0.20f, 0.20f, 0.22f);
    glLineWidth(1.6f);
    glBegin(GL_LINES);
    glVertex2f(0, sy(0)); glVertex2f(cw, sy(0));
    glVertex2f(sx(0), 0); glVertex2f(sx(0), (float)winH);
    glEnd();
    glLineWidth(1.0f);

    /* axis labels: stick to the canvas edge when the axis is off-screen */
    float labY = std::min(std::max(sy(0) + 6, 6.0f), (float)winH - 14);
    float labX = std::min(std::max(sx(0) + 6, 4.0f), cw - 40);
    char b[24];
    if (darkMode) glColor3f(0.55f, 0.55f, 0.58f);
    else          glColor3f(0.45f, 0.45f, 0.48f);
    for (float x = floorf(wxMin / lstep) * lstep; x <= wxMax; x += lstep) {
        if (fabsf(x) < 0.001f) continue;
        sprintf(b, "%g", x);
        text(sx(x) - textWidth(b, GLUT_BITMAP_HELVETICA_10) / 2.0f, labY, b, GLUT_BITMAP_HELVETICA_10);
    }
    for (float y = floorf(wyMin / lstep) * lstep; y <= wyMax; y += lstep) {
        if (fabsf(y) < 0.001f) continue;
        sprintf(b, "%g", y);
        text(labX, sy(y) - 4, b, GLUT_BITMAP_HELVETICA_10);
    }
    text(sx(0) + 5, sy(0) + 6, "(0,0)", GLUT_BITMAP_HELVETICA_10);
}
#if F_CIRCLE

/* Circle mode: shows the 8 octants the midpoint circle mirrors into. */
void drawOctantOverlay() {
    float cw = canvasW();
    float ox = originX() + viewOffsetX;
    float oy = originY() + viewOffsetY;
    float reach = std::max((float)winW, (float)winH);

    glLineWidth(1.2f);
    glColor3f(0.55f, 0.35f, 0.90f);
    glEnable(GL_LINE_STIPPLE);
    glLineStipple(1, 0x3F3F);
    glBegin(GL_LINES);
    glVertex2f(ox - reach, oy - reach);
    glVertex2f(ox + reach, oy + reach);
    glVertex2f(ox - reach, oy + reach);
    glVertex2f(ox + reach, oy - reach);
    glEnd();
    glDisable(GL_LINE_STIPPLE);
    glLineWidth(1.0f);

    const float LR = 68.0f;
    const float S2 = 0.7071f;
    struct { float dx, dy; const char* name; } oct[8] = {
        {  1.0f,  S2, "O1" }, {  S2,  1.0f, "O2" },
        { -S2,  1.0f, "O3" }, { -1.0f,  S2, "O4" },
        { -1.0f, -S2, "O5" }, { -S2, -1.0f, "O6" },
        {  S2, -1.0f, "O7" }, {  1.0f, -S2, "O8" },
    };
    for (int k = 0; k < 8; k++) {
        float lx = ox + oct[k].dx * LR;
        float ly = oy + oct[k].dy * LR;
        float tw = (float)textWidth(oct[k].name, GLUT_BITMAP_HELVETICA_10);
        glColor3f(1.0f, 1.0f, 1.0f);
        rect(lx - 2, ly - 2, tw + 4, 13);
        glColor3f(0.45f, 0.20f, 0.80f);
        text(lx, ly + 1, oct[k].name, GLUT_BITMAP_HELVETICA_10);
    }

    glColor3f(0.50f, 0.30f, 0.80f);
    const char* hint = "Circle mode: 8-octant symmetry active";
    float hw = (float)textWidth(hint, GLUT_BITMAP_HELVETICA_10);
    text((cw - hw) / 2.0f, (float)(winH - 10), hint, GLUT_BITMAP_HELVETICA_10);
}
#endif

void labelPoint(float wx, float wy, float dy, const char* s) {
    float cw = canvasW();
    float w = (float)textWidth(s, GLUT_BITMAP_HELVETICA_10);
    float lx = (sx(wx) + 8 + w > cw - 4) ? sx(wx) - w - 8 : sx(wx) + 8;
    if (lx < 4) lx = 4;
    float ly = sy(wy) + dy;
    if (ly > (float)(winH - 14)) ly = (float)(winH - 14);
    if (ly < 6) ly = 6;
    text(lx, ly, s, GLUT_BITMAP_HELVETICA_10);
}
#if F_FILL

void plotFillPixels(const FillRegion& F, size_t from, size_t to) {
    if (viewScale <= 1.0f) {
        glPointSize(viewScale < 1.0f ? 1.0f : 1.5f);
        glBegin(GL_POINTS);
        for (size_t k = from; k < to; k++)
            glVertex2f(sx((float)F.pixels[k].first), sy((float)F.pixels[k].second));
        glEnd();
        glPointSize(1.0f);
    }
    else {
        glBegin(GL_QUADS);
        float h = viewScale / 2.0f;
        for (size_t k = from; k < to; k++) {
            float cx = sx((float)F.pixels[k].first);
            float cy = sy((float)F.pixels[k].second);
            glVertex2f(cx - h, cy - h); glVertex2f(cx + h, cy - h);
            glVertex2f(cx + h, cy + h); glVertex2f(cx - h, cy + h);
        }
        glEnd();
    }
}

void drawFills() {
    for (size_t i = 0; i < fillRegions.size(); i++) {
        const FillRegion& F = fillRegions[i];
        if (F.deleted) continue;
        size_t n = F.pixels.size();
#if F_ANIM
        if ((int)i == fillAnimIndex) {
            size_t shown = std::min(pixelsShownAt(F, fillAnimPos), n);
#if F_SCAN
            if (!F.rows.empty()) {
                /* scan-line: finished rows, plus the current scan line and its edge crossings */
                col255(F.r, F.g, F.b);
                plotFillPixels(F, 0, shown);
                size_t r = std::min(F.rows.size() - 1, (size_t)std::max(0.0, fillAnimPos - 1.0));
                const ScanRow& row = F.rows[r];
                float ly = sy((float)row.y);
                glColor3f(0.10f, 0.75f, 0.95f);
                glLineWidth(2.0f);
                glBegin(GL_LINES); glVertex2f(0, ly); glVertex2f(canvasW(), ly); glEnd();
                glLineWidth(1.0f);
                glColor3f(0.90f, 0.10f, 0.10f);
                for (size_t k = 0; k < row.xs.size(); k++) drawMarker(row.xs[k], (float)row.y, 9.0f);
                char b[40]; sprintf(b, "y = %d", row.y);
                glColor3f(0.10f, 0.55f, 0.80f);
                text(6, ly + 5, b, GLUT_BITMAP_HELVETICA_12);
                continue;
            }
#endif
            /* pixels appear in the order the algorithm coloured them; the newest
               band is drawn in a lighter tint and the current pixel is boxed */
            size_t front = std::min(shown, (size_t)std::max(6.0, fillAnimRate * 6));
            col255(F.r, F.g, F.b);
            plotFillPixels(F, 0, shown - front);
            col255((F.r + 255 * 2) / 3, (F.g + 255 * 2) / 3, (F.b + 255 * 2) / 3);
            plotFillPixels(F, shown - front, shown);
            glColor3f(0.9f, 0.1f, 0.1f);                     /* seed */
            drawMarker((float)F.seedX, (float)F.seedY, 6.0f);
            if (shown > 0) {                                 /* current pixel */
                float cx = sx((float)F.pixels[shown - 1].first), cy = sy((float)F.pixels[shown - 1].second);
                float h = std::max(6.0f, viewScale);
                glLineWidth(2.0f);
                glColor3f(0.0f, 0.0f, 0.0f); rectOutline(cx - h - 1, cy - h - 1, 2 * h + 2, 2 * h + 2);
                glColor3f(1.0f, 0.95f, 0.2f); rectOutline(cx - h, cy - h, 2 * h, 2 * h);
                glLineWidth(1.0f);
            }
            continue;
        }
#endif
        col255(F.r, F.g, F.b);
        plotFillPixels(F, 0, n);
    }
}
#endif

#if F_SNAP
float snapX(float wx) { return snapToGrid ? floorf(wx / snapStep + 0.5f) * snapStep : wx; }
float snapY(float wy) { return snapToGrid ? floorf(wy / snapStep + 0.5f) * snapStep : wy; }
#else
float snapX(float wx) { return wx; }   /* (no snap-to-grid in this program) */
float snapY(float wy) { return wy; }
#endif

void drawShapeMaybeAnimated(int i, const LineObj& L) {
#if F_ANIM
    if (i == animObjIndex) {
        size_t n = std::min(animRevealCount, animPoints.size());
        beginPlot();
        for (size_t k = 0; k < n; k++) plotPixel(animPoints[k].first, animPoints[k].second);
        endPlot();
        return;
    }
#endif
    (void)i;
    traceShapeBatched(L);
}

/* Temporary shape object used for the dashed previews. */
LineObj makePreviewShape(int shape, float ax, float ay, float bx, float by) {
    LineObj L = makeShapeWithCurrentStyle(shape);
    L.x0 = ax; L.y0 = ay; L.x1 = bx; L.y1 = by;
#if F_POLYBUILD
    if (isPolyShape(shape)) { L.pts = buildPolyVerts(shape, ax, ay, bx, by); syncPolyBox(L); }
#endif
#if F_ELLIPSE
    if (shape == SHAPE_ELLIPSE) {
        L.algo = ALGO_MID_ELLIPSE;
        L.x1 = ax + fabsf(bx - ax); L.y1 = ay + fabsf(by - ay);
    }
#endif
    return L;
}
void beginPreviewStyle() {
    glColor3f(0.50f, 0.50f, 0.55f);
    plotPattern = 0xAAAA; plotBits = 16; plotScale = 1;
    plotThickness = (float)curThickness;
}
void drawPreviewShape(const LineObj& L) {
    unsigned int sp = plotPattern; int sb = plotBits, ss = plotScale;
    beginPreviewStyle();
    traceShapeBatched(L);
    plotPattern = sp; plotBits = sb; plotScale = ss;
}

void drawLines() {
    char lb[64];
    for (size_t i = 0; i < lines.size(); i++) {
        const LineObj& L = lines[i];
        if (L.deleted) continue;
#if F_SELECT

        if (isSelected((int)i)) {
            glColor3f(0.20f, 0.55f, 1.0f);
            /* halo: ~6 screen px wider than the shape at any zoom */
            plotThickness = L.thickness + (viewScale > 1.0f ? 6.0f / viewScale : 6.0f);
            plotPattern = 0xFFFF; plotBits = 16; plotScale = 1;
            drawShapeMaybeAnimated((int)i, L);
        }
#endif

        if (darkMode && (L.r + L.g + L.b) < 120) col255(255, 255, 255);
        else                                     col255(L.r, L.g, L.b);
        plotThickness = (float)L.thickness;
        plotPattern = L.pattern; plotBits = L.patternBits; plotScale = L.patternScale;
#if F_TRANSFORM

        bool isOldDuringConfirm = tfConfirmActive &&
            std::find(tfOldIndices.begin(), tfOldIndices.end(), (int)i) != tfOldIndices.end();
        bool isNewDuringConfirm = tfConfirmActive &&
            std::find(tfNewIndices.begin(), tfNewIndices.end(), (int)i) != tfNewIndices.end();
        if (isOldDuringConfirm) {            /* original shown dashed grey */
            glColor3f(0.55f, 0.55f, 0.60f);
            plotThickness = (float)std::max(1, L.thickness);
            plotPattern = 0xCCCC; plotBits = 16; plotScale = 2;
        }
#endif
        drawShapeMaybeAnimated((int)i, L);
#if F_MARKERS

        if (endpointsOn) {
            col255(epR, epG, epB);
            if (isPolyShape(L.shape)) {
                for (size_t k = 0; k < L.pts.size(); k++) drawMarker(L.pts[k].x, L.pts[k].y, 6.0f);
            }
            else {
                drawMarker(L.x0, L.y0, 6.0f);
                drawMarker(L.x1, L.y1, 6.0f);
            }
        }
#endif
        if (showLabels) {
            if (darkMode) glColor3f(0.82f, 0.84f, 0.88f);
            else          glColor3f(0.15f, 0.15f, 0.18f);
#if F_LINE
            if (L.shape == SHAPE_LINE) {
                sprintf(lb, "P1 (%d,%d)", (int)L.x0, (int)L.y0);
                labelPoint(L.x0, L.y0, -14.0f, lb);
                sprintf(lb, "P2 (%d,%d)", (int)L.x1, (int)L.y1);
                labelPoint(L.x1, L.y1, 8.0f, lb);
            }
#endif
#if F_CIRCLE
            if (L.shape == SHAPE_CIRCLE) {
                int r = (int)floor(sqrtf((L.x1 - L.x0) * (L.x1 - L.x0) + (L.y1 - L.y0) * (L.y1 - L.y0)) + 0.5f);
                sprintf(lb, "Centre (%d,%d)", (int)L.x0, (int)L.y0);
                labelPoint(L.x0, L.y0, -14.0f, lb);
                sprintf(lb, "r = %d", r);
                labelPoint(L.x1, L.y1, 8.0f, lb);
            }
#endif
#if F_ELLIPSE
            if (L.shape == SHAPE_ELLIPSE) {
                int rx = (int)floor(fabs(L.x1 - L.x0) + 0.5f), ry = (int)floor(fabs(L.y1 - L.y0) + 0.5f);
                sprintf(lb, "Centre (%d,%d)", (int)L.x0, (int)L.y0);
                labelPoint(L.x0, L.y0, -14.0f, lb);
                sprintf(lb, "rx=%d ry=%d", rx, ry);
                labelPoint(L.x1, L.y1, 8.0f, lb);
            }
#endif
#if F_POLY
            if (isPolyShape(L.shape)) {
                if (L.pts.size() <= 6) {
                    /* label each vertex, above or below depending on side */
                    float cy = (L.y0 + L.y1) / 2.0f;
                    for (size_t k = 0; k < L.pts.size(); k++) {
                        sprintf(lb, "V%d (%d,%d)", (int)k + 1,
                            (int)floorf(L.pts[k].x + 0.5f), (int)floorf(L.pts[k].y + 0.5f));
                        labelPoint(L.pts[k].x, L.pts[k].y, (L.pts[k].y >= cy) ? 8.0f : -14.0f, lb);
                    }
                }
                else {
                    sprintf(lb, "%s  %d verts  (%d,%d)-(%d,%d)", shapeName[L.shape], (int)L.pts.size(),
                        (int)L.x0, (int)L.y0, (int)L.x1, (int)L.y1);
                    labelPoint(L.x0, L.y1, 8.0f, lb);
                }
            }
#endif
        }
#if F_TRANSFORM
        if (tfConfirmActive && showLabels && (isOldDuringConfirm || isNewDuringConfirm)) {
            float ax, ay; anchorOf(L, ax, ay);
            if (isOldDuringConfirm) { glColor3f(0.90f, 0.40f, 0.15f); labelPoint(ax, ay, -28.0f, "[OLD]"); }
            else                    { glColor3f(0.15f, 0.75f, 0.30f); labelPoint(ax, ay, -28.0f, "[NEW]"); }
        }
#endif
    }

    float mxs = snapX(mouseWorldX), mys = snapY(mouseWorldY);
    (void)lb;
#if F_VERTS

    /* vertex-by-vertex shapes in progress (free polygon, 3-click triangle) */
    if (!polyVerts.empty()) {
        unsigned int sp = plotPattern; int sb = plotBits, ss = plotScale;
        beginPreviewStyle();
        glColor3f(0.20f, 0.45f, 0.85f);
        beginPlot();
        for (size_t k = 0; k + 1 < polyVerts.size(); k++)
            runAlgorithm(currentAlgo, polyVerts[k].x, polyVerts[k].y, polyVerts[k + 1].x, polyVerts[k + 1].y);
        endPlot();
        glColor3f(0.50f, 0.50f, 0.55f);
        beginPlot();
        runAlgorithm(currentAlgo, polyVerts.back().x, polyVerts.back().y, mxs, mys);
        if (polyVerts.size() >= 2)
            runAlgorithm(currentAlgo, mxs, mys, polyVerts[0].x, polyVerts[0].y);
        endPlot();
        plotPattern = sp; plotBits = sb; plotScale = ss;

        glColor3f(0.85f, 0.0f, 0.0f);
        for (size_t k = 0; k < polyVerts.size(); k++) drawMarker(polyVerts[k].x, polyVerts[k].y, 7.0f);
#if F_FREEPOLY
        /* closing target on the first vertex */
        if (currentShape == SHAPE_FREEPOLY && polyVerts.size() >= 3) {
            float d = hypotf(sx(polyVerts[0].x) - sx(mouseWorldX), sy(polyVerts[0].y) - sy(mouseWorldY));
            glColor3f(d < 10.0f ? 0.10f : 0.85f, d < 10.0f ? 0.70f : 0.0f, 0.10f);
            rectOutline(sx(polyVerts[0].x) - 8, sy(polyVerts[0].y) - 8, 16, 16);
        }
#endif
        char b[64];
        sprintf(b, "V%d (%d,%d)", (int)polyVerts.size() + 1, (int)mxs, (int)mys);
        glColor3f(0.20f, 0.45f, 0.85f);
        labelPoint(mxs, mys, 8.0f, b);

        glColor3f(0.40f, 0.40f, 0.45f);
        const char* hint = (currentShape == SHAPE_FREEPOLY)
            ? "Click to add vertices - click first vertex, Enter or right-click to close - Backspace undo point - Esc cancel"
            : "Click the next vertex - Backspace undo point - Esc cancel";
        text((canvasW() - textWidth(hint, GLUT_BITMAP_HELVETICA_10)) / 2.0f, 12.0f, hint, GLUT_BITMAP_HELVETICA_10);
    }
    else
#endif
#if F_DRAG
    if (clickState >= 1 && !dragging) {
#else
    if (clickState >= 1) {
#endif
        glColor3f(0.85f, 0.0f, 0.0f);
        drawMarker(pendingX, pendingY, 7.0f);
        char b[64];
        bool corner = (currentShape == SHAPE_LINE || currentShape == SHAPE_RECTANGLE ||
            currentShape == SHAPE_SQUARE || currentShape == SHAPE_DIAMOND || currentShape == SHAPE_TRIANGLE);
        sprintf(b, "%s (%d,%d)", currentShape == SHAPE_LINE ? "P1" : corner ? "Corner" : "Centre",
            (int)pendingX, (int)pendingY);
        labelPoint(pendingX, pendingY, -14.0f, b);

        /* rubber-band preview between the two clicks */
        if (clickState == 1 && currentShape != SHAPE_ELLIPSE) {
            drawPreviewShape(makePreviewShape(currentShape, pendingX, pendingY, mxs, mys));
#if F_POLY
            if (isPolyShape(currentShape)) {
                LineObj P = makePreviewShape(currentShape, pendingX, pendingY, mxs, mys);
                sprintf(b, "%d x %d", (int)(P.x1 - P.x0 + 0.5f), (int)(P.y1 - P.y0 + 0.5f));
                glColor3f(0.40f, 0.40f, 0.45f);
                labelPoint(mxs, mys, 8.0f, b);
            }
#endif
        }
    }
#if F_DRAG

    /* drag-to-draw preview */
    if (dragging) {
        drawPreviewShape(makePreviewShape(currentShape, pendingX, pendingY, liveEndX, liveEndY));
    }
#endif
#if F_SELECT

    /* box-select rectangle */
    if (boxSelecting) {
        float x0 = std::min(boxX0, boxX1), x1 = std::max(boxX0, boxX1);
        float y0 = std::min(boxY0, boxY1), y1 = std::max(boxY0, boxY1);
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(0.20f, 0.55f, 1.0f, 0.10f);
        rect(x0, y0, x1 - x0, y1 - y0);
        glDisable(GL_BLEND);
        glColor3f(0.20f, 0.55f, 1.0f);
        glEnable(GL_LINE_STIPPLE); glLineStipple(1, 0x0F0F);
        rectOutline(x0, y0, x1 - x0, y1 - y0);
        glDisable(GL_LINE_STIPPLE);
    }

    /* selection bounding box */
    if (selection.size() > 1 && !boxSelecting) {
        float a, b2, c, d;
        if (selectionBBox(a, b2, c, d)) {
            glColor3f(0.20f, 0.55f, 1.0f);
            glEnable(GL_LINE_STIPPLE); glLineStipple(1, 0x3333);
            rectOutline(sx(a) - 6, sy(b2) - 6, sx(c) - sx(a) + 12, sy(d) - sy(b2) + 12);
            glDisable(GL_LINE_STIPPLE);
        }
    }
#endif
#if F_ELLIPSE

    /* ellipse step 2 guides */
#if F_DRAG
    if (currentShape == SHAPE_ELLIPSE && clickState == 2 && ellipseRx > 0 && !dragging) {
#else
    if (currentShape == SHAPE_ELLIPSE && clickState == 2 && ellipseRx > 0) {
#endif
        float csx = sx(pendingX), csy = sy(pendingY);
        float p2sx = sx(ellipseP2X), p2sy = sy(ellipseP2Y);
        glColor3f(0.10f, 0.60f, 0.95f);
        glLineWidth(2.0f);
        glBegin(GL_LINES);
        glVertex2f(csx, csy); glVertex2f(p2sx, p2sy);
        glEnd();
        glLineWidth(1.0f);
        drawMarker(ellipseP2X, ellipseP2Y, 8.0f);

        char rlabel[32];
        sprintf(rlabel, "rx = %d", ellipseRx);
        text(p2sx + 8, p2sy + 6, rlabel, GLUT_BITMAP_HELVETICA_10);

        float rdx = ellipseP2X - pendingX, rdy = ellipseP2Y - pendingY;
        float rlen = sqrtf(rdx * rdx + rdy * rdy);
        if (rlen < 1.0f) rlen = 1.0f;
        float perpUX = -rdy / rlen, perpUY = rdx / rlen;
        float reach = std::max((float)winW, (float)winH);

        glColor3f(0.90f, 0.35f, 0.15f);
        glLineWidth(1.5f);
        glEnable(GL_LINE_STIPPLE);
        glLineStipple(1, 0x3F3F);
        glBegin(GL_LINES);
        glVertex2f(csx - perpUX * reach, csy - perpUY * reach);
        glVertex2f(csx + perpUX * reach, csy + perpUY * reach);
        glEnd();
        glDisable(GL_LINE_STIPPLE);
        glLineWidth(1.0f);

        glColor3f(0.90f, 0.35f, 0.15f);
        drawMarker(pendingX + perpUX * ellipseRy, pendingY + perpUY * ellipseRy, 8.0f);
        drawMarker(pendingX - perpUX * ellipseRy, pendingY - perpUY * ellipseRy, 8.0f);

        char rylabel[32];
        sprintf(rylabel, "ry = %d", ellipseRy);
        text(csx + perpUX * (float)ellipseRy * viewScale + 8,
            csy + perpUY * (float)ellipseRy * viewScale + 6,
            rylabel, GLUT_BITMAP_HELVETICA_10);

        glColor3f(0.60f, 0.60f, 0.65f);
        unsigned int savedPat = plotPattern; int savedBits = plotBits;
        int savedScale = plotScale; float savedThick = plotThickness;
        float savedAngle = plotAngle;
        plotPattern = 0xAAAA; plotBits = 16; plotScale = 1; plotThickness = 1;
        plotAngle = currentAngle;
        beginPlot();
        runAlgorithm(ALGO_MID_ELLIPSE, pendingX, pendingY, pendingX + (float)ellipseRx, pendingY + (float)ellipseRy);
        endPlot();
        plotPattern = savedPat; plotBits = savedBits;
        plotScale = savedScale; plotThickness = savedThick;
        plotAngle = savedAngle;

        glColor3f(0.40f, 0.40f, 0.45f);
#if F_ZOOM
        const char* hint = "Move mouse for ry, scroll to fine-tune, click to confirm";
#else
        const char* hint = "Move mouse for ry, wheel to fine-tune, click to confirm";
#endif
        float hw = (float)textWidth(hint, GLUT_BITMAP_HELVETICA_10);
        text((canvasW() - hw) / 2.0f, 12.0f, hint, GLUT_BITMAP_HELVETICA_10);
    }
#endif
}

/* ===========================================================================
   12. DISPLAY
   =========================================================================== */
void translucentBox(float x, float y, float w, float h, float a) {
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.10f, 0.11f, 0.14f, a);
    rect(x, y, w, h);
    glDisable(GL_BLEND);
}

/* Permanent readout in the canvas' top-left corner. */
void drawInfoOverlay() {
    char l[5][160];
    int nl = 0;
    sprintf(l[nl++], "Zoom %.0f%%   Cursor (%d, %d)", viewScale * 100.0f,
        (int)floorf(mouseWorldX + 0.5f), (int)floorf(mouseWorldY + 0.5f));
    {
        char sel[32] = "", fil[32] = "";
#if F_SELECT
        sprintf(sel, "   Selected %d", (int)selection.size());
#endif
#if F_FILL
        sprintf(fil, "   Fills %d", (int)fillRegions.size());
#endif
        sprintf(l[nl++], "Shapes %d/%d%s%s", activeCount(), (int)lines.size(), sel, fil);
    }
#if F_STYLE
    {
        char bin[40];
        patternToBinary(curPattern, curPatternBits, bin);
        sprintf(l[nl++], "Pat 0x%X = %s  x%d", curPattern, bin, curPatternScale);
    }
#endif
#if F_FILL && F_ANIM
    if (fillAnimIndex >= 0) {                  /* live counters while a fill animates */
        const FillRegion& F = fillRegions[fillAnimIndex];
        size_t shown = std::min(pixelsShownAt(F, fillAnimPos), F.pixels.size());
        bool byRow = false;
#if F_SCAN
        byRow = !F.rows.empty();
        if (byRow) {
            size_t r = std::min(F.rows.size() - 1, (size_t)std::max(0.0, fillAnimPos - 1.0));
            const ScanRow& row = F.rows[r];
            char xs[80] = "";
            for (size_t k = 0; k < row.xs.size() && k < 6; k++) {
                char one[16]; sprintf(one, k ? ", %.1f" : "%.1f", row.xs[k]); strcat(xs, one);
            }
            sprintf(l[nl++], "Scan-line y = %d  (row %d/%d)  crossings: %s",
                row.y, (int)r + 1, (int)F.rows.size(), row.xs.empty() ? "none" : xs);
        }
#endif
        if (!byRow) {
            int a = (shown > 0 && shown <= F.aux.size()) ? F.aux[shown - 1] : 0;
            sprintf(l[nl++], "%s: %d / %d px   %s size %d", fillAlgoName[F.algorithm], (int)shown,
                (int)F.pixels.size(), F.algorithm == FILL_FLOOD ? "queue" : "stack", a);
        }
        sprintf(l[nl++], fillAnimPaused ? "PAUSED   Space = resume   Right arrow = step" : "Space = pause");
    }
#endif

    int w = 0;
    for (int i = 0; i < nl; i++) w = std::max(w, textWidth(l[i], GLUT_BITMAP_HELVETICA_10));
    for (size_t i = 0; i < logLines.size(); i++) w = std::max(w, textWidth(logLines[i].c_str(), GLUT_BITMAP_HELVETICA_10));
    float bw = (float)std::min(w + 20, (int)canvasW() - 20);
    float bh = nl * 13 + 18 + (float)logLines.size() * 12 + 10;
    float bx = 10, by = (float)winH - 10 - bh;

    translucentBox(bx, by, bw, bh, 0.85f);
    glColor3f(0.30f, 0.33f, 0.40f);
    rectOutline(bx, by, bw, bh);

    float y = by + bh - 15;
    glColor3f(0.75f, 0.78f, 0.82f);
    for (int i = 0; i < nl; i++) { text(bx + 10, y, l[i], GLUT_BITMAP_HELVETICA_10); y -= 13; }
    y -= 3;
    glColor3f(0.42f, 0.58f, 0.80f);
    text(bx + 10, y, "STATUS", GLUT_BITMAP_HELVETICA_10);
    y -= 13;
    glColor3f(0.45f, 0.85f, 0.55f);
    for (size_t i = 0; i < logLines.size(); i++) { text(bx + 10, y, logLines[i].c_str(), GLUT_BITMAP_HELVETICA_10); y -= 12; }
}
#if F_INPUTBOX

/* Entry box, bottom-left of the canvas. */
void inputBoxGeom(float& bx, float& by, float& bw, float& bh) {
    bw = std::min(470.0f, canvasW() - 20);
#if F_PIVOT
    bh = takesPivot(inputTarget) ? 124.0f : 96.0f;
#else
    bh = 96.0f;
#endif
    bx = 10; by = 10;
}
#if F_PIVOT
/* Pivot-taking transforms get an extra row with Set Pivot / Origin buttons. */
bool pivotRowShown() {
    return takesPivot(inputTarget) || inputTarget == IN_TF_PIVOT;
}
void pivotButtonGeom(float& x, float& y, float& w, float& h, float& rx, float& rw) {
    float bx, by, bw, bh; inputBoxGeom(bx, by, bw, bh);
    h = 22; y = by + 52; w = 118; rw = 64;
    x = bx + bw - 14 - w;            /* Set Pivot */
    rx = x - 6 - rw;                 /* Origin */
}
#endif

void drawInputBox() {
    float bx, by, bw, bh2; inputBoxGeom(bx, by, bw, bh2);

    glColor3f(0.12f, 0.13f, 0.16f);
    rect(bx, by, bw, bh2);
    glColor3f(0.35f, 0.60f, 0.95f);
    rectOutline(bx, by, bw, bh2);

    glColor3f(0.65f, 0.80f, 1.0f);
    text(bx + 14, by + bh2 - 22, inputPrompt.c_str());
    glColor3f(0.55f, 0.58f, 0.62f);
    text(bx + 14, by + bh2 - 40, inputHint.c_str(), GLUT_BITMAP_HELVETICA_10);
#if F_PIVOT

    if (takesPivot(inputTarget)) {
        float x, y, w, h, rx, rw; pivotButtonGeom(x, y, w, h, rx, rw);
        char pl[64];
        if (tfPivotSet) sprintf(pl, "Pivot: (%g, %g)", tfPivotX, tfPivotY);
        else            sprintf(pl, "Pivot: origin (0, 0)");
        glColor3f(0.95f, 0.80f, 0.35f);
        text(bx + 14, y + 7, pl, GLUT_BITMAP_HELVETICA_12);

        glColor3f(0.20f, 0.50f, 0.90f);
        rect(x, y, w, h);
        glColor3f(0.40f, 0.43f, 0.48f);
        rectOutline(x, y, w, h);
        glColor3f(0.95f, 0.96f, 0.98f);
        textCentred(x + w / 2, y + 7, "Set Pivot (Tab)");

        if (tfPivotSet) {
            glColor3f(0.26f, 0.28f, 0.32f);
            rect(rx, y, rw, h);
            glColor3f(0.40f, 0.43f, 0.48f);
            rectOutline(rx, y, rw, h);
            glColor3f(0.92f, 0.93f, 0.95f);
            textCentred(rx + rw / 2, y + 7, "Origin");
        }
    }
#endif

    glColor3f(0.06f, 0.07f, 0.09f);
    rect(bx + 14, by + 16, bw - 28, 26);
    glColor3f(0.40f, 0.45f, 0.52f);
    rectOutline(bx + 14, by + 16, bw - 28, 26);

    std::string shown = inputBuf + "_";
    while (shown.size() > 1 && textWidth(shown.c_str()) > bw - 44) shown.erase(0, 1);
    glColor3f(0.95f, 0.96f, 0.98f);
    text(bx + 22, by + 24, shown.c_str());
}
#endif
#if F_TRANSFORM

void drawConfirmBox() {
    float bw = std::min(480.0f, canvasW() - 20), bh2 = 90;
    float bx = 10, by = 10;

    glColor3f(0.10f, 0.11f, 0.14f);
    rect(bx, by, bw, bh2);
    glColor3f(0.90f, 0.55f, 0.15f);
    rectOutline(bx, by, bw, bh2);

    glColor3f(0.95f, 0.80f, 0.30f);
    char hdr[96];
    if (tfNewIndices.size() > 1) sprintf(hdr, "Transform applied to %d shapes! Remove the originals?", (int)tfNewIndices.size());
    else                         sprintf(hdr, "Transform applied! Remove the original?");
    text(bx + 14, by + bh2 - 22, hdr, GLUT_BITMAP_HELVETICA_12);

    glColor3f(0.70f, 0.72f, 0.78f);
    text(bx + 14, by + bh2 - 40, tfConfirmDesc.c_str(), GLUT_BITMAP_HELVETICA_10);

    glColor3f(0.55f, 0.85f, 0.55f);
    text(bx + 14, by + 14, "Y = Remove old    N = Keep both    Esc = Undo",
        GLUT_BITMAP_HELVETICA_10);
}
#endif
#if F_FILL

/* Red ring where the colour first escaped. */
void drawLeakMarker() {
    if (!leakAlertOn) return;
    float cx = sx((float)leakAlertX), cy = sy((float)leakAlertY);
    glColor3f(0.95f, 0.10f, 0.10f);
    glLineWidth(2.5f);
    for (int r = 10; r <= 18; r += 8) {
        glBegin(GL_LINE_LOOP);
        for (int k = 0; k < 32; k++) glVertex2f(cx + r * cosf(k * 6.2831853f / 32), cy + r * sinf(k * 6.2831853f / 32));
        glEnd();
    }
    glLineWidth(1.0f);
    char b[48]; sprintf(b, "leak (%d,%d)", leakAlertX, leakAlertY);
    text(cx + 22, cy - 4, b, GLUT_BITMAP_HELVETICA_12);
}

/* Alert banner, top centre of the canvas. */
void drawLeakBanner() {
    if (!leakAlertOn) return;
    char l1[96], l2[120];
    sprintf(l1, "COLOUR LEAKED at (%d, %d) - fill stopped", leakAlertX, leakAlertY);
#if F_UNDO
    if (leakAlertAlgo == FILL_BOUNDARY_8)
        sprintf(l2, "8-connected fill slipped through a diagonal step in the edge.  U = undo fill");
    else
        sprintf(l2, "The region is not closed.  U = undo fill");
#else
    if (leakAlertAlgo == FILL_BOUNDARY_8)
        sprintf(l2, "8-connected fill slipped through a diagonal step in the edge.");
    else
        sprintf(l2, "The region is not closed.");
#endif
    float w = (float)std::max(textWidth(l1, GLUT_BITMAP_HELVETICA_18), textWidth(l2)) + 40;
    float h = 58, x = (canvasW() - w) / 2.0f, y = (float)winH - h - 12;
    glColor3f(0.60f, 0.05f, 0.05f);
    rect(x, y, w, h);
    glColor3f(1.0f, 0.45f, 0.40f);
    glLineWidth(2.0f); rectOutline(x, y, w, h); glLineWidth(1.0f);
    glColor3f(1.0f, 1.0f, 1.0f);
    textCentred(x + w / 2, y + h - 24, l1, GLUT_BITMAP_HELVETICA_18);
    glColor3f(1.0f, 0.85f, 0.80f);
    textCentred(x + w / 2, y + 12, l2);
}
#endif
#if F_PIVOT

/* Crosshair at the active pivot while a pivot transform is being entered. */
void drawPivotMarker() {
    if (!pivotRowShown()) return;
    float cx = sx(tfPivotX), cy = sy(tfPivotY);
    glColor3f(0.95f, 0.55f, 0.10f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glVertex2f(cx - 10, cy); glVertex2f(cx + 10, cy);
    glVertex2f(cx, cy - 10); glVertex2f(cx, cy + 10);
    glEnd();
    glLineWidth(1.0f);
    rectOutline(cx - 5, cy - 5, 10, 10);
    char b[48]; sprintf(b, "pivot (%g,%g)", tfPivotX, tfPivotY);
    text(cx + 9, cy + 7, b, GLUT_BITMAP_HELVETICA_10);
}
#endif

void display() {
    if (darkMode) glClearColor(0.12f, 0.13f, 0.15f, 1.0f);
    else          glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, (int)canvasW(), winH);
    if (showGrid) drawGrid();
#if F_CIRCLE
    if (currentShape == SHAPE_CIRCLE) drawOctantOverlay();
#endif
#if F_FILL
    drawFills();
#endif
    drawLines();
#if F_PIVOT
    drawPivotMarker();
#endif
#if F_FILL
    drawLeakMarker();
#endif
    glDisable(GL_SCISSOR_TEST);

    buildPanel();
    drawPanel();

    drawInfoOverlay();
#if F_FILL
    drawLeakBanner();
#endif
#if F_INPUTBOX
    if (inputTarget != IN_NONE) drawInputBox();
#endif
#if F_TRANSFORM
    if (tfConfirmActive) drawConfirmBox();
#endif

    glutSwapBuffers();
}

/* ===========================================================================
   13. INPUT
   =========================================================================== */
#if F_TRANSFORM
void beginTransformInput(int tfType) {
#if F_PIVOT
    tfPivotX = tfPivotY = 0.0f; tfPivotSet = false;
#endif
    switch (tfType) {
#if F_TRANSLATE
    case TF_TRANSLATE:
        beginInput(IN_TF_TRANSLATE, "Translation: enter tx ty",
            "e.g.  50 -30      Enter = apply, Esc = cancel"); break;
#endif
#if F_SCALE
    case TF_SCALE:
        beginInput(IN_TF_SCALE, "Scaling: enter sx sy",
            "x' = sx*x,  y' = sy*y   (sy defaults to sx)"); break;
#endif
#if F_ROTATE
    case TF_ROTATE:
        beginInput(IN_TF_ROTATE, "Rotation: enter angle in degrees",
            "counter-clockwise positive"); break;
#endif
#if F_REFLECT
    case TF_REFLECT_AXIS:
        beginInput(IN_TF_REFLECT, "Reflect: enter x1 y1 x2 y2, or X / Y / XY",
            "two points on the mirror line;  X = x-axis, Y = y-axis, XY = line y=x"); break;
#endif
#if F_SHEAR
    case TF_SHEAR:
        beginInput(IN_TF_SHEAR, "Shear: enter shx shy",
            "x' = x + shx*y,  y' = y + shy*x"); break;
#endif
    }
}
#endif
#if F_PIVOT

void openPivotEntry() {
    if (!takesPivot(inputTarget)) return;
    pivotReturnTarget = inputTarget; pivotReturnBuf = inputBuf;
    pivotReturnPrompt = inputPrompt; pivotReturnHint = inputHint;
    beginInput(IN_TF_PIVOT, "Pivot point: enter px py",
        "transform is done about this point;  empty = origin (0,0);  Esc = back");
}
void closePivotEntry() {
    inputTarget = pivotReturnTarget; inputBuf = pivotReturnBuf;
    inputPrompt = pivotReturnPrompt; inputHint = pivotReturnHint;
}
#endif
#if F_ZOOM

/* ---- View helpers ---- */
void zoomAt(float mx, float my, float factor) {
    float wx = screenToWorldX(mx), wy = screenToWorldY(my);
    viewScale *= factor;
    if (viewScale < 0.1f)  viewScale = 0.1f;
    if (viewScale > 40.0f) viewScale = 40.0f;
    /* keep the world point under (mx,my) fixed on screen */
    viewOffsetX = mx - originX() - wx * viewScale;
    viewOffsetY = my - originY() - wy * viewScale;
}
void zoomCentre(float factor) { zoomAt(originX(), originY(), factor); }
void resetView() { viewScale = 1.0f; viewOffsetX = viewOffsetY = 0.0f; logMsg("View reset"); }
void fitAll() {
    bool any = false; float minx = 0, miny = 0, maxx = 0, maxy = 0;
    for (size_t i = 0; i < lines.size(); i++) {
        if (lines[i].deleted) continue;
        float a, b, c, d; shapeBBox(lines[i], a, b, c, d);
        if (!any) { minx = a; miny = b; maxx = c; maxy = d; any = true; }
        else { minx = std::min(minx, a); miny = std::min(miny, b); maxx = std::max(maxx, c); maxy = std::max(maxy, d); }
    }
    if (!any) { resetView(); return; }
    float w = std::max(maxx - minx, 1.0f), h = std::max(maxy - miny, 1.0f);
    viewScale = std::min((canvasW() - 120) / w, ((float)winH - 120) / h);
    viewScale = std::min(std::max(viewScale, 0.1f), 40.0f);
    float cx = (minx + maxx) / 2.0f, cy = (miny + maxy) / 2.0f;
    viewOffsetX = -cx * viewScale;
    viewOffsetY = -cy * viewScale;
    logMsg("Zoomed to fit all shapes");
}
#endif

/* ---- Drawing-state helpers ---- */
void cancelDrawing() {
    clickState = 0;
#if F_VERTS
    polyVerts.clear();
#endif
#if F_DRAG
    dragging = false;
#endif
}
#if F_VERTS

/* shapes drawn by clicking each vertex */
bool usesVertexClicks() {
#if F_FREEPOLY
    if (currentShape == SHAPE_FREEPOLY) return true;
#endif
#if F_TRIANGLE
#if F_DRAG
    if (currentShape == SHAPE_TRIANGLE) return !dragDraw;
#else
    if (currentShape == SHAPE_TRIANGLE) return true;
#endif
#endif
    return false;
}
#endif

/* algorithm a shape is drawn with */
int algoForShape(int s) {
#if F_CIRCLE
    if (s == SHAPE_CIRCLE)  return ALGO_MID_CIRCLE;
#endif
#if F_ELLIPSE
    if (s == SHAPE_ELLIPSE) return ALGO_MID_ELLIPSE;
#endif
    (void)s;
    return lastLineAlgo;
}

void setShape(int s) {
    currentShape = s;
    cancelDrawing();
    currentAlgo = algoForShape(s);
#if F_FILL
    if (inputMode != MODE_MOUSE && inputMode != MODE_TYPE && !fillMode) inputMode = MODE_MOUSE;
#else
    if (inputMode != MODE_MOUSE && inputMode != MODE_TYPE) inputMode = MODE_MOUSE;
#endif
    char m[64]; sprintf(m, "Shape: %s", shapeName[currentShape]); logMsg(m);
}
#if F_FREEPOLY

void finishFreePoly() {
    if (polyVerts.size() >= 3) addPolygon(currentShape, polyVerts);
    else logMsg("Need at least 3 vertices");
    polyVerts.clear(); clickState = 0;
}
#endif
#if F_VIEWOPTS

void toggleDark() {
    darkMode = !darkMode;
    if (darkMode && curR == 0 && curG == 0 && curB == 0) {
        curR = 255; curG = 255; curB = 255;
#if F_COLOUR
        colourIsCustom = true;
#endif
    }
    else if (!darkMode && curR == 255 && curG == 255 && curB == 255) {
        curR = 0; curG = 0; curB = 0;
#if F_COLOUR
        colourSel = 0; colourIsCustom = false;
#endif
    }
    logMsg(darkMode ? "Dark mode on" : "Dark mode off");
}
#endif
#if F_LINEALGO

/* Picks the line algorithm for new lines / polygon edges AND re-draws the
   already-drawn shape with it: the selected shapes if any, otherwise the
   most recently drawn line or polygon. */
void setLineAlgo(int a) {
    currentAlgo = lastLineAlgo = a;
    if (currentShape != SHAPE_LINE && !isPolyShape(currentShape)) {
#if F_LINE
        currentShape = SHAPE_LINE;
#endif
    }
    std::vector<int> targets;
#if F_SELECT
    targets = selection;
#endif
    if (targets.empty()) {
        for (int i = (int)lines.size() - 1; i >= 0; i--) {
            if (lines[i].deleted) continue;
            if (lines[i].shape == SHAPE_LINE || isPolyShape(lines[i].shape)) targets.push_back(i);
            break;
        }
    }
    int changed = 0, last = -1;
    for (size_t t = 0; t < targets.size(); t++) {
        LineObj& L = lines[targets[t]];
        if (L.shape != SHAPE_LINE && !isPolyShape(L.shape)) continue;
        if (L.algo != a) { L.algo = a; changed++; last = targets[t]; }
    }
    (void)last;
    char m[96];
    if (changed > 0) {
#if F_FILE
        saveLines();
#endif
        sprintf(m, "Algorithm: %s (re-drew %d shape%s)", algoName[a], changed, changed > 1 ? "s" : "");
#if F_ANIM
        if (animateShapes && changed == 1) startAnimation(last, captureShapePoints(lines[last]));
#endif
    }
    else sprintf(m, "Algorithm: %s", algoName[a]);
    logMsg(m);
}
#endif

void handleWidget(int id) {
    switch (id) {
#if F_FILE
#if F_FILL
    case W_SAVE:   saveLines(); saveFills(); logMsg("Saved to lines.dat and fills.dat"); break;
#else
    case W_SAVE:   saveLines(); logMsg("Saved to lines.dat"); break;
#endif
#endif
    case W_CLEAR:  clearAll(); break;
#if F_UNDO
    case W_UNDO:   undo(); break;
    case W_REDO:   redo(); break;
#endif
#if F_SELECT
    case W_DELSEL: deleteSelected(); break;
    case W_SELALL: inputMode = MODE_SELECT; cancelDrawing(); selectAll(); break;
    case W_GROUP:  groupSelection(); break;
    case W_UNGROUP: ungroupSelection(); break;
#if F_FILL
    case W_MODE_SELECT: inputMode = MODE_SELECT; cancelDrawing(); fillMode = false; logMsg("Mode: select / move"); break;
#else
    case W_MODE_SELECT: inputMode = MODE_SELECT; cancelDrawing(); logMsg("Mode: select / move"); break;
#endif
#endif
    case W_MODE_MOUSE:
        inputMode = MODE_MOUSE;
#if F_FILL
        fillMode = false;
#endif
#if F_SELECT
        selection.clear();
#endif
        logMsg("Mode: mouse click"); break;
#if F_TYPE
    case W_MODE_TYPE:
        inputMode = MODE_TYPE;
#if F_FILL
        fillMode = false;
#endif
        cancelDrawing(); typeCoords(); break;
#endif
#if F_MARKERS
    case W_EP_CUSTOM: askEndpointRGB(); break;
    case W_EP_ENABLE: endpointsOn = !endpointsOn;
        logMsg(endpointsOn ? "Endpoint markers on" : "Endpoint markers off"); break;
#endif
#if F_COLOUR
    case W_DD_COLOUR: openDropdown = (openDropdown == W_DD_COLOUR) ? 0 : W_DD_COLOUR; break;
#endif
#if F_STYLE
    case W_DD_STYLE:  openDropdown = (openDropdown == W_DD_STYLE) ? 0 : W_DD_STYLE;  break;
#endif
#if F_VIEWOPTS
    case W_GRID:    showGrid = !showGrid; break;
    case W_LABELS:  showLabels = !showLabels; break;
    case W_DARKMODE: toggleDark(); break;
#endif
#if F_ANIM
    case W_ANIMATE: animateShapes = !animateShapes;
        logMsg(animateShapes ? "Animation on" : "Animation off"); break;
#endif
#if F_SNAP
    case W_SNAP: snapToGrid = !snapToGrid;
        logMsg(snapToGrid ? "Snap to grid on" : "Snap to grid off"); break;
#endif
#if F_DRAG
    case W_DRAGDRAW: dragDraw = !dragDraw; cancelDrawing();
        logMsg(dragDraw ? "Drag-to-draw on" : "Two-click drawing"); break;
#endif
#if F_ZOOM
    case W_ZOOM_RESET: resetView(); break;
    case W_ZOOM_FIT:   fitAll(); break;
    case W_ZOOM_IN:    zoomCentre(1.25f); break;
    case W_ZOOM_OUT:   zoomCentre(1.0f / 1.25f); break;
#endif
#if F_FILL
    case W_FILL_TOGGLE:
        fillMode = !fillMode;
        if (fillMode) { inputMode = MODE_FILL; cancelDrawing(); logMsg("Fill mode ON - click inside a closed shape"); }
        else { inputMode = MODE_MOUSE; logMsg("Fill mode OFF"); }
        break;
#if F_B4
    case W_FILL_B4:    fillAlgorithm = FILL_BOUNDARY_4; fillConnectivity = 4; logMsg("Boundary fill 4-connected"); break;
#endif
#if F_B8
    case W_FILL_B8:    fillAlgorithm = FILL_BOUNDARY_8; fillConnectivity = 8; logMsg("Boundary fill 8-connected"); break;
#endif
#if F_FLOOD
    case W_FILL_FLOOD: fillAlgorithm = FILL_FLOOD;      fillConnectivity = 4; logMsg("Flood fill"); break;
#endif
#if F_SCAN
    case W_FILL_SCAN:  fillAlgorithm = FILL_SCANLINE;   fillConnectivity = 4; logMsg("Scan-line fill"); break;
#endif
    case W_FILL_COLOUR: askFillRGB(); break;
    case W_FILL_CLEAR:
        fillRegions.clear(); leakAlertOn = false;
#if F_ANIM
        fillAnimIndex = -1;
#endif
#if F_FILE
        saveFills();
#endif
        logMsg("All fills cleared"); break;
#if F_ANIM
    case W_FILL_VSLOW: case W_FILL_SLOW: case W_FILL_MED: case W_FILL_FAST:
        fillSpeed = id - W_FILL_VSLOW;
        { char m[48]; sprintf(m, "Fill animation speed: %s", fillSpeedName[fillSpeed]); logMsg(m); }
        break;
#endif
#endif
#if F_TRANSFORM
    case W_TF_TOGGLE:
        transformModeActive = !transformModeActive;
        currentTransform = TF_NONE;
#if F_COMPOSITE
        compositeQueue.clear();
#endif
        if (transformModeActive) {
            inputMode = MODE_SELECT; cancelDrawing();
#if F_FILL
            fillMode = false;
#endif
            logMsg("Transform mode ON - select shape(s)");
        }
        else logMsg("Transform mode OFF");
        break;

    case W_TF_TRANSLATE: case W_TF_SCALE: case W_TF_ROTATE: case W_TF_REFLECT: case W_TF_SHEAR: {
        int t = (id == W_TF_TRANSLATE) ? TF_TRANSLATE : (id == W_TF_SCALE) ? TF_SCALE :
                (id == W_TF_ROTATE) ? TF_ROTATE : (id == W_TF_REFLECT) ? TF_REFLECT_AXIS : TF_SHEAR;
        currentTransform = t;
        if (selection.empty()) { logMsg("Select shape(s) first (click, Shift+click or box drag)"); break; }
        beginTransformInput(t);
        break;
    }
#if F_COMPOSITE

    case W_TF_COMPOSITE:
        if (currentTransform == TF_COMPOSITE) {
            currentTransform = TF_NONE;
            compositeQueue.clear();
            logMsg("Exited composite mode");
        }
        else {
            currentTransform = TF_COMPOSITE;
            compositeQueue.clear();
            logMsg("Composite mode: add transforms then Apply");
        }
        break;

    case W_TF_COMP_TRANSLATE: beginTransformInput(TF_TRANSLATE); break;
    case W_TF_COMP_SCALE:     beginTransformInput(TF_SCALE); break;
    case W_TF_COMP_ROTATE:    beginTransformInput(TF_ROTATE); break;
    case W_TF_COMP_REFLECT:   beginTransformInput(TF_REFLECT_AXIS); break;
    case W_TF_COMP_SHEAR:     beginTransformInput(TF_SHEAR); break;

    case W_TF_COMP_APPLY:
        if (selection.empty()) { logMsg("Select shape(s) first"); break; }
        if (compositeQueue.empty()) { logMsg("Queue is empty"); break; }
        {
            /* M = Mn * ... * M2 * M1 : the first queued transform acts first */
            Mat3 combined;
            for (size_t qi = 0; qi < compositeQueue.size(); qi++)
                combined = mat3Mul(compositeQueue[qi].matrix, combined);
            char cm[80];
            sprintf(cm, "Composite (%d transforms)", (int)compositeQueue.size());
            applyTransformToSelection(combined, cm);
            compositeQueue.clear();
        }
        break;

    case W_TF_COMP_CLEAR:
        compositeQueue.clear();
        logMsg("Composite queue cleared");
        break;
#endif
#endif
#if F_POLYGON || F_STAR

    case W_SIDES_DEC: case W_SIDES_INC: {
#if F_POLYGON && F_STAR
        int& n = (currentShape == SHAPE_STAR) ? starPoints : polySides;
#elif F_POLYGON
        int& n = polySides;
#else
        int& n = starPoints;
#endif
        n = std::min(std::max(n + (id == W_SIDES_INC ? 1 : -1), 3), 24);
        char m[48]; sprintf(m, "%s: %d", currentShape == SHAPE_STAR ? "Star points" : "Polygon sides", n); logMsg(m);
        break;
    }
#endif

    default:
#if F_THICK
        if (id >= W_TH0 && id <= W_TH3) {
            curThickness = thicknessOpts[id - W_TH0];
            char m[48]; sprintf(m, "Thickness %d", curThickness); logMsg(m);
        }
#endif
        if (id >= W_SHAPE0 && id <= W_SHAPE_LAST) {
            setShape(id - W_SHAPE0);
        }
#if F_LINEALGO
        else if (id >= W_ALGO0 && id <= W_ALGO2) {
            setLineAlgo(id - W_ALGO0);
        }
#endif
        break;
    }
}
#if F_DROPDOWN

void dropdownGeom(float& bx, float& ly, float& bw, float& listH, int& n) {
    float by = 0; bx = 0; bw = 0;
    for (size_t i = 0; i < widgets.size(); i++)
        if (widgets[i].id == openDropdown) { bx = widgets[i].x; by = widgets[i].y; bw = widgets[i].w; }
#if F_COLOUR && F_STYLE
    n = (openDropdown == W_DD_COLOUR) ? NCOLOUR_OPTS + 1 : NSTYLE_OPTS + 1;
#elif F_COLOUR
    n = NCOLOUR_OPTS + 1;
#else
    n = NSTYLE_OPTS + 1;
#endif
    listH = n * 20.0f;
    ly = by - listH;
    if (ly < 4) ly = by + 22;
}

bool handleDropdownClick(float mx, float my) {
    if (openDropdown == 0) return false;

    float bx, ly, bw, listH; int n;
    dropdownGeom(bx, ly, bw, listH, n);
    const float ih = 20;

    if (mx < bx || mx > bx + bw || my < ly || my > ly + listH) { openDropdown = 0; return false; }

    int idx = (int)((ly + listH - my) / ih);
    idx = std::min(std::max(idx, 0), n - 1);
    bool isCustom = (idx == n - 1);
#if F_COLOUR

    if (openDropdown == W_DD_COLOUR) {
        if (isCustom) askLineRGB();
        else {
            colourSel = idx; colourIsCustom = false;
            curR = colourOpts[idx].r; curG = colourOpts[idx].g; curB = colourOpts[idx].b;
            char m[64]; sprintf(m, "Line colour: %s", colourOpts[idx].name); logMsg(m);
        }
    }
#endif
#if F_STYLE
    if (openDropdown == W_DD_STYLE) {
        if (isCustom) customPattern();
        else {
            styleSel = idx; styleIsCustom = false;
            curPattern = styleOpts[idx].value; curPatternBits = styleOpts[idx].bits;
            char m[64]; sprintf(m, "Line style: %s", styleOpts[idx].name); logMsg(m);
        }
    }
#endif
    openDropdown = 0;
    return true;
}
#endif
#if F_DRAG

void commitDragShape(float wx, float wy) {
    dragging = false; clickState = 0;
    if (fabsf(wx - pendingX) < 1.0f && fabsf(wy - pendingY) < 1.0f) {
        logMsg("Drag further to draw (shape too small)");
        return;
    }
#if F_LINE || F_CIRCLE
    if (currentShape == SHAPE_LINE || currentShape == SHAPE_CIRCLE) {
        addLine(pendingX, pendingY, wx, wy);
        return;
    }
#endif
#if F_ELLIPSE
    if (currentShape == SHAPE_ELLIPSE) {
        ellipseRx = std::max(1, (int)fabsf(wx - pendingX));
        ellipseRy = std::max(1, (int)fabsf(wy - pendingY));
        currentAngle = 0.0f;
        addLine(pendingX, pendingY, pendingX + ellipseRx, pendingY + ellipseRy);
        return;
    }
#endif
#if F_POLYBUILD
    addPolygon(currentShape, buildPolyVerts(currentShape, pendingX, pendingY, wx, wy));
#endif
}
#endif

/* right click without dragging: close a dropdown, finish / cancel a shape */
void rightClick() {
#if F_DROPDOWN
    if (openDropdown) { openDropdown = 0; return; }
#endif
#if F_FREEPOLY
    if (currentShape == SHAPE_FREEPOLY && polyVerts.size() >= 3) { finishFreePoly(); return; }
#endif
#if F_VERTS
    if (clickState > 0 || !polyVerts.empty()) { cancelDrawing(); logMsg("Cancelled"); }
#else
    if (clickState > 0) { cancelDrawing(); logMsg("Cancelled"); }
#endif
}

void mouse(int button, int state, int mxi, int myi) {
    float mx = (float)mxi;
    float my = (float)(winH - myi);
#if F_INPUTBOX
    if (inputTarget != IN_NONE) {
#if F_PIVOT
        /* only the entry box's own buttons are live while typing */
        if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN && takesPivot(inputTarget)) {
            float x, y, w, h, rx, rw; pivotButtonGeom(x, y, w, h, rx, rw);
            if (my >= y && my <= y + h) {
                if (mx >= x && mx <= x + w) openPivotEntry();
                else if (tfPivotSet && mx >= rx && mx <= rx + rw) {
                    tfPivotX = tfPivotY = 0.0f; tfPivotSet = false;
                    logMsg("Pivot reset to origin (0,0)");
                }
            }
            glutPostRedisplay();
        }
#endif
        return;
    }
#endif
    bool overPanel = mx >= canvasW();

    /* mouse wheel: scrolls the panel when over it */
    if (button == 3 || button == 4) {
        if (state != GLUT_DOWN) return;
        if (overPanel) { scrollPanel(button == 3 ? -40.0f : 40.0f); return; }
#if F_ELLIPSE
        if (clickState == 2 && currentShape == SHAPE_ELLIPSE) {
            ellipseRy += (button == 3) ? 1 : -1;
            if (ellipseRy < 1) ellipseRy = 1;
            glutPostRedisplay();
            return;
        }
#endif
#if F_ZOOM
        zoomAt(mx, my, (button == 3) ? 1.10f : (1.0f / 1.10f));   /* zoom at cursor */
        glutPostRedisplay();
#endif
        return;
    }
#if F_ZOOM

    /* pan: middle drag, right drag, or Space + left drag (canvas only).
       A right click without movement still acts as a click. */
    if (button == GLUT_MIDDLE_BUTTON || button == GLUT_RIGHT_BUTTON ||
        (button == GLUT_LEFT_BUTTON && spaceDown && (!overPanel || panning))) {
        if (state == GLUT_DOWN) {
            if (overPanel) { rightClick(); glutPostRedisplay(); return; }
            panning = true; panMoved = false; panButton = button;
            panStartX = mx; panStartY = my;
            panStartOffX = viewOffsetX; panStartOffY = viewOffsetY;
        }
        else if (panning && button == panButton) {
            panning = false;
            if (!panMoved && button == GLUT_RIGHT_BUTTON) rightClick();
        }
        glutPostRedisplay();
        return;
    }
#else

    if (button == GLUT_RIGHT_BUTTON) {
        if (state == GLUT_DOWN) rightClick();
        glutPostRedisplay();
        return;
    }
#endif

    if (button != GLUT_LEFT_BUTTON) return;

    /* ---- left button release ---- */
    if (state == GLUT_UP) {
        if (scrollDragging) { scrollDragging = false; }
#if F_DRAG
        else if (dragging) {
            commitDragShape(snapX(screenToWorldX(mx)), snapY(screenToWorldY(my)));
        }
#endif
#if F_SELECT
        else if (moveDragging) {
            moveDragging = false;
            if (moveDidMove) {
#if F_FILE
                saveLines();
#endif
                char m[64]; sprintf(m, "Moved %d shape(s)", (int)selection.size()); logMsg(m);
            }
        }
        else if (boxSelecting) {
            boxSelecting = false;
            if (fabsf(boxX1 - boxX0) > 4 || fabsf(boxY1 - boxY0) > 4)
                boxSelect(screenToWorldX(boxX0), screenToWorldY(boxY0),
                    screenToWorldX(boxX1), screenToWorldY(boxY1), boxAdditive);
            else if (!boxAdditive && !selection.empty()) { selection.clear(); logMsg("Selection cleared"); }
        }
#endif
        glutPostRedisplay();
        return;
    }

    /* ---- left button press ---- */
#if F_DROPDOWN
    if (openDropdown) {
        if (handleDropdownClick(mx, my)) { glutPostRedisplay(); return; }
    }
#endif

    if (overPanel) {
        float tx, ty, tw, th, thumbY, thumbH;
        if (scrollbarGeom(tx, ty, tw, th, thumbY, thumbH) && mx >= tx - 4 && my >= ty && my <= ty + th) {
            if (my > thumbY + thumbH)  scrollPanel(-panelViewH() * 0.8f);
            else if (my < thumbY)      scrollPanel(panelViewH() * 0.8f);
            scrollDragging = true;
            scrollDragStartY = my; scrollDragStartScroll = panelScroll;
            glutPostRedisplay(); return;
        }
        if (inPanelView(my)) {
            for (size_t i = 0; i < widgets.size(); i++) {
                const Widget& wd = widgets[i];
                if (mx >= wd.x && mx <= wd.x + wd.w && my >= wd.y && my <= wd.y + wd.h) {
                    handleWidget(wd.id); glutPostRedisplay(); return;
                }
            }
        }
        glutPostRedisplay(); return;
    }

    float rwx = screenToWorldX(mx), rwy = screenToWorldY(my);
    float wx = snapX(rwx), wy = snapY(rwy);
#if F_SELECT

    if (inputMode == MODE_SELECT) {
        bool shift = (glutGetModifiers() & (GLUT_ACTIVE_SHIFT | GLUT_ACTIVE_CTRL)) != 0;
        int hit = hitTest(rwx, rwy);
        if (hit >= 0) {
            if (!(isSelected(hit) && !shift)) selectShape(hit, shift);
            if (isSelected(hit)) {
                moveDragging = true; moveDidMove = false;
                moveLastX = wx; moveLastY = wy;
            }
        }
        else {
            boxSelecting = true; boxAdditive = shift;
            boxX0 = boxX1 = mx; boxY0 = boxY1 = my;
        }
        glutPostRedisplay(); return;
    }
#endif
#if F_TYPE
    if (inputMode == MODE_TYPE) { logMsg("Type Coords mode: use the panel button"); glutPostRedisplay(); return; }
#endif
#if F_FILL
    if (inputMode == MODE_FILL) { performFill(rwx, rwy); glutPostRedisplay(); return; }
#endif
#if F_VERTS

    /* vertex-by-vertex shapes: free polygon, 3-click triangle */
    if (usesVertexClicks()) {
#if F_FREEPOLY
        if (currentShape == SHAPE_FREEPOLY && polyVerts.size() >= 3 &&
            hypotf(sx(polyVerts[0].x) - mx, sy(polyVerts[0].y) - my) < 10.0f) {
            finishFreePoly(); glutPostRedisplay(); return;
        }
#endif
        if (!polyVerts.empty() && fabsf(wx - polyVerts.back().x) < 0.5f && fabsf(wy - polyVerts.back().y) < 0.5f) {
            glutPostRedisplay(); return;   /* ignore double click on the same spot */
        }
        Pt p; p.x = wx; p.y = wy;
        polyVerts.push_back(p);
        clickState = 1;
        if (currentShape == SHAPE_TRIANGLE && polyVerts.size() == 3) {
            addPolygon(SHAPE_TRIANGLE, polyVerts);
            polyVerts.clear(); clickState = 0;
        }
        else {
            char m[64]; sprintf(m, "V%d (%d,%d)", (int)polyVerts.size(), (int)wx, (int)wy); logMsg(m);
        }
        glutPostRedisplay(); return;
    }
#endif
#if F_DRAG

    /* drag-to-draw initiation */
    if (dragDraw && clickState == 0) {
        pendingX = wx; pendingY = wy;
        liveEndX = wx; liveEndY = wy;
        dragging = true;
        clickState = 1;
        glutPostRedisplay();
        return;
    }
#endif

#if F_TWOCLICK
    if (clickState == 0) {
        pendingX = wx; pendingY = wy; clickState = 1;
        const char* next = "click opposite corner";
#if F_LINE
        if (currentShape == SHAPE_LINE)    next = "click P2";
#endif
#if F_CIRCLE
        if (currentShape == SHAPE_CIRCLE)  next = "click a point on the circle";
#endif
#if F_ELLIPSE
        if (currentShape == SHAPE_ELLIPSE) next = "click to set rx";
#endif
#if F_POLYGON || F_STAR
        if (currentShape == SHAPE_POLYGON || currentShape == SHAPE_STAR) next = "click a vertex";
#endif
        char m[96];
        sprintf(m, "Point (%d,%d) - %s", (int)wx, (int)wy, next);
        logMsg(m);
    }
    else if (clickState == 1) {
#if F_ELLIPSE
        if (currentShape == SHAPE_ELLIPSE) {
            ellipseP2X = wx; ellipseP2Y = wy;
            float edx = wx - pendingX, edy = wy - pendingY;
            ellipseRx = (int)floor(sqrtf(edx * edx + edy * edy) + 0.5f);
            if (ellipseRx < 1) ellipseRx = 1;
            currentAngle = atan2f(edy, edx);
            ellipseRy = ellipseRx / 2;
            if (ellipseRy < 1) ellipseRy = 1;
            clickState = 2;
            char m[96];
            sprintf(m, "rx = %d  -  scroll to adjust, click for ry", ellipseRx);
            logMsg(m);
        }
        else
#endif
        if (fabsf(wx - pendingX) < 0.5f && fabsf(wy - pendingY) < 0.5f) {
            logMsg("Same point as the first click - pick another");
        }
#if F_LINE || F_CIRCLE
        else if (currentShape == SHAPE_LINE || currentShape == SHAPE_CIRCLE) {
            addLine(pendingX, pendingY, wx, wy);
            clickState = 0;
        }
#endif
#if F_POLYBUILD
        else {
            addPolygon(currentShape, buildPolyVerts(currentShape, pendingX, pendingY, wx, wy));
            clickState = 0;
        }
#endif
    }
#if F_ELLIPSE
    else if (clickState == 2) {
        float rdx = ellipseP2X - pendingX, rdy = ellipseP2Y - pendingY;
        float rlen = sqrtf(rdx * rdx + rdy * rdy);
        if (rlen < 1.0f) rlen = 1.0f;
        float perpUX = -rdy / rlen, perpUY = rdx / rlen;
        float dmx = wx - pendingX, dmy = wy - pendingY;
        ellipseRy = (int)floor(fabs(dmx * perpUX + dmy * perpUY) + 0.5f);
        if (ellipseRy < 1) ellipseRy = 1;

        addLine(pendingX, pendingY, pendingX + (float)ellipseRx, pendingY + (float)ellipseRy);
        clickState = 0;
    }
#endif
#endif
    (void)wx; (void)wy;
    glutPostRedisplay();
}

void motion(int mxi, int myi) {
    float mx = (float)mxi;
    float my = (float)(winH - myi);
    mouseWorldX = screenToWorldX(mx);
    mouseWorldY = screenToWorldY(my);
    (void)mx;

    if (scrollDragging) {
        float tx, ty, tw, th, thumbY, thumbH;
        if (scrollbarGeom(tx, ty, tw, th, thumbY, thumbH) && th > thumbH) {
            float perPx = panelMaxScroll / (th - thumbH);
            panelScroll = std::min(std::max(scrollDragStartScroll + (scrollDragStartY - my) * perPx, 0.0f), panelMaxScroll);
        }
        glutPostRedisplay();
        return;
    }
#if F_ZOOM
    if (panning) {
        if (!panMoved && hypotf(mx - panStartX, my - panStartY) < 3.0f) return;
        panMoved = true;
        viewOffsetX = panStartOffX + (mx - panStartX);
        viewOffsetY = panStartOffY + (my - panStartY);
        glutPostRedisplay();
        return;
    }
#endif
#if F_SELECT
    if (moveDragging) {
        float wx = snapX(mouseWorldX), wy = snapY(mouseWorldY);
        float dx = wx - moveLastX, dy = wy - moveLastY;
        if (dx != 0 || dy != 0) {
            for (size_t s = 0; s < selection.size(); s++) translateShapeInPlace(lines[selection[s]], dx, dy);
            moveLastX = wx; moveLastY = wy;
            moveDidMove = true;
        }
        glutPostRedisplay();
        return;
    }
    if (boxSelecting) {
        boxX1 = std::min(mx, canvasW()); boxY1 = my;
        glutPostRedisplay();
        return;
    }
#endif
#if F_DRAG
    if (dragging) {
        liveEndX = snapX(mouseWorldX);
        liveEndY = snapY(mouseWorldY);
    }
#endif
    glutPostRedisplay();
}

void passiveMotion(int mxi, int myi) {
    float mx = (float)mxi;
    float my = (float)(winH - myi);
    mouseWorldX = screenToWorldX(mx);
    mouseWorldY = screenToWorldY(my);
#if F_ELLIPSE

    if (currentShape == SHAPE_ELLIPSE && clickState == 2) {   /* ry follows the mouse */
        float rdx = ellipseP2X - pendingX, rdy = ellipseP2Y - pendingY;
        float rlen = sqrtf(rdx * rdx + rdy * rdy);
        if (rlen < 1.0f) rlen = 1.0f;
        float perpUX = -rdy / rlen, perpUY = rdx / rlen;
        float dmx = mouseWorldX - pendingX, dmy = mouseWorldY - pendingY;
        int newRy = (int)floor(fabs(dmx * perpUX + dmy * perpUY) + 0.5f);
        if (newRy < 1) newRy = 1;
        ellipseRy = newRy;
    }
#endif
    /* redraw for previews and the cursor readout */
    if (mx < canvasW() || clickState > 0) glutPostRedisplay();
}

void keyboard(unsigned char key, int, int) {
#if F_TRANSFORM
    if (tfConfirmActive) {
        if (key == 'y' || key == 'Y') {
            for (size_t k = 0; k < tfOldIndices.size(); k++) lines[tfOldIndices[k]].deleted = true;
            logMsg(tfOldIndices.size() > 1 ? "Old shapes removed" : "Old shape removed");
            tfConfirmActive = false; tfOldIndices.clear(); tfNewIndices.clear();
#if F_FILE
            saveLines();
#endif
        }
        else if (key == 'n' || key == 'N') {
            logMsg("Both kept");
            tfConfirmActive = false; tfOldIndices.clear(); tfNewIndices.clear();
        }
        else if (key == 27) {
            for (size_t k = 0; k < tfNewIndices.size(); k++) lines[tfNewIndices[k]].deleted = true;
            selection = tfOldIndices;
            logMsg("Transform undone");
            tfConfirmActive = false; tfOldIndices.clear(); tfNewIndices.clear();
#if F_FILE
            saveLines();
#endif
        }
        glutPostRedisplay();
        return;
    }
#endif
#if F_INPUTBOX

    if (inputTarget != IN_NONE) {
        if (key == 13) { commitInput(); }
#if F_PIVOT
        else if (key == 27 && inputTarget == IN_TF_PIVOT) { closePivotEntry(); }
#endif
        else if (key == 27) { inputTarget = IN_NONE; inputBuf.clear(); logMsg("Cancelled"); }
#if F_PIVOT
        else if (key == 9) { openPivotEntry(); }   /* Tab */
#endif
        else if (key == 8 || key == 127) {
            if (!inputBuf.empty()) inputBuf.erase(inputBuf.size() - 1);
        }
        else if (key >= 32 && key < 127 && inputBuf.size() < 240) {
            inputBuf.push_back((char)key);
        }
        glutPostRedisplay();
        return;
    }
#endif

#if F_FILL && F_ANIM
    if (key == ' ' && fillAnimIndex >= 0) { toggleFillPause(); glutPostRedisplay(); return; }
#endif

    switch (key) {
#if F_BRES
    case '1': setLineAlgo(ALGO_BRESENHAM); break;
#endif
#if F_SDDA
    case '2': setLineAlgo(ALGO_SYMMETRIC); break;
#endif
#if F_DDA
    case '3': setLineAlgo(ALGO_SIMPLE); break;
#endif
#if F_LINE
    case '4': setShape(SHAPE_LINE); break;
#endif
#if F_CIRCLE
    case '5': setShape(SHAPE_CIRCLE); break;
#endif
#if F_ELLIPSE
    case '6': setShape(SHAPE_ELLIPSE); break;
#endif
#if F_RECT
    case '7': setShape(SHAPE_RECTANGLE); break;
#endif
#if F_SQUARE
    case '8': setShape(SHAPE_SQUARE); break;
#endif
#if F_TRIANGLE
    case '9': setShape(SHAPE_TRIANGLE); break;
#endif
#if F_POLYGON
    case 'p': case 'P': setShape(SHAPE_POLYGON); break;
#endif
#if F_STAR
    case 'o': case 'O': setShape(SHAPE_STAR); break;
#endif
#if F_FREEPOLY
    case 'i': case 'I': setShape(SHAPE_FREEPOLY); break;
    case 13:  if (!polyVerts.empty()) finishFreePoly(); break;
#endif
#if F_VERTS
    case 8:   /* Backspace: remove the last placed vertex */
        if (!polyVerts.empty()) { polyVerts.pop_back(); clickState = polyVerts.empty() ? 0 : 1; logMsg("Removed last vertex"); }
        break;
#endif
#if F_SELECT
    case 'm': case 'M': handleWidget(W_MODE_SELECT); break;
    case 'a': case 'A': case 1 /* Ctrl+A */: handleWidget(W_SELALL); break;
    case 7 /* Ctrl+G */: groupSelection(); break;
    case 127: deleteSelected(); break;   /* Delete */
#endif
#if F_UNDO
    case 'u': case 'U': undo(); break;
    case 'y': case 'Y': redo(); break;
#endif
    case 'n': case 'N': clearAll(); break;
#if F_TYPE
    case 't': case 'T': inputMode = MODE_TYPE; cancelDrawing(); typeCoords(); break;
#endif
#if F_STYLE
    case 'h': case 'H': customPattern(); break;
    case '[': if (curPatternScale > 1)  curPatternScale--; break;
    case ']': if (curPatternScale < 12) curPatternScale++; break;
#endif
#if F_VIEWOPTS
    case 'g': case 'G': showGrid = !showGrid; break;
    case 'l': case 'L': showLabels = !showLabels; break;
    case 'd': case 'D': toggleDark(); break;
#endif
#if F_ANIM
    case 'v': case 'V': animateShapes = !animateShapes;
        logMsg(animateShapes ? "Animation on" : "Animation off"); break;
#endif
#if F_FILE
#if F_FILL
    case 's': case 'S': saveLines(); saveFills(); logMsg("Saved"); break;
#else
    case 's': case 'S': saveLines(); logMsg("Saved"); break;
#endif
#endif
#if F_FILL
    case 'f': case 'F': handleWidget(W_FILL_TOGGLE); break;
#endif
#if F_SNAP
    case 'b': case 'B': snapToGrid = !snapToGrid;
        logMsg(snapToGrid ? "Snap on" : "Snap off"); break;
#endif
#if F_DRAG
    case 'k': case 'K': handleWidget(W_DRAGDRAW); break;
#endif
#if F_ZOOM
    case 'z': case 'Z': fitAll(); break;
    case '+': case '=': zoomCentre(1.15f); break;
    case '-': case '_': zoomCentre(1.0f / 1.15f); break;
    case '0': resetView(); break;
    case ' ':
        spaceDown = true; break;
#endif
    case 27:
#if F_DRAG
        if (clickState > 0 || dragging) { cancelDrawing(); logMsg("Cancelled"); break; }
#else
        if (clickState > 0) { cancelDrawing(); logMsg("Cancelled"); break; }
#endif
#if F_VERTS
        if (!polyVerts.empty()) { cancelDrawing(); logMsg("Cancelled"); break; }
#endif
#if F_DROPDOWN
        if (openDropdown) { openDropdown = 0; break; }
#endif
#if F_SELECT
        if (!selection.empty()) { selection.clear(); logMsg("Selection cleared"); break; }
#endif
        exit(0);
    }
    glutPostRedisplay();
}
#if F_ZOOM

void keyboardUp(unsigned char key, int, int) {
    if (key == ' ') spaceDown = false;
}
#endif

void special(int key, int, int) {
#if F_FILL && F_ANIM
    if (key == GLUT_KEY_RIGHT && fillAnimIndex >= 0 && fillAnimPaused) { stepFill(); glutPostRedisplay(); return; }
#endif
    switch (key) {
#if F_ZOOM
    case GLUT_KEY_LEFT:  viewOffsetX += 20; break;
    case GLUT_KEY_RIGHT: viewOffsetX -= 20; break;
    case GLUT_KEY_UP:    viewOffsetY -= 20; break;
    case GLUT_KEY_DOWN:  viewOffsetY += 20; break;
#endif
    case GLUT_KEY_PAGE_UP:   scrollPanel(-panelViewH() * 0.8f); break;
    case GLUT_KEY_PAGE_DOWN: scrollPanel(panelViewH() * 0.8f); break;
    case GLUT_KEY_HOME:      scrollPanel(-panelMaxScroll); break;
    case GLUT_KEY_END:       scrollPanel(panelMaxScroll); break;
#if F_TRANSFORM
    case GLUT_KEY_F7:
        handleWidget(W_TF_TOGGLE);
        break;
#endif
#if F_FILL
    case GLUT_KEY_F8:
        handleWidget(W_FILL_TOGGLE);
        break;
#endif
    }
    glutPostRedisplay();
}

/* ===========================================================================
   14. SETUP
   =========================================================================== */
void reshape(int w, int h) {
    winW = w; winH = h;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, w, 0, h);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    buildPanel();
}

int main(int argc, char** argv) {
    std::cout << "==============================================\n";
    std::cout << "   SHAPE DRAWING TOOL\n";
    std::cout << "==============================================\n";
    std::cout << "Keys:\n";
#if F_LINE
    std::cout << "  4  line\n";
#endif
#if F_CIRCLE
    std::cout << "  5  circle\n";
#endif
#if F_ELLIPSE
    std::cout << "  6  ellipse\n";
#endif
#if F_RECT
    std::cout << "  7  rectangle\n";
#endif
#if F_SQUARE
    std::cout << "  8  square\n";
#endif
#if F_TRIANGLE
    std::cout << "  9  triangle\n";
#endif
#if F_POLYGON
    std::cout << "  P  regular polygon\n";
#endif
#if F_STAR
    std::cout << "  O  star\n";
#endif
#if F_FREEPOLY
    std::cout << "  I  free polygon (Enter / right-click closes it)\n";
#endif
#if F_BRES
    std::cout << "  1  Bresenham\n";
#endif
#if F_SDDA
    std::cout << "  2  Symmetric DDA\n";
#endif
#if F_DDA
    std::cout << "  3  Simple DDA\n";
#endif
#if F_SELECT
    std::cout << "  M  select / move    A select all    Ctrl+G group    Del delete selected\n";
#endif
#if F_TYPE
    std::cout << "  T  type coordinates\n";
#endif
#if F_STYLE
    std::cout << "  H  hex line pattern    [ ]  pattern scale\n";
#endif
#if F_SNAP
    std::cout << "  B  snap to grid\n";
#endif
#if F_DRAG
    std::cout << "  K  drag-to-draw\n";
#endif
#if F_FILL
    std::cout << "  F / F8  fill mode\n";
#endif
#if F_TRANSFORM
    std::cout << "  F7  transform mode\n";
#endif
#if F_ZOOM
    std::cout << "  wheel / + -  zoom    Z fit all    0 reset view\n";
    std::cout << "  right / middle / Space+drag  pan    arrows  pan\n";
#endif
#if F_UNDO
    std::cout << "  U  undo    Y  redo\n";
#endif
#if F_FILE
    std::cout << "  S  save\n";
#endif
#if F_VIEWOPTS
    std::cout << "  G  grid    L  labels    D  dark mode\n";
#endif
#if F_ANIM
    std::cout << "  V  animation on/off\n";
#endif
    std::cout << "  N  clear all    Esc  cancel / quit\n";
    std::cout << "  wheel over panel / PgUp PgDn  scroll the panel\n\n";

    currentAlgo = algoForShape(currentShape);
#if F_LINEALGO && !(F_BRES || F_SDDA || F_DDA)
    logMsg("WARNING: no line algorithm in this program -");
    logMsg("  lines / polygon edges will not be drawn.");
#endif
#if F_FILE
    loadLines();
#if F_FILL
    loadFills();
#endif
    char m[80];
    sprintf(m, "Loaded %d shape(s)", (int)lines.size());
    logMsg(m);
#endif

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(winW, winH);
    glutInitWindowPosition(40, 30);
    glutCreateWindow("Shape Drawing Tool");

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutPassiveMotionFunc(passiveMotion);
    glutKeyboardFunc(keyboard);
#if F_ZOOM
    glutKeyboardUpFunc(keyboardUp);
#endif
    glutSpecialFunc(special);

    glutMainLoop();
    return 0;
}
