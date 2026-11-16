// ui_font.h - the app's fonts (Inter) and the text functions every screen uses.
//   Inter SemiBold : title, headings, big buttons (any text of size 18 or more)
//   Inter Medium   : area names and car name tags (drawLabel, with a white outline so they stay readable on a map image)
//   Inter Regular  : panel text, log lines, small text

#pragma once
#include "raylib.h"
#include <iostream>
using namespace std;

// Grow the window to use most of the screen (never smaller than the size it already has). Call right after InitWindow.
// w and h are updated, so every layout calculation that uses them follows the new size
static inline void fitWindowToScreen(int &w, int &h){
    int m = GetCurrentMonitor(), mw = GetMonitorWidth(m), mh = GetMonitorHeight(m);
    if (mw <= 0 || mh <= 0)
        return;
    int nw = mw - 80, nh = mh - 140; // leave room for the title bar and the taskbar
    if (nw > 2000)
        nw = 2000;
    if (nh > 1100)
        nh = 1100;
    if (nw > w)
        w = nw;
    if (nh > h)
        h = nh;
    SetWindowSize(w, h);
    int top = (mh - h) / 2 - 10;
    SetWindowPosition((mw - w) / 2, top > 0 ? top : 0);
}

struct UiFonts{
    Font regular, medium, semibold;
};
static UiFonts uiFonts; // filled in by loadUiFonts()

static inline Font loadOneFont(const char *file, int baseSize){
    if (!FileExists(file))
        return GetFontDefault();
    Font f = LoadFontEx(file, baseSize, 0, 0); // 0, 0 = the standard ASCII characters
    GenTextureMipmaps(&f.texture);             // mipmaps + trilinear keep small text smooth
    SetTextureFilter(f.texture, TEXTURE_FILTER_TRILINEAR);
    return f;
}
static inline void loadUiFonts(){ // call once, after InitWindow
    uiFonts.regular = loadOneFont("fonts/Inter-Regular.ttf", 40);
    uiFonts.medium = loadOneFont("fonts/Inter-Medium.ttf", 56); // bigger atlas: area names scale up when you zoom the map
    uiFonts.semibold = loadOneFont("fonts/Inter-SemiBold.ttf", 72);
}
static inline void unloadUiFonts(){
    if (uiFonts.regular.texture.id != GetFontDefault().texture.id)
        UnloadFont(uiFonts.regular);
    if (uiFonts.medium.texture.id != GetFontDefault().texture.id)
        UnloadFont(uiFonts.medium);
    if (uiFonts.semibold.texture.id != GetFontDefault().texture.id)
        UnloadFont(uiFonts.semibold);
}

static inline const Font &fontForSize(int size){ 
    return size >= 18 ? uiFonts.semibold : uiFonts.regular; 
}

static inline Vector2 uiPos(int x, int y){
    Vector2 v;
    v.x = (float)x;
    v.y = (float)y;
    return v;
}

// Drop-in replacements for raylib's DrawText / MeasureText
static inline void drawText(const char *t, int x, int y, int size, Color c){ 
    DrawTextEx(fontForSize(size), t, uiPos(x, y), (float)size, 0, c); 
}
static inline int measureText(const char *t, int size){ 
    return (int)MeasureTextEx(fontForSize(size), t, (float)size, 0).x; 
}

// Medium weight, no outline: log headlines and other short emphasised text
static inline void drawMedium(const char *t, int x, int y, int size, Color c) { 
    DrawTextEx(uiFonts.medium, t, uiPos(x, y), (float)size, 0, c); 
}
static inline int measureMedium(const char *t, int size){ 
    return (int)MeasureTextEx(uiFonts.medium, t, (float)size, 0).x; 
}

// Area names / name tags: Medium weight with a thin white outline, readable on any background
static inline void drawLabel(const char *t, int x, int y, int size, Color c){
    Color halo = Fade(WHITE, 0.9f);
    for (int dx = -1; dx <= 1; dx += 2)
        for (int dy = -1; dy <= 1; dy += 2)
            DrawTextEx(uiFonts.medium, t, uiPos(x + dx, y + dy), (float)size, 0, halo);
    DrawTextEx(uiFonts.medium, t, uiPos(x, y), (float)size, 0, c);
}
