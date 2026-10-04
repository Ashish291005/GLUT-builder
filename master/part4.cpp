
/* ===========================================================================
   10. CONTROL PANEL
   =========================================================================== */
enum WidgetId {
    W_NONE = 0,
    W_CLEAR,
#if F_FILE
    W_SAVE,
#endif
#if F_UNDO
    W_UNDO, W_REDO,
#endif
#if F_SELECT
    W_DELSEL, W_SELALL, W_GROUP, W_UNGROUP, W_MODE_SELECT,
#endif
#if F_TYPE
    W_MODE_TYPE,
#endif
    W_MODE_MOUSE,
#if F_MARKERS
    W_EP_CUSTOM, W_EP_ENABLE,
#endif
#if F_COLOUR
    W_DD_COLOUR,
#endif
#if F_STYLE
    W_DD_STYLE,
#endif
#if F_THICK
    W_TH0, W_TH1, W_TH2, W_TH3,
#endif
    W_SHAPE0, W_SHAPE_LAST = W_SHAPE0 + NSHAPES - 1,
#if F_POLYGON || F_STAR
    W_SIDES_DEC, W_SIDES_INC,
#endif
    W_ALGO0, W_ALGO1, W_ALGO2,
#if F_VIEWOPTS
    W_GRID, W_LABELS, W_DARKMODE,
#endif
#if F_ANIM
    W_ANIMATE,
#endif
#if F_SNAP
    W_SNAP,
#endif
#if F_DRAG
    W_DRAGDRAW,
#endif
#if F_ZOOM
    W_ZOOM_RESET, W_ZOOM_FIT, W_ZOOM_IN, W_ZOOM_OUT,
#endif
#if F_FILL
    W_FILL_TOGGLE, W_FILL_B4, W_FILL_B8, W_FILL_FLOOD, W_FILL_SCAN,
    W_FILL_COLOUR, W_FILL_CLEAR,
#if F_ANIM
    W_FILL_VSLOW, W_FILL_SLOW, W_FILL_MED, W_FILL_FAST,
#endif
#endif
#if F_TRANSFORM
    W_TF_TOGGLE,
    W_TF_TRANSLATE, W_TF_SCALE, W_TF_ROTATE, W_TF_REFLECT, W_TF_SHEAR,
#if F_COMPOSITE
    W_TF_COMPOSITE,
    W_TF_COMP_APPLY, W_TF_COMP_CLEAR,
    W_TF_COMP_TRANSLATE, W_TF_COMP_SCALE, W_TF_COMP_ROTATE, W_TF_COMP_REFLECT, W_TF_COMP_SHEAR,
#endif
#endif
    W_LAST
};

struct Widget { float x = 0, y = 0, w = 0, h = 0; std::string label; int id = 0; bool active = false; };
std::vector<Widget> widgets;
#if F_DROPDOWN

int  openDropdown = 0;
#endif

void addWidget(float x, float y, float w, float h, const char* label, int id, bool active) {
    Widget wd; wd.x = x; wd.y = y; wd.w = w; wd.h = h;
    wd.label = label; wd.id = id; wd.active = active;
    widgets.push_back(wd);
}
struct Section { float y = 0; std::string name; };
std::vector<Section> sections;
struct PanelText { float x = 0, y = 0; std::string s; float r = 0, g = 0, b = 0; };
std::vector<PanelText> panelTexts;
void addPanelText(float x, float y, const std::string& s, float r, float g, float b) {
    PanelText t; t.x = x; t.y = y; t.s = s; t.r = r; t.g = g; t.b = b;
    panelTexts.push_back(t);
}

/* Scrollable region of the panel, below the fixed header. */
float panelViewTop() { return (float)winH - PANEL_HEADER_H; }
float panelViewBottom() { return 0.0f; }
float panelViewH() { return std::max(0.0f, panelViewTop() - panelViewBottom()); }
bool  inPanelView(float my) { return my >= panelViewBottom() && my <= panelViewTop(); }

