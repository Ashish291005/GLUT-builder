/* MSVC reports sprintf / fopen as unsafe (C4996). This define must appear
   BEFORE any #include to suppress that. */
#define _CRT_SECURE_NO_WARNINGS

/*
 ============================================================================
    SHAPE DRAWING TOOL - interactive, with an on-screen control panel
 ============================================================================
    LAYOUT
        Left  : drawing canvas, with grid and origin (0,0) at canvas centre.
        Right : dark control panel built from rectangles and hit tests.

    SHAPES
#if F_LINE
        Line            : click P1, click P2
#endif
#if F_CIRCLE
        Circle          : midpoint circle algorithm (8-way symmetry)
#endif
#if F_ELLIPSE
        Ellipse         : midpoint ellipse algorithm (4-way symmetry)
#endif
#if F_RECT
        Rectangle       : two opposite corners
#endif
#if F_SQUARE
        Square          : two corners, side = larger of |dx|, |dy|
#endif
#if F_TRIANGLE
        Triangle        : three vertices
#endif
#if F_DIAMOND
        Diamond         : bounding box corners
#endif
#if F_POLYGON
        Regular polygon : centre and first vertex
#endif
#if F_STAR
        Star            : centre and first point
#endif
#if F_FREEPOLY
        Free polygon    : click any number of vertices
#endif
#if F_LINEALGO

    LINE ALGORITHMS (lines and polygon edges)
#if F_DDA
        Simple DDA
#endif
#if F_SDDA
        Symmetric DDA
#endif
#if F_BRES
        Bresenham
#endif
#endif
#if F_FILL

    FILL
#if F_B4
        Boundary fill, 4-connected
#endif
#if F_B8
        Boundary fill, 8-connected
#endif
#if F_FLOOD
        Flood fill
#endif
#if F_SCAN
        Scan-line fill
#endif
#endif
#if F_TRANSFORM

    2D TRANSFORMATIONS (3x3 homogeneous matrices)
#if F_TRANSLATE
        Translation
#endif
#if F_SCALE
        Scaling (about the origin or a pivot)
#endif
#if F_ROTATE
        Rotation (about the origin or a pivot)
#endif
#if F_REFLECT
        Reflection (about an axis or any line)
#endif
#if F_SHEAR
        Shearing
#endif
#if F_COMPOSITE
        Composite (several transforms combined into one matrix)
#endif
#endif

    BUILD
        Visual Studio : Console App (C++), NuGet package nupengl.core
        MinGW         : g++ tool.cpp -o tool.exe -lfreeglut -lopengl32 -lglu32
        Linux         : g++ tool.cpp -o tool -lGL -lGLU -lglut
 ============================================================================
*/

#include <GL/glut.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <utility>
#if F_FILL
#include <unordered_set>
#include <unordered_map>
#endif
#if F_TRANSFORM
#include <map>
#endif

/* ===========================================================================
   1.  WINDOW GEOMETRY AND VIEW TRANSFORM
   =========================================================================== */
int   winW = 1280, winH = 800;
const float PANEL_W = 260.0f;

float canvasW() { return winW - PANEL_W; }
float originX() { return canvasW() / 2.0f; }
float originY() { return winH / 2.0f; }

/* Camera: world -> screen = origin + offset + world * scale */
float viewScale = 1.0f;
float viewOffsetX = 0.0f;
float viewOffsetY = 0.0f;

float sx(float wx) { return originX() + viewOffsetX + wx * viewScale; }
float sy(float wy) { return originY() + viewOffsetY + wy * viewScale; }
float screenToWorldX(float mx) { return (mx - originX() - viewOffsetX) / viewScale; }
float screenToWorldY(float my) { return (my - originY() - viewOffsetY) / viewScale; }

/* ===========================================================================
   2.  DATA MODEL
   =========================================================================== */
enum Algo {
    ALGO_BRESENHAM = 0, ALGO_SYMMETRIC = 1, ALGO_SIMPLE = 2,
    ALGO_MID_CIRCLE = 3, ALGO_MID_ELLIPSE = 4
};
const char* algoName[5] = { "Bresenham", "Symmetric DDA", "Simple DDA",
                             "Midpoint Circle", "Midpoint Ellipse" };

