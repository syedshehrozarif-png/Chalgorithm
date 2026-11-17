// gui_draw.h - small drawing helpers shared by gui_main.cpp and gui_tools.h
#pragma once
#include "raylib.h"
#include "ui_font.h"
#include <cmath>
#include <string>
#include "Graph.h"
#include "DynamicArray.h"
using namespace std;

const int PANEL_W = 320;                                     // the side panel keeps this width
static int WIN_W = 1280, WIN_H = 760, MAP_W = 960;           // map area on the left, panel on the right. main() enlarges them to fit the screen

static inline Vector2 V(float x, float y) { Vector2 v; v.x = x; v.y = y; return v; }
static inline Rectangle R(float x, float y, float w, float h) { Rectangle r; r.x = x; r.y = y; r.width = w; r.height = h; return r; }
static inline Color C(int r, int g, int b, int a = 255) { Color c; c.r = (unsigned char)r; c.g = (unsigned char)g; c.b = (unsigned char)b; c.a = (unsigned char)a; return c; }
static inline Vector2 rotOff(Vector2 c, float deg, float lx, float ly) {      // local car coordinates -> world
    float a = deg * DEG2RAD;
    return V(c.x + lx * cosf(a) - ly * sinf(a), c.y + lx * sinf(a) + ly * cosf(a));
}

// ---------- customer blip, car, destination pin ----------
static inline void drawPerson(Vector2 p, Color c, float time) {
    float ph = fmodf(time, 1.4f) / 1.4f;                                   // pulsing "you are here" ring
    DrawRing(p, 10 + 22 * ph, 12 + 22 * ph, 0, 360, 40, Fade(c, 0.55f * (1 - ph)));
    DrawEllipse((int)p.x, (int)p.y, 9, 3, Fade(BLACK, 0.25f));
    DrawLineEx(V(p.x - 2.5f, p.y - 9), V(p.x - 3.5f, p.y), 3.5f, C(50, 55, 70));      // legs
    DrawLineEx(V(p.x + 2.5f, p.y - 9), V(p.x + 3.5f, p.y), 3.5f, C(50, 55, 70));
    DrawRectangleRounded(R(p.x - 6, p.y - 24, 12, 16), 0.5f, 6, c);                    // body
    DrawLineEx(V(p.x - 6, p.y - 22), V(p.x - 8, p.y - 13), 3, c);                      // arms
    DrawLineEx(V(p.x + 6, p.y - 22), V(p.x + 8, p.y - 13), 3, c);
    DrawCircleV(V(p.x, p.y - 30), 5.5f, C(250, 215, 180));                             // head
}

static inline void drawPin(Vector2 p, float s) {                           // red map pin, tip at p
    DrawEllipse((int)p.x, (int)p.y, 6 * s, 2.5f * s, Fade(BLACK, 0.3f));
    Vector2 l = V(p.x - 9 * s, p.y - 20 * s), r = V(p.x + 9 * s, p.y - 20 * s);
    DrawTriangle(p, r, l, C(214, 40, 40));
    DrawTriangle(p, l, r, C(214, 40, 40));
    DrawCircleV(V(p.x, p.y - 26 * s), 12 * s, C(214, 40, 40));
    DrawCircleV(V(p.x, p.y - 26 * s), 5 * s, WHITE);
}

static inline void drawCar(Vector2 p, float deg, Color body, float s) {    // top-down car, nose points along 'deg'
    Color dark = C(35, 45, 65);
    DrawRectanglePro(R(p.x, p.y, 36 * s, 21 * s), V(18 * s, 10.5f * s), deg, WHITE);              // white outline
    DrawRectanglePro(R(p.x, p.y, 32 * s, 17 * s), V(16 * s, 8.5f * s), deg, body);
    Vector2 w = rotOff(p, deg, 5 * s, 0), rw = rotOff(p, deg, -9 * s, 0), roof = rotOff(p, deg, -2 * s, 0);
    DrawRectanglePro(R(w.x, w.y, 7 * s, 12 * s), V(3.5f * s, 6 * s), deg, dark);                   // windscreen
    DrawRectanglePro(R(rw.x, rw.y, 5 * s, 12 * s), V(2.5f * s, 6 * s), deg, dark);                 // rear window
    DrawRectanglePro(R(roof.x, roof.y, 9 * s, 12 * s), V(4.5f * s, 6 * s), deg, Fade(BLACK, 0.18f)); // roof
    for (int side = -1; side <= 1; side += 2) {
        DrawCircleV(rotOff(p, deg, 15 * s, side * 5.5f * s), 2 * s, C(255, 235, 120));              // headlights
        DrawCircleV(rotOff(p, deg, -15 * s, side * 5.5f * s), 1.6f * s, C(120, 10, 10));            // tail lights
    }
}