/* Lays out buttons in rows of `perRow` equal-width buttons. Returns new y. */
float addButtonRows(float px, float y, float pw, float bh, float gap, int count, int perRow,
    const char* const* labels, const int* ids, const bool* active) {
    for (int i = 0; i < count; i++) {
        int col = i % perRow;
        if (col == 0) y -= bh + (i ? gap : 0);
        int inRow = std::min(perRow, count - (i - col));
        float cw = (pw - (inRow - 1) * 4) / (float)inRow;
        addWidget(px + col * (cw + 4), y, cw, bh, labels[i], ids[i], active[i]);
    }
    return y;
}

void buildPanelAt(float scroll) {
    widgets.clear(); sections.clear(); panelTexts.clear();

    const float px = winW - PANEL_W + 12;
    const float pw = PANEL_W - 26;
    const float bh = 22;
    const float gap = 4;
    const float sgap = 8;
    const float lab = 15;

    const float top = panelViewTop() - 4 + scroll;
    float y = top;

    /* Actions */
    sections.push_back({ y, "ACTIONS" }); y -= lab;
#if F_FILE
    y -= bh; addWidget(px, y, pw, bh, "Save to File", W_SAVE, false); y -= gap;
#endif
    y -= bh; addWidget(px, y, pw, bh, "Clear All", W_CLEAR, false);   y -= gap;
#if F_SELECT
    y -= bh;
    {
        char dl[40]; sprintf(dl, selection.size() > 1 ? "Delete Selected (%d)" : "Delete Selected", (int)selection.size());
        addWidget(px, y, pw, bh, dl, W_DELSEL, !selection.empty());
    }
    y -= gap;
#endif
#if F_UNDO
    y -= bh;
    addWidget(px, y, pw / 2 - 2, bh, "Undo", W_UNDO, false);
    addWidget(px + pw / 2 + 2, y, pw / 2 - 2, bh, "Redo", W_REDO, false);
    y -= gap;
#endif
#if F_SELECT
    {
        const char* l[3] = { "Select All", "Group", "Ungroup" };
        int id[3] = { W_SELALL, W_GROUP, W_UNGROUP };
        bool ac[3] = { false, false, false };
        y = addButtonRows(px, y, pw, bh, gap, 3, 3, l, id, ac);
    }
    y -= gap;
#endif
    y -= sgap;
#if F_SELECT || F_TYPE

    /* Input mode */
    sections.push_back({ y, "INPUT MODE" }); y -= lab;
#if F_SELECT
    y -= bh; addWidget(px, y, pw, bh, "Select / Move", W_MODE_SELECT, inputMode == MODE_SELECT); y -= gap;
#endif
#if F_TYPE
    y -= bh; addWidget(px, y, pw, bh, "Type Coords", W_MODE_TYPE, inputMode == MODE_TYPE); y -= gap;
#endif
    y -= bh; addWidget(px, y, pw, bh, "Mouse Click", W_MODE_MOUSE, inputMode == MODE_MOUSE);
    y -= gap;
#if F_SELECT
    if (inputMode == MODE_SELECT) {
        y -= 12; addPanelText(px, y, "Shift+click add  |  drag empty = box", 0.60f, 0.65f, 0.72f);
        y -= 12; addPanelText(px, y, "drag a selected shape to move it", 0.60f, 0.65f, 0.72f);
        y -= 2;
    }
#endif
    y -= sgap;
#endif

#if F_ANYSHAPE
    /* Shape */
    sections.push_back({ y, "SHAPE" }); y -= lab;
    {
        const int order[] = {
#if F_LINE
            SHAPE_LINE,
#endif
#if F_CIRCLE
            SHAPE_CIRCLE,
#endif
#if F_ELLIPSE
            SHAPE_ELLIPSE,
#endif
#if F_RECT
            SHAPE_RECTANGLE,
#endif
#if F_SQUARE
            SHAPE_SQUARE,
#endif
#if F_TRIANGLE
            SHAPE_TRIANGLE,
#endif
#if F_DIAMOND
            SHAPE_DIAMOND,
#endif
#if F_POLYGON
            SHAPE_POLYGON,
#endif
#if F_STAR
            SHAPE_STAR,
#endif
#if F_FREEPOLY
            SHAPE_FREEPOLY,
#endif
        };
        const int n = (int)(sizeof(order) / sizeof(order[0]));
        const char* l[NSHAPES]; int id[NSHAPES]; bool ac[NSHAPES];
        for (int i = 0; i < n; i++) {
            l[i] = shapeName[order[i]]; id[i] = W_SHAPE0 + order[i]; ac[i] = (currentShape == order[i]);
        }
        y = addButtonRows(px, y, pw, bh, gap, n, 3, l, id, ac);
    }
    y -= gap;
#if F_POLYGON || F_STAR
    if (currentShape == SHAPE_POLYGON || currentShape == SHAPE_STAR) {
        y -= bh;
        addWidget(px, y, 36, bh, "-", W_SIDES_DEC, false);
        addWidget(px + pw - 36, y, 36, bh, "+", W_SIDES_INC, false);
        char sl[40] = "";
#if F_POLYGON
        if (currentShape == SHAPE_POLYGON) sprintf(sl, "Sides: %d", polySides);
#endif
#if F_STAR
        if (currentShape == SHAPE_STAR) sprintf(sl, "Points: %d", starPoints);
#endif
        addPanelText(px + pw / 2 - textWidth(sl) / 2.0f, y + 8, sl, 0.92f, 0.93f, 0.95f);
        y -= gap;
    }
#endif
    {
        const char* hint = "";
        switch (currentShape) {
#if F_LINE
        case SHAPE_LINE:      hint = "click P1, click P2"; break;
#endif
#if F_CIRCLE
        case SHAPE_CIRCLE:    hint = "click centre, click rim"; break;
#endif
#if F_ELLIPSE
        case SHAPE_ELLIPSE:   hint = "centre, rx point, then ry"; break;
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
                              hint = "click corner, click opposite corner"; break;
#endif
#if F_TRIANGLE
#if F_DRAG
        case SHAPE_TRIANGLE:  hint = dragDraw ? "drag a box (isosceles)" : "click 3 vertices"; break;
#else
        case SHAPE_TRIANGLE:  hint = "click 3 vertices"; break;
#endif
#endif
#if F_POLYGON
        case SHAPE_POLYGON:
#endif
#if F_STAR
        case SHAPE_STAR:
#endif
#if F_POLYGON || F_STAR
                              hint = "click centre, click a vertex"; break;
#endif
#if F_FREEPOLY
        case SHAPE_FREEPOLY:  hint = "click points; Enter/right-click/1st pt"; break;
#endif
        }
        y -= 12; addPanelText(px, y, hint, 0.60f, 0.65f, 0.72f);
        y -= 2;
    }
    y -= sgap;

    /* Algorithm */
#if F_LINEALGO
    if (currentShape == SHAPE_LINE || isPolyShape(currentShape)) {
        sections.push_back({ y, currentShape == SHAPE_LINE ? "ALGORITHM" : "EDGE ALGORITHM" }); y -= lab;
        const char* l[3]; int id[3]; bool ac[3]; int n = 0;
#if F_BRES
        l[n] = "Bresenham";  id[n] = W_ALGO0 + ALGO_BRESENHAM; ac[n++] = (currentAlgo == ALGO_BRESENHAM);
#endif
#if F_SDDA
        l[n] = "Sym DDA";    id[n] = W_ALGO0 + ALGO_SYMMETRIC; ac[n++] = (currentAlgo == ALGO_SYMMETRIC);
#endif
#if F_DDA
        l[n] = "Simple DDA"; id[n] = W_ALGO0 + ALGO_SIMPLE;    ac[n++] = (currentAlgo == ALGO_SIMPLE);
#endif
#if !(F_BRES || F_SDDA || F_DDA)
        y -= 12; addPanelText(px, y, "none in this program - edges are NOT drawn", 1.0f, 0.45f, 0.40f);
        y -= 12; addPanelText(px, y, "(tick a line algorithm in the builder)", 1.0f, 0.45f, 0.40f);
#endif
        y = addButtonRows(px, y, pw, bh, gap, n, 3, l, id, ac);
        y -= gap;
    }
    else
#endif
    {
        sections.push_back({ y, "ALGORITHM (FIXED)" }); y -= lab;
        y -= bh;
        addWidget(px, y, pw, bh, algoName[currentAlgo], W_NONE, true);
        y -= gap;
    }
    y -= sgap;
#endif
#if F_SNAP || F_DRAG

    /* Drawing helpers */
    sections.push_back({ y, "DRAWING HELPERS" }); y -= lab;
    {
        const char* l[2]; int id[2]; bool ac[2]; int n = 0;
#if F_SNAP
        l[n] = snapToGrid ? "Snap ON" : "Snap OFF"; id[n] = W_SNAP; ac[n++] = snapToGrid;
#endif
#if F_DRAG
        l[n] = dragDraw ? "Drag ON" : "Drag OFF"; id[n] = W_DRAGDRAW; ac[n++] = dragDraw;
#endif
        y = addButtonRows(px, y, pw, bh, gap, n, 2, l, id, ac);
    }
    y -= gap + sgap;
#endif
#if F_ZOOM

    /* View: zoom / pan */
    sections.push_back({ y, "ZOOM / PAN" }); y -= lab;
    {
        const char* l[4] = { "Zoom +", "Zoom -", "Fit All", "Reset" };
        int id[4] = { W_ZOOM_IN, W_ZOOM_OUT, W_ZOOM_FIT, W_ZOOM_RESET };
        bool ac[4] = { false, false, false, false };
        y = addButtonRows(px, y, pw, bh, gap, 4, 4, l, id, ac);
    }
    y -= 12; addPanelText(px, y, "wheel = zoom at cursor", 0.60f, 0.65f, 0.72f);
    y -= 12; addPanelText(px, y, "right/middle drag or Space+drag = pan", 0.60f, 0.65f, 0.72f);
    y -= 2 + sgap;
#endif
#if F_TRANSFORM

    /* Transforms */
    sections.push_back({ y, "2D TRANSFORMS" }); y -= lab;
    y -= bh;
    addWidget(px, y, pw, bh,
        transformModeActive ? "[Transform Mode ON]" : "Enable Transform Mode",
        W_TF_TOGGLE, transformModeActive);
    y -= gap;

    if (transformModeActive) {
        {
            char sl[64]; sprintf(sl, "%d shape(s) selected", (int)selection.size());
            y -= 12; addPanelText(px, y, sl, 0.95f, 0.80f, 0.35f);
            y -= 2;
        }
#if F_COMPOSITE
        if (currentTransform != TF_COMPOSITE) {
#endif
            const char* l[5]; int id[5]; bool ac[5]; int n = 0;
#if F_TRANSLATE
            l[n] = "Translate"; id[n] = W_TF_TRANSLATE; ac[n++] = (currentTransform == TF_TRANSLATE);
#endif
#if F_SCALE
            l[n] = "Scale";     id[n] = W_TF_SCALE;     ac[n++] = (currentTransform == TF_SCALE);
#endif
#if F_ROTATE
            l[n] = "Rotate";    id[n] = W_TF_ROTATE;    ac[n++] = (currentTransform == TF_ROTATE);
#endif
#if F_REFLECT
            l[n] = "Reflect";   id[n] = W_TF_REFLECT;   ac[n++] = (currentTransform == TF_REFLECT_AXIS);
#endif
#if F_SHEAR
            l[n] = "Shear";     id[n] = W_TF_SHEAR;     ac[n++] = (currentTransform == TF_SHEAR);
#endif
            y = addButtonRows(px, y, pw, bh, gap, n, 3, l, id, ac);
            y -= gap;
#if F_COMPOSITE
            y -= bh;
            addWidget(px, y, pw, bh, "Composite Mode", W_TF_COMPOSITE, false);
            y -= gap;
        }
        else {
            const char* l[5]; int id[5]; bool ac[5] = { false, false, false, false, false }; int n = 0;
#if F_TRANSLATE
            l[n] = "+T";  id[n++] = W_TF_COMP_TRANSLATE;
#endif
#if F_SCALE
            l[n] = "+S";  id[n++] = W_TF_COMP_SCALE;
#endif
#if F_ROTATE
            l[n] = "+R";  id[n++] = W_TF_COMP_ROTATE;
#endif
#if F_REFLECT
            l[n] = "+X";  id[n++] = W_TF_COMP_REFLECT;
#endif
#if F_SHEAR
            l[n] = "+Sh"; id[n++] = W_TF_COMP_SHEAR;
#endif
            y = addButtonRows(px, y, pw, bh, gap, n, 5, l, id, ac);
            y -= gap;
            y -= bh;
            {
                float hw = (pw - 4) / 2.0f;
                char applabel[40];
                sprintf(applabel, "Apply (%d)", (int)compositeQueue.size());
                addWidget(px, y, hw, bh, applabel, W_TF_COMP_APPLY, !compositeQueue.empty());
                addWidget(px + hw + 4, y, hw, bh, "Clear Q", W_TF_COMP_CLEAR, false);
            }
            y -= gap;
            y -= bh;
            addWidget(px, y, pw, bh, "Exit Composite", W_TF_COMPOSITE, true);
            y -= gap;
            if (!compositeQueue.empty()) {
                y -= 13; addPanelText(px, y, "COMPOSITE QUEUE:", 0.80f, 0.60f, 0.20f);
                for (size_t qi = 0; qi < compositeQueue.size(); qi++) {
                    char qline[96];
                    sprintf(qline, " %d. %s", (int)(qi + 1), compositeQueue[qi].desc.c_str());
                    y -= 13; addPanelText(px, y, qline, 0.90f, 0.80f, 0.40f);
                }
                y -= 2;
            }
        }
#endif
    }
    y -= sgap;
#endif
#if F_FILL

    /* Fill */
    sections.push_back({ y, "FILL" }); y -= lab;
    y -= bh; addWidget(px, y, pw, bh,
        fillMode ? "[Fill Mode ON]" : "Enable Fill Mode",
        W_FILL_TOGGLE, fillMode); y -= gap;
    if (fillMode) {
        {
            const char* l[4]; int id[4]; bool ac[4]; int n = 0;
#if F_B4
            l[n] = "B4";    id[n] = W_FILL_B4;    ac[n++] = (fillAlgorithm == FILL_BOUNDARY_4);
#endif
#if F_B8
            l[n] = "B8";    id[n] = W_FILL_B8;    ac[n++] = (fillAlgorithm == FILL_BOUNDARY_8);
#endif
#if F_FLOOD
            l[n] = "Flood"; id[n] = W_FILL_FLOOD; ac[n++] = (fillAlgorithm == FILL_FLOOD);
#endif
#if F_SCAN
            l[n] = "Scan";  id[n] = W_FILL_SCAN;  ac[n++] = (fillAlgorithm == FILL_SCANLINE);
#endif
            y = addButtonRows(px, y, pw, bh, gap, n, 4, l, id, ac);
        }
        y -= gap;
        y -= bh;
        {
            float cw = (pw - 4) / 2.0f;
            addWidget(px, y, cw, bh, "Fill Colour", W_FILL_COLOUR, false);
            addWidget(px + cw + 4, y, cw, bh, "Clear Fills", W_FILL_CLEAR, false);
        }
        y -= gap;
#if F_ANIM
        y -= 12; addPanelText(px, y, "Fill animation speed (needs Anim ON):", 0.60f, 0.65f, 0.72f);
        y -= 4;
        {
            const char* l[4] = { "V.Slow", "Slow", "Medium", "Fast" };
            int id[4] = { W_FILL_VSLOW, W_FILL_SLOW, W_FILL_MED, W_FILL_FAST };
            bool ac[4] = { fillSpeed == 0, fillSpeed == 1, fillSpeed == 2, fillSpeed == 3 };
            y = addButtonRows(px, y, pw, bh, gap, 4, 4, l, id, ac);
        }
        y -= 12; addPanelText(px, y, "while filling: Space pause, Right arrow step", 0.60f, 0.65f, 0.72f);
        y -= 2 + gap;
#endif
    }
    y -= sgap;
#endif
#if F_COLOUR

    /* Line colour */
    sections.push_back({ y, "LINE COLOR" }); y -= lab;
    y -= bh; addWidget(px, y, pw, bh, colourIsCustom ? "Custom" : colourOpts[colourSel].name,
        W_DD_COLOUR, openDropdown == W_DD_COLOUR);
    y -= gap + sgap;
#endif
#if F_STYLE

    /* Line style */
    sections.push_back({ y, "LINE STYLE" }); y -= lab;
    y -= bh; addWidget(px, y, pw, bh, styleIsCustom ? "Custom hex" : styleOpts[styleSel].name,
        W_DD_STYLE, openDropdown == W_DD_STYLE);
    y -= gap + sgap;
#endif
#if F_THICK

    /* Thickness */
    sections.push_back({ y, "THICKNESS" }); y -= lab;
    {
        const char* l[NTHICK]; int id[NTHICK]; bool ac[NTHICK];
        char txt[NTHICK][8];
        for (int i = 0; i < NTHICK; i++) {
            sprintf(txt[i], "%d", thicknessOpts[i]);
            l[i] = txt[i]; id[i] = W_TH0 + i; ac[i] = (curThickness == thicknessOpts[i]);
        }
        y = addButtonRows(px, y, pw, bh, gap, NTHICK, NTHICK, l, id, ac);
    }
    y -= gap + sgap;
#endif
#if F_MARKERS

    /* Endpoint markers */
    sections.push_back({ y, "ENDPOINT MARKERS" }); y -= lab;
    y -= bh;
    {
        float cw = (pw - 4) / 2.0f;
        addWidget(px, y, cw, bh, "Custom r,g,b", W_EP_CUSTOM, false);
        addWidget(px + cw + 4, y, cw, bh, endpointsOn ? "Enabled" : "Disabled", W_EP_ENABLE, endpointsOn);
    }
    y -= gap + sgap;
#endif
#if F_VIEWOPTS || F_ANIM

    /* View */
    sections.push_back({ y, "VIEW" }); y -= lab;
    {
        const char* l[4]; int id[4]; bool ac[4]; int n = 0;
#if F_VIEWOPTS
        l[n] = "Grid";   id[n] = W_GRID;     ac[n++] = showGrid;
        l[n] = "Labels"; id[n] = W_LABELS;   ac[n++] = showLabels;
#endif
#if F_ANIM
        l[n] = "Anim";   id[n] = W_ANIMATE;  ac[n++] = animateShapes;
#endif
#if F_VIEWOPTS
        l[n] = "Dark";   id[n] = W_DARKMODE; ac[n++] = darkMode;
#endif
        y = addButtonRows(px, y, pw, bh, gap, n, 4, l, id, ac);
    }
#endif
    y -= 10;

    panelContentH = top - y;
}