/* Values are stored in lines.dat, so they must not move. */
enum Shape {
    SHAPE_LINE = 0, SHAPE_CIRCLE = 1, SHAPE_ELLIPSE = 2,
    SHAPE_RECTANGLE = 3, SHAPE_TRIANGLE = 4,
    SHAPE_SQUARE = 5, SHAPE_DIAMOND = 6, SHAPE_POLYGON = 7,
    SHAPE_STAR = 8, SHAPE_FREEPOLY = 9
};
const int NSHAPES = 10;
const char* shapeName[NSHAPES] = { "Line", "Circle", "Ellipse", "Rectangle", "Triangle",
                                   "Square", "Diamond", "Polygon", "Star", "Free Poly" };

/* shape selected when the program starts */
#if F_LINE
int currentShape = SHAPE_LINE;
#elif F_CIRCLE
int currentShape = SHAPE_CIRCLE;
#elif F_ELLIPSE
int currentShape = SHAPE_ELLIPSE;
#elif F_RECT
int currentShape = SHAPE_RECTANGLE;
#elif F_SQUARE
int currentShape = SHAPE_SQUARE;
#elif F_TRIANGLE
int currentShape = SHAPE_TRIANGLE;
#elif F_DIAMOND
int currentShape = SHAPE_DIAMOND;
#elif F_POLYGON
int currentShape = SHAPE_POLYGON;
#elif F_STAR
int currentShape = SHAPE_STAR;
#elif F_FREEPOLY
int currentShape = SHAPE_FREEPOLY;
#else
int currentShape = -1;           /* no shapes in this program */
#endif

/* line algorithm used for new lines / polygon edges */
#if F_BRES
const int DEFAULT_LINE_ALGO = ALGO_BRESENHAM;
#elif F_SDDA
const int DEFAULT_LINE_ALGO = ALGO_SYMMETRIC;
#else
const int DEFAULT_LINE_ALGO = ALGO_SIMPLE;
#endif
int lastLineAlgo = DEFAULT_LINE_ALGO;

/* Polygon-family shapes are stored as a closed list of vertices and drawn
   edge by edge with the chosen line algorithm. */
bool isPolyShape(int s) {
    return s == SHAPE_RECTANGLE || s == SHAPE_TRIANGLE || s == SHAPE_SQUARE ||
        s == SHAPE_DIAMOND || s == SHAPE_POLYGON || s == SHAPE_STAR || s == SHAPE_FREEPOLY;
}

/* Shapes this program can draw. */
bool shapeEnabled(int s) {
#if F_LINE
    if (s == SHAPE_LINE) return true;
#endif
#if F_CIRCLE
    if (s == SHAPE_CIRCLE) return true;
#endif
#if F_ELLIPSE
    if (s == SHAPE_ELLIPSE) return true;
#endif
#if F_RECT
    if (s == SHAPE_RECTANGLE) return true;
#endif
#if F_TRIANGLE
    if (s == SHAPE_TRIANGLE) return true;
#endif
#if F_SQUARE
    if (s == SHAPE_SQUARE) return true;
#endif
#if F_DIAMOND
    if (s == SHAPE_DIAMOND) return true;
#endif
#if F_POLYGON
    if (s == SHAPE_POLYGON) return true;
#endif
#if F_STAR
    if (s == SHAPE_STAR) return true;
#endif
#if F_FREEPOLY
    if (s == SHAPE_FREEPOLY) return true;
#endif
    (void)s;
    return false;
}

struct Pt { float x = 0, y = 0; };

struct LineObj {
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;   /* polygons: bounding box (min, max) */
    int   shape = 0;
    int   algo = 0;
    int   r = 0, g = 0, b = 0;
    int   thickness = 1;
    unsigned int pattern = 0xFFFF;
    int   patternBits = 16;
    int   patternScale = 3;
    bool  deleted = false;
    float angle = 0.0f;
    int   group = 0;        /* 0 = ungrouped; shapes sharing an id move together */
#if F_UNDO
    int   seq = 0;          /* creation order, shared with fills (for undo) */
#endif
    std::vector<Pt> pts;    /* polygon vertices */
};

