#include "../Studio/src/ScreenEditorCanvasCamera.h"
#include <cmath>
#include <iostream>
#include <limits>
using renegade::studio::ScreenEditorCanvasCamera;
int main()
{
    auto require = [](bool value, const char* message) {
        if (!value) { std::cerr << message << '\n'; return false; }
        return true;
    };
    auto near = [](float a, float b) { return std::abs(a-b) < 0.001f; };
    ScreenEditorCanvasCamera c;
    for (const float aspect : {16.0f/9.0f, 16.0f/10.0f, 21.0f/9.0f})
    {
        c.Reset();
        const float x = 700.0f, y = x / aspect;
        c.ZoomAt(2.0f, x, y);
        if (!require(near(c.ViewX(x), x) && near(c.ViewY(y), y),
                "Zoom moved its cursor pivot")) return 1;
        c.PanBy(12.0f, -7.0f);
        if (!require(near(c.DesignX(c.ViewX(23.0f)), 23.0f) &&
                near(c.DesignY(c.ViewY(44.0f)), 44.0f),
                "Pointer and rendered geometry do not round-trip")) return 1;
    }
    c.ZoomAt(100.0f, 100.0f, 75.0f);
    if (!require(c.zoom == ScreenEditorCanvasCamera::MaxZoom, "Missing max clamp")) return 1;
    c.ZoomAt(0.00001f, 100.0f, 75.0f);
    if (!require(c.zoom == ScreenEditorCanvasCamera::MinZoom, "Missing min clamp")) return 1;
    const float oldX=c.panX, oldY=c.panY;
    c.ZoomAt(std::numeric_limits<float>::quiet_NaN(), 0, 0);
    c.PanBy(std::numeric_limits<float>::infinity(), 0);
    if (!require(c.zoom == ScreenEditorCanvasCamera::MinZoom &&
            c.panX == oldX && c.panY == oldY, "Invalid input changed the camera")) return 1;
    c.Reset();
    if (!require(c.zoom == 1 && c.panX == 0 && c.panY == 0, "Fit did not reset")) return 1;
    std::cout << "PASS: cursor zoom, projection inverse, pan, bounds and Fit\n";
    return 0;
}