void buildPanel() {
    buildPanelAt(panelScroll);
    panelMaxScroll = std::max(0.0f, panelContentH - panelViewH() + 4);
    float clamped = std::min(std::max(panelScroll, 0.0f), panelMaxScroll);
    if (clamped != panelScroll) { panelScroll = clamped; buildPanelAt(panelScroll); }
}

void scrollPanel(float dy) {
    panelScroll = std::min(std::max(panelScroll + dy, 0.0f), panelMaxScroll);
#if F_DROPDOWN
    openDropdown = 0;
#endif
    glutPostRedisplay();
}

/* Scrollbar geometry (track + thumb) in the panel's right margin. */
bool scrollbarGeom(float& tx, float& ty, float& tw, float& th, float& thumbY, float& thumbH) {
    if (panelMaxScroll <= 0) return false;
    tx = winW - 9.0f; tw = 6.0f;
    ty = panelViewBottom() + 2; th = panelViewH() - 4;
    thumbH = std::max(24.0f, th * panelViewH() / std::max(1.0f, panelContentH));
    float t = panelScroll / panelMaxScroll;        /* 0 = top */
    thumbY = ty + (th - thumbH) * (1.0f - t);
    return true;
}

void drawPanel() {
    float px = winW - PANEL_W;

    glColor3f(0.14f, 0.15f, 0.18f);
    rect(px, 0, PANEL_W, (float)winH);

    /* scrollable body */
    glEnable(GL_SCISSOR_TEST);
    glScissor((int)px, (int)panelViewBottom(), (int)PANEL_W, (int)panelViewH());

    glColor3f(0.42f, 0.58f, 0.80f);
    for (size_t i = 0; i < sections.size(); i++)
        text(px + 12, sections[i].y - 11, sections[i].name.c_str(), GLUT_BITMAP_HELVETICA_10);

    for (size_t i = 0; i < panelTexts.size(); i++) {
        const PanelText& t = panelTexts[i];
        glColor3f(t.r, t.g, t.b);
        text(t.x, t.y, t.s.c_str(), GLUT_BITMAP_HELVETICA_10);
    }

    for (size_t i = 0; i < widgets.size(); i++) {
        const Widget& wd = widgets[i];
        if (wd.y + wd.h < panelViewBottom() || wd.y > panelViewTop()) continue;
        if (wd.active) glColor3f(0.20f, 0.50f, 0.90f);
        else           glColor3f(0.26f, 0.28f, 0.32f);
        rect(wd.x, wd.y, wd.w, wd.h);
        glColor3f(0.40f, 0.43f, 0.48f);
        rectOutline(wd.x, wd.y, wd.w, wd.h);

        glColor3f(0.92f, 0.93f, 0.95f);
        textCentred(wd.x + wd.w / 2, wd.y + 8, wd.label.c_str());
#if F_COLOUR

        if (wd.id == W_DD_COLOUR) {          /* colour swatch */
            col255(curR, curG, curB);
            rect(wd.x + 6, wd.y + 6, 12, 12);
            glColor3f(0.7f, 0.7f, 0.7f);
            rectOutline(wd.x + 6, wd.y + 6, 12, 12);
        }
#endif
#if F_FILL
        if (wd.id == W_FILL_COLOUR) {
            col255(fillR, fillG, fillB);
            rect(wd.x + 6, wd.y + 6, 12, 12);
            glColor3f(0.7f, 0.7f, 0.7f);
            rectOutline(wd.x + 6, wd.y + 6, 12, 12);
        }
#endif
#if F_COLOUR
        if (wd.id == W_DD_COLOUR) {
            glColor3f(0.85f, 0.87f, 0.90f);
            text(wd.x + wd.w - 16, wd.y + 8, "v", GLUT_BITMAP_HELVETICA_10);
        }
#endif
#if F_STYLE
        if (wd.id == W_DD_STYLE) {
            glColor3f(0.85f, 0.87f, 0.90f);
            text(wd.x + wd.w - 16, wd.y + 8, "v", GLUT_BITMAP_HELVETICA_10);
        }
#endif
    }
    glDisable(GL_SCISSOR_TEST);

    /* scrollbar */
    float tx, ty, tw, th, thumbY, thumbH;
    if (scrollbarGeom(tx, ty, tw, th, thumbY, thumbH)) {
        glColor3f(0.20f, 0.21f, 0.25f);
        rect(tx, ty, tw, th);
        if (scrollDragging) glColor3f(0.45f, 0.65f, 0.95f);
        else                glColor3f(0.38f, 0.42f, 0.50f);
        rect(tx, thumbY, tw, thumbH);
    }

    /* fixed header */
    glColor3f(0.11f, 0.12f, 0.15f);
    rect(px, panelViewTop(), PANEL_W, PANEL_HEADER_H);
    glColor3f(0.55f, 0.75f, 1.0f);
    text(px + 12, (float)(winH - 21), "SHAPE DRAWING TOOL", GLUT_BITMAP_HELVETICA_12);
    if (panelMaxScroll > 0) {
        glColor3f(0.45f, 0.50f, 0.58f);
        const char* sh = (panelScroll < panelMaxScroll) ? "scroll for more" : "";
        text(px + PANEL_W - 14 - textWidth(sh, GLUT_BITMAP_HELVETICA_10), (float)(winH - 21), sh, GLUT_BITMAP_HELVETICA_10);
    }
    glColor3f(0.30f, 0.33f, 0.40f);
    glBegin(GL_LINES);
    glVertex2f(px, panelViewTop()); glVertex2f(px + PANEL_W, panelViewTop());
    glEnd();
#if F_DROPDOWN

    /* dropdown overlays (drawn last so they sit above everything) */
    if (openDropdown != 0) {
        float bx = 0, by = 0, bw = 0;
        for (size_t i = 0; i < widgets.size(); i++)
            if (widgets[i].id == openDropdown) { bx = widgets[i].x; by = widgets[i].y; bw = widgets[i].w; }
#if F_COLOUR && F_STYLE
        int   n = (openDropdown == W_DD_COLOUR) ? NCOLOUR_OPTS + 1 : NSTYLE_OPTS + 1;
#elif F_COLOUR
        int   n = NCOLOUR_OPTS + 1;
#else
        int   n = NSTYLE_OPTS + 1;
#endif
        float ih = 20, listH = n * ih;
        float ly = by - listH;
        if (ly < 4) ly = by + 22;   /* not enough room below: open upwards */

        glColor3f(0.10f, 0.11f, 0.13f);
        rect(bx, ly, bw, listH);
        glColor3f(0.35f, 0.55f, 0.85f);
        rectOutline(bx, ly, bw, listH);

        for (int i = 0; i < n; i++) {
            float iy = ly + listH - (i + 1) * ih;
            bool isCustom = (i == n - 1);
#if F_COLOUR
            if (openDropdown == W_DD_COLOUR) {
                if (!isCustom) {
                    col255(colourOpts[i].r, colourOpts[i].g, colourOpts[i].b);
                    rect(bx + 6, iy + 5, 11, 11);
                    glColor3f(0.92f, 0.93f, 0.95f);
                    text(bx + 24, iy + 6, colourOpts[i].name, GLUT_BITMAP_HELVETICA_10);
                }
                else {
                    glColor3f(0.95f, 0.85f, 0.45f);
                    text(bx + 24, iy + 6, "Custom r,g,b ...", GLUT_BITMAP_HELVETICA_10);
                }
            }
#endif
#if F_STYLE
            if (openDropdown == W_DD_STYLE) {
                if (!isCustom) {
                    char lab[64], b2[40];
                    patternToBinary(styleOpts[i].value, styleOpts[i].bits, b2);
                    sprintf(lab, "%-11s %s", styleOpts[i].name, b2);
                    glColor3f(0.92f, 0.93f, 0.95f);
                    text(bx + 8, iy + 6, lab, GLUT_BITMAP_HELVETICA_10);
                }
                else {
                    glColor3f(0.95f, 0.85f, 0.45f);
                    text(bx + 8, iy + 6, "Custom hex ...", GLUT_BITMAP_HELVETICA_10);
                }
            }
#endif
        }
    }
#endif
}