std::vector<LineObj> lines;
#if F_UNDO
/* Undo/redo covers shapes and fills together: undo removes whichever live
   item (shape or fill) was created last. Redo entries are (kind, index). */
enum { ITEM_SHAPE = 0, ITEM_FILL = 1 };
std::vector<std::pair<int, int>> redoStack;
int nextSeq = 1;
#endif
#if F_SELECT

/* ---- Selection (multi-select, groups) ---- */
std::vector<int> selection;
bool isSelected(int i) {
    return std::find(selection.begin(), selection.end(), i) != selection.end();
}
void deselect(int i) {
    selection.erase(std::remove(selection.begin(), selection.end(), i), selection.end());
}
int nextGroupId() {
    int g = 0;
    for (size_t i = 0; i < lines.size(); i++) g = std::max(g, lines[i].group);
    return g + 1;
}
#endif
#if F_POLY
void syncPolyBox(LineObj& L) {
    if (L.pts.empty()) return;
    L.x0 = L.x1 = L.pts[0].x; L.y0 = L.y1 = L.pts[0].y;
    for (size_t k = 1; k < L.pts.size(); k++) {
        L.x0 = std::min(L.x0, L.pts[k].x); L.x1 = std::max(L.x1, L.pts[k].x);
        L.y0 = std::min(L.y0, L.pts[k].y); L.y1 = std::max(L.y1, L.pts[k].y);
    }
}
#endif
#if F_FILL

/* ---- Fill regions (cached pixel sets) ---- */
enum FillAlgo {
    FILL_BOUNDARY_4 = 0, FILL_BOUNDARY_8 = 1,
    FILL_FLOOD = 2, FILL_SCANLINE = 3
};
const char* fillAlgoName[4] = { "Boundary 4", "Boundary 8", "Flood", "Scan-line" };

#if F_SCAN
/* Scan-line fill keeps one entry per scan line for the animation:
   which pixels belong to the row and where the row crosses the edges. */
struct ScanRow { int y = 0; size_t start = 0, end = 0; std::vector<float> xs; };
#endif

struct FillRegion {
    int seedX = 0, seedY = 0;
    int r = 0, g = 0, b = 0;
    int algorithm = 0;
    int connectivity = 0;
    bool deleted = false;
#if F_UNDO
    int seq = 0;
#endif
    long leakAt = -1;       /* index in pixels of the first escaped pixel, -1 = contained */
    std::vector<std::pair<int, int>> pixels;   /* in the order the algorithm coloured them */
#if F_ANIM
    std::vector<int> aux;   /* stack size (boundary fill) or queue size (flood fill) per pixel */
#endif
#if F_SCAN
    std::vector<ScanRow> rows;                 /* scan-line fill only */
#endif
};
std::vector<FillRegion> fillRegions;
#if F_ANIM

/* Fill animation: pixels are revealed in the order the algorithm visited
   them (scan-line fill: one scan line at a time). */
int    fillAnimIndex = -1;
double fillAnimPos = 0;          /* progress: pixels, or scan lines for scan-line fill */
double fillAnimRate = 1;         /* progress per timer tick */
bool   fillAnimPaused = false;   /* Space pauses, Right arrow steps */
int    fillSpeed = 2;            /* 0 very slow, 1 slow, 2 medium, 3 fast */
const char* fillSpeedName[4] = { "Very slow", "Slow", "Medium", "Fast" };
#endif

/* Leak alert: raised when a fill reaches its first escaped pixel. */
bool  leakAlertOn = false;
int   leakAlertX = 0, leakAlertY = 0, leakAlertAlgo = 0;
int   leakAlertGen = 0;
#endif

#if F_FILE
void saveLines();
#if F_FILL
void saveFills();
#endif
#endif
void logMsg(const std::string& s);
#if F_TRANSFORM

/* ===========================================================================
   2b. 2D TRANSFORMS
   =========================================================================== */
