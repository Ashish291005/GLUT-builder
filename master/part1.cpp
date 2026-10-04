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