static inline void drawPath(const Graph& g, const DynamicArray<int>& p, float width, Color col) {
    for (int i = 0; i + 1 < p.size(); i++) DrawLineEx(V(g.x(p[i]), g.y(p[i])), V(g.x(p[i + 1]), g.y(p[i + 1])), width, col);
}

static inline void drawBadge(Vector2 c, const char* label, Color col) {     // round numbered/lettered marker
    DrawCircleV(c, 14, WHITE);
    DrawCircleV(c, 11.5f, col);
    drawText(label, (int)c.x - measureText(label, 15) / 2, (int)c.y - 7, 15, WHITE);
}

// ---------- text helpers ----------
static inline DynamicArray<string> wrap(const string& s, int size, int maxW) {
    DynamicArray<string> out;
    string cur, word;
    for (size_t i = 0; i <= s.size(); i++) {
        char ch = i < s.size() ? s[i] : ' ';
        if (ch != ' ') { word += ch; continue; }
        string t = cur.empty() ? word : cur + " " + word;
        if (!cur.empty() && measureText(t.c_str(), size) > maxW) { out.push_back(cur); cur = word; } else cur = t;
        word = "";
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}
static inline int text(const string& s, int x, int y, int size, Color c, int maxW = 296) {
    DynamicArray<string> ls = wrap(s, size, maxW);
    for (int i = 0; i < ls.size(); i++) { drawText(ls[i].c_str(), x, y, size, c); y += size + 4; }
    return y;
}

// ---------- side panel: text sizes and layout ----------
// Panel text was small, so every size used in the side panel goes through pf(): +2 points up to 14, +1 above.
static inline int pf(int s) { return s <= 14 ? s + 2 : s + 1; }

// Shorten a line with "..." so that it fits in maxW pixels.
static inline string fitText(const string& s, int size, int maxW) {
    if (measureText(s.c_str(), size) <= maxW) return s;
    string t = s;
    while (!t.empty() && measureText((t + "...").c_str(), size) > maxW) t.pop_back();
    return t + "...";
}
// Wrapped text, but at most two lines (the second one is shortened with "..."), so a row never grows past its box.
static inline int textMax2(const string& s, int x, int y, int size, Color c, int maxW = 296) {
    DynamicArray<string> ls = wrap(s, size, maxW);
    for (int i = 0; i < ls.size() && i < 2; i++) {
        string line = ls[i];
        if (i == 1) { for (int k = 2; k < ls.size(); k++) line += " " + ls[k]; line = fitText(line, size, maxW); }
        drawText(line.c_str(), x, y, size, c);
        y += size + 4;
    }
    return y;
}

// All fixed y positions of the panel live here, so drawing and mouse hit-tests always agree.
const int PANEL_TOP = 108;          // first content line (the header above holds the status line and up to 2 road-status lines)
const int RIDER_TOP = 152;          // riders have two driver-stat lines above their content
const int OFFER_ROW_H = 44;         // customer's offer row: 40 px box + 4 px gap
const int WAIT_ROW_H = 64;          // rider's waiting-customer row: 60 px box + 4 px gap
const int TOOLS_ROW_TOP = 140;      // tools menu rows (36 px box, 40 px pitch)
const int TOOLS_ROW_H = 40;
const int LOG_TOP_USER = 490;       // where the event log starts for customers and riders
const int LOG_TOP_ADMIN = 458;      // ... and for the dispatcher (his driver list is above it)

static inline int offerRowsTop() { return PANEL_TOP + (int)wrap("Offers arrive live - click one (or press 1-9) before it expires:", pf(14), 296).size() * (pf(14) + 4) + 2; }
// Rows stop early enough to leave room for the lines under them ("+N more", hints, an error message of up to 2 lines).
const int ROWS_BOTTOM_RESERVE = 80;
static inline int offerRowsMax() { int n = (LOG_TOP_USER - 32 - ROWS_BOTTOM_RESERVE - offerRowsTop()) / OFFER_ROW_H; return n > 9 ? 9 : n; }
static inline int waitRowsTop() { return RIDER_TOP + (int)wrap("Customers waiting - click one (or its blip):", pf(14), 296).size() * (pf(14) + 4) + 2; }
static inline int waitRowsMax() { return (LOG_TOP_USER - 32 - 62 - waitRowsTop()) / WAIT_ROW_H; }