enum TransformType {
    TF_NONE = 0, TF_TRANSLATE, TF_SCALE, TF_ROTATE, TF_REFLECT_AXIS, TF_COMPOSITE, TF_SHEAR
};
int  currentTransform = TF_NONE;
bool transformModeActive = false;

struct Mat3 {
    float m[3][3];
    Mat3() { identity(); }
    void identity() {
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++)
                m[i][j] = (i == j) ? 1.0f : 0.0f;
    }
};
Mat3 mat3Mul(const Mat3& A, const Mat3& B) {
    Mat3 R;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            R.m[i][j] = 0;
            for (int k = 0; k < 3; k++)
                R.m[i][j] += A.m[i][k] * B.m[k][j];
        }
    return R;
}
void mat3Apply(const Mat3& M, float& x, float& y) {
    float nx = M.m[0][0] * x + M.m[0][1] * y + M.m[0][2];
    float ny = M.m[1][0] * x + M.m[1][1] * y + M.m[1][2];
    x = nx; y = ny;
}
Mat3 mat3Translation(float tx, float ty) { Mat3 M; M.m[0][2] = tx; M.m[1][2] = ty; return M; }
#if F_SCALE
Mat3 mat3Scaling(float sx, float sy) { Mat3 M; M.m[0][0] = sx; M.m[1][1] = sy; return M; }
Mat3 mat3ScaleAbout(float sxv, float syv, float px, float py) {
    return mat3Mul(mat3Translation(px, py),
        mat3Mul(mat3Scaling(sxv, syv), mat3Translation(-px, -py)));
}
#endif
#if F_ROTATE || F_REFLECT
Mat3 mat3Rotation(float r) {
    Mat3 M; float c = cosf(r), s = sinf(r);
    M.m[0][0] = c; M.m[0][1] = -s; M.m[1][0] = s; M.m[1][1] = c;
    return M;
}
#endif
#if F_ROTATE
Mat3 mat3RotateAbout(float r, float px, float py) {
    return mat3Mul(mat3Translation(px, py),
        mat3Mul(mat3Rotation(r), mat3Translation(-px, -py)));
}
#endif
#if F_REFLECT
Mat3 mat3ReflectAboutLine(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1, dy = y2 - y1;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1e-6f) { Mat3 I; return I; }
    float angle = atan2f(dy, dx);
    Mat3 T1 = mat3Translation(-x1, -y1);
    Mat3 R1 = mat3Rotation(-angle);
    Mat3 Ref; Ref.m[1][1] = -1.0f;
    Mat3 R2 = mat3Rotation(angle);
    Mat3 T2 = mat3Translation(x1, y1);
    return mat3Mul(T2, mat3Mul(R2, mat3Mul(Ref, mat3Mul(R1, T1))));
}
#endif
#if F_SHEAR
Mat3 mat3ShearAbout(float shx, float shy, float px, float py) {
    Mat3 S; S.m[0][1] = shx; S.m[1][0] = shy;
    return mat3Mul(mat3Translation(px, py), mat3Mul(S, mat3Translation(-px, -py)));
}
#endif
#if F_COMPOSITE

struct CompositeEntry { int type = 0; std::string desc; Mat3 matrix; };
std::vector<CompositeEntry> compositeQueue;
#endif

bool tfConfirmActive = false;
std::vector<int> tfOldIndices, tfNewIndices;
std::string tfConfirmDesc;

