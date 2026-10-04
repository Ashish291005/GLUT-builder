# GLUT Code Builder

An offline web page that generates C++ / OpenGL (freeglut) programs for computer-graphics practicals.

**Open it:** `index.html` (or `GLUT-Code-Builder.html`) - a single self-contained file, no internet needed.

## Two modes

- **Full app** - the interactive GraphicsLab drawing tool (sidebar, mouse drawing, zoom/pan, fills, 2D transformations) with only the features you tick. Unticked features are removed from the source entirely.
- **Simple scene program** - a short standalone program that draws a fixed scene, or reads coordinates from the console / mouse, using only the algorithms you pick (DDA, Symmetric DDA, Bresenham, midpoint circle/ellipse, boundary/flood/scan-line fill, transformations).

## Files

| File | Purpose |
|---|---|
| `index.html`, `GLUT-Code-Builder.html` | the built page (identical) |
| `ui.html` | page source |
| `gen.js`, `presets.js` | simple-scene generator and its presets |
| `master/part*.cpp` | the full app, with every feature wrapped in `#if F_...` markers |
| `strip.js` | keeps only the selected features and removes the markers |
| `build.py` | rebuilds the single-file page: `python build.py` |

## Building the generated code

Visual Studio C++ console project with the `nupengl.core` NuGet package, or:

```
g++ prog.cpp -lfreeglut -lopengl32 -lglu32
```