/* Returns a transformed copy of one shape. */
LineObj transformedShape(const LineObj& src, const Mat3& M) {
    LineObj N = src;
#if F_POLY
    if (isPolyShape(N.shape)) {          /* polygons: transform every vertex exactly */
        for (size_t k = 0; k < N.pts.size(); k++) mat3Apply(M, N.pts[k].x, N.pts[k].y);
        syncPolyBox(N);
        return N;
    }
#endif
#if F_LINE
    if (N.shape == SHAPE_LINE) {
        mat3Apply(M, N.x0, N.y0);
        mat3Apply(M, N.x1, N.y1);
    }
#endif
#if F_CIRCLE || F_ELLIPSE
    if (N.shape == SHAPE_CIRCLE || N.shape == SHAPE_ELLIPSE) {
        /* transform the centre and the two semi-axis end points */
        float cx = N.x0, cy = N.y0, rx, ry, ang;
        if (N.shape == SHAPE_CIRCLE) {
            rx = ry = sqrtf((N.x1 - N.x0) * (N.x1 - N.x0) + (N.y1 - N.y0) * (N.y1 - N.y0));
            ang = 0.0f;
        }
        else {
            rx = fabsf(N.x1 - N.x0); ry = fabsf(N.y1 - N.y0); ang = N.angle;
        }
        float cosA = cosf(ang), sinA = sinf(ang);
        float ax1 = cx + rx * cosA, ay1 = cy + rx * sinA;
        float ax2 = cx - ry * sinA, ay2 = cy + ry * cosA;
        mat3Apply(M, cx, cy);
        mat3Apply(M, ax1, ay1);
        mat3Apply(M, ax2, ay2);
        float newRx = sqrtf((ax1 - cx) * (ax1 - cx) + (ay1 - cy) * (ay1 - cy));
        float newRy = sqrtf((ax2 - cx) * (ax2 - cx) + (ay2 - cy) * (ay2 - cy));
        float newAngle = atan2f(ay1 - cy, ax1 - cx);
#if F_ELLIPSE
        if (N.shape == SHAPE_CIRCLE && fabsf(newRx - newRy) < 0.5f) {
            N.x0 = cx; N.y0 = cy; N.x1 = ax1; N.y1 = ay1;
        }
        else {                           /* a stretched circle becomes an ellipse */
            N.shape = SHAPE_ELLIPSE; N.algo = ALGO_MID_ELLIPSE;
            N.x0 = cx; N.y0 = cy;
            N.x1 = cx + newRx; N.y1 = cy + newRy;
            N.angle = newAngle;
        }
#else
        float r = (newRx + newRy) / 2.0f;   /* circles stay circles */
        N.x0 = cx; N.y0 = cy;
        N.x1 = cx + r * cosf(newAngle); N.y1 = cy + r * sinf(newAngle);
#endif
    }
#endif
    N.x0 = floorf(N.x0 + 0.5f); N.y0 = floorf(N.y0 + 0.5f);
    N.x1 = floorf(N.x1 + 0.5f); N.y1 = floorf(N.y1 + 0.5f);
    return N;
}

/* Applies M to every selected shape. Copies are added next to the originals
   and the user confirms (Y/N/Esc) whether to drop the originals. Groups stay
   grouped: each old group id maps to one fresh id. */
void applyTransformToSelection(const Mat3& M, const std::string& desc) {
    if (selection.empty()) { logMsg("Select a shape first"); return; }
    std::map<int, int> groupMap;
    int groupBase = nextGroupId();
    tfOldIndices.clear(); tfNewIndices.clear();
    std::vector<int> src = selection;
    for (size_t s = 0; s < src.size(); s++) {
        int idx = src[s];
        if (idx < 0 || idx >= (int)lines.size() || lines[idx].deleted) continue;
        LineObj N = transformedShape(lines[idx], M);
        if (N.group != 0) {
            if (!groupMap.count(N.group)) {
                int fresh = groupBase + (int)groupMap.size();
                groupMap[N.group] = fresh;
            }
            N.group = groupMap[N.group];
        }
#if F_UNDO
        N.seq = nextSeq++;
#endif
        lines.push_back(N);
        tfOldIndices.push_back(idx);
        tfNewIndices.push_back((int)lines.size() - 1);
    }
    if (tfNewIndices.empty()) return;
    tfConfirmActive = true;
    tfConfirmDesc = desc;
    selection = tfNewIndices;
#if F_FILE
    saveLines();
#endif
    logMsg(desc);
    glutPostRedisplay();
}
#endif

/* ===========================================================================
   3.  CURRENT SETTINGS
   =========================================================================== */
int currentAlgo = DEFAULT_LINE_ALGO;   /* set from the shape in main() */

int curR = 0, curG = 0, curB = 0;      /* line colour */
#if F_COLOUR
struct ColourOpt { int r = 0, g = 0, b = 0; const char* name = ""; };
ColourOpt colourOpts[] = {
    {   0,   0,   0, "Black"   }, { 220,   0,   0, "Red"     },
    {   0, 150,   0, "Green"   }, {   0,  70, 230, "Blue"    },
    { 200,   0, 200, "Magenta" }, { 255, 140,   0, "Orange"  },
    {   0, 160, 160, "Teal"    }, { 120,  60,   0, "Brown"   }
};
const int NCOLOUR_OPTS = 8;
int colourSel = 0;
bool colourIsCustom = false;
#endif

/* line pattern: 1 bits are drawn, 0 bits skipped; each bit covers
   curPatternScale pixels */
unsigned int curPattern = 0xFFFF;
int curPatternBits = 16;
int curPatternScale = 3;
#if F_STYLE
struct StyleOpt { unsigned int value = 0; int bits = 0; const char* name = ""; };
StyleOpt styleOpts[] = {
    { 0xFFFF, 16, "Solid"      }, { 0xA,     4, "Dotted"     },
    { 0xB,     4, "Dash-dot"   }, { 0xC,     4, "Even dash"  },
    { 0xF0,    8, "Long dash"  }, { 0xE4,    8, "Dash-dot-2" },
    { 0xCCCC, 16, "Wide dash"  }
};
const int NSTYLE_OPTS = 7;
int styleSel = 0;
bool styleIsCustom = false;
#endif

int curThickness = 1;
#if F_THICK
int thicknessOpts[] = { 1, 3, 5, 7 };
const int NTHICK = 4;
#endif

#if F_MARKERS
bool endpointsOn = true;
int  epR = 200, epG = 0, epB = 0;
#endif

enum InputMode { MODE_MOUSE = 0, MODE_TYPE = 1, MODE_SELECT = 2, MODE_FILL = 3 };
int inputMode = MODE_MOUSE;

bool showGrid = true;
bool showLabels = true;
bool darkMode = false;

int   clickState = 0;
float pendingX = 0, pendingY = 0;
float mouseWorldX = 0, mouseWorldY = 0;
#if F_ELLIPSE

int   ellipseRx = 0;
int   ellipseRy = 50;
float ellipseP2X = 0, ellipseP2Y = 0;
#endif
#if F_SNAP

/* Drawing helpers */
bool snapToGrid = false;
int  snapStep = 10;
#endif
#if F_DRAG
bool dragDraw = false;
bool dragging = false;
float liveEndX = 0, liveEndY = 0;
#endif
#if F_FILL

/* Fill state */
bool  fillMode = false;
int   fillR = 255, fillG = 200, fillB = 0;
int   fillConnectivity = 4;
#if F_B4
int   fillAlgorithm = FILL_BOUNDARY_4;
#elif F_B8
int   fillAlgorithm = FILL_BOUNDARY_8;
#elif F_FLOOD
int   fillAlgorithm = FILL_FLOOD;
#else
int   fillAlgorithm = FILL_SCANLINE;
#endif
#endif

/* Polygon drawing state */
#if F_POLYGON
int polySides = 6;               /* regular polygon */
#endif
#if F_STAR
int starPoints = 5;              /* star */
const float STAR_INNER = 0.45f;  /* inner radius / outer radius */
#endif
#if F_VERTS
std::vector<Pt> polyVerts;       /* vertices placed so far (free poly, 3-click triangle) */
#endif
#if F_ZOOM

/* Pan state */
bool  panning = false;
bool  panMoved = false;
int   panButton = -1;
float panStartX = 0, panStartY = 0;
float panStartOffX = 0, panStartOffY = 0;
bool  spaceDown = false;
#endif
#if F_SELECT

/* Select / move state */
bool  moveDragging = false, moveDidMove = false;
float moveLastX = 0, moveLastY = 0;
bool  boxSelecting = false, boxAdditive = false;
float boxX0 = 0, boxY0 = 0, boxX1 = 0, boxY1 = 0;   /* screen coords */
#endif

/* Side panel scrolling */
const float PANEL_HEADER_H = 34.0f;
float panelScroll = 0.0f;
float panelMaxScroll = 0.0f;
float panelContentH = 0.0f;
bool  scrollDragging = false;
float scrollDragStartY = 0, scrollDragStartScroll = 0;
#if F_INPUTBOX

/* Text input overlay */
enum InputTarget {
    IN_NONE = 0,
#if F_TYPE
    IN_COORDS,
#endif
#if F_COLOUR
    IN_LINE_RGB,
#endif
#if F_MARKERS
    IN_EP_RGB,
#endif
#if F_STYLE
    IN_HEX,
#endif
#if F_FILL
    IN_FILL_RGB,
#endif
#if F_TRANSLATE
    IN_TF_TRANSLATE,
#endif
#if F_SCALE
    IN_TF_SCALE,
#endif
#if F_ROTATE
    IN_TF_ROTATE,
#endif
#if F_REFLECT
    IN_TF_REFLECT,
#endif
#if F_SHEAR
    IN_TF_SHEAR,
#endif
#if F_PIVOT
    IN_TF_PIVOT,
#endif
    IN_LAST
};
#if F_PIVOT
/* Pivot for scale / rotate / shear. Origin (0,0) unless set with the
   "Set Pivot" button in the entry box; reset for every new transform. */
float tfPivotX = 0.0f, tfPivotY = 0.0f;
bool  tfPivotSet = false;
int         pivotReturnTarget = 0;
std::string pivotReturnBuf, pivotReturnPrompt, pivotReturnHint;
bool takesPivot(int target) {
    switch (target) {
#if F_SCALE
    case IN_TF_SCALE:
#endif
#if F_ROTATE
    case IN_TF_ROTATE:
#endif
#if F_SHEAR
    case IN_TF_SHEAR:
#endif
        return true;
    }
    return false;
}
#endif
int         inputTarget = IN_NONE;
std::string inputBuf;
std::string inputPrompt;
std::string inputHint;

void beginInput(int target, const char* prompt, const char* hint) {
    inputTarget = target; inputBuf.clear();
    inputPrompt = prompt; inputHint = hint;
}
#endif

std::vector<std::string> logLines;
void logMsg(const std::string& s) {
    logLines.push_back(s);
    if (logLines.size() > 5) logLines.erase(logLines.begin());
    std::cout << s << "\n";
}

/* ===========================================================================
   4.  TEXT AND COLOUR HELPERS
   =========================================================================== */
void text(float x, float y, const char* s, void* font = GLUT_BITMAP_HELVETICA_12) {
    glRasterPos2i((int)x, (int)y);
    for (const char* c = s; *c; c++) glutBitmapCharacter(font, *c);
}
int textWidth(const char* s, void* font = GLUT_BITMAP_HELVETICA_12) {
    int w = 0;
    for (const char* c = s; *c; c++) w += glutBitmapWidth(font, *c);
    return w;
}
void textCentred(float cx, float y, const char* s, void* font = GLUT_BITMAP_HELVETICA_12) {
    text(cx - textWidth(s, font) / 2.0f, y, s, font);
}
void col255(int r, int g, int b) { glColor3f(r / 255.0f, g / 255.0f, b / 255.0f); }

void rect(float x, float y, float w, float h) {
    glBegin(GL_QUADS);
    glVertex2f(x, y); glVertex2f(x + w, y);
    glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();
}
void rectOutline(float x, float y, float w, float h) {
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y); glVertex2f(x + w, y);
    glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();
}
#if F_STYLE
void patternToBinary(unsigned int v, int bits, char* out) {
    int k = 0;
    for (int i = bits - 1; i >= 0; i--) out[k++] = ((v >> i) & 1) ? '1' : '0';
    out[k] = '\0';
}
#endif


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
#if F_POLY
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
            spanFill(seedX, seedY, boundary, bb, order);             /* region made of separate lines */
            groupIntoRows(order, fr.rows);
            for (size_t k = 0; k < order.size(); k++)
                if (escapedPixel(order[k].first, order[k].second)) { leakAt = (long)k; break; }
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
