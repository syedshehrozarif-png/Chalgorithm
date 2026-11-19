// gui_tools.h - the TOOLS window: compare routing algorithms, multi-stop planner, hash table showdown, sorting benchmark.
// Press T in the map window. The simulation is paused while tools are open.
#pragma once
#include <chrono>
#include "gui_draw.h"
#include "Pathfinding.h"
#include "HashTable.h"
#include "Sorting.h"
using namespace std;

enum Tool{
    TOOL_MENU,
    TOOL_COMPARE,
    TOOL_MULTI,
    TOOL_HASH,
    TOOL_SORT
};

static inline bool guiIntLess(const int &a, const int &b) { 
    return a < b; 
}
static inline float fnBfs(const Graph &g, int a, int b, DynamicArray<int> &p, int *e) { 
    return (float)bfs(g, a, b, p, e); 
}
static inline float fnDijkstra(const Graph &g, int a, int b, DynamicArray<int> &p, int *e) { 
    return dijkstra(g, a, b, p, e); 
}
static inline float fnAstar(const Graph &g, int a, int b, DynamicArray<int> &p, int *e) { 
    return astar(g, a, b, p, e); 
}
typedef float (*AlgoFn)(const Graph &, int, int, DynamicArray<int> &, int *);

static inline double nowUs(){
    return (double)chrono::duration_cast<chrono::microseconds>(chrono::steady_clock::now().time_since_epoch()).count();
}
static inline int hashSize(int i){
    static const int v[5] = {100, 250, 500, 1000, 2000};
    return v[i];
}
static inline int sortSize(int i){
    static const int v[5] = {500, 1000, 2000, 4000, 8000};
    return v[i];
}

struct AlgoResult{
    DynamicArray<int> path;
    float km;
    int hops, explored;
    double us;
    AlgoResult() : km(0), hops(-1), explored(0), us(0) {}
};

struct Tools{
    bool open;
    Tool tool;
    string msg;
    // compare
    int a, b;
    bool compared;
    AlgoResult res[3];
    // multi-stop
    int start, nStops, stops[MAXSTOPS], visit[MAXSTOPS];
    bool planned;
    StopPlan plan;
    float naive;
    long fullTree;
    DynamicArray<int> route;
    // hash + sort
    int hashIdx, sortIdx;
    bool hashRan, sortRan;
    long hCol[3];
    int hReh[3], hCap[3];
    double sortMs[5];

    Tools() : open(false), tool(TOOL_MENU), a(-1), b(-1), compared(false), start(-1), nStops(0), planned(false),
              naive(0), fullTree(0), hashIdx(2), sortIdx(2), hashRan(false), sortRan(false) {
        plan.best = 0;
        plan.visited = 0;
        for (int i = 0; i < 3; i++)
        {
            hCol[i] = 0;
            hReh[i] = 0;
            hCap[i] = 0;
        }
        for (int i = 0; i < 5; i++)
            sortMs[i] = 0;
    }

    void show(Tool t){
        tool = t;
        msg = "";
        a = b = start = -1;
        compared = false;
        nStops = 0;
        planned = false;
        if (t == TOOL_HASH)
            runHash();
        if (t == TOOL_SORT)
            runSort();
    }

    void pickNode(const Graph &g, int n){ // player clicked area n on the map
        if (tool == TOOL_COMPARE)
        {
            if (a < 0 || b >= 0)
            {
                a = n;
                b = -1;
                compared = false;
            } else if (n != a) {
                b = n;
                runCompare(g);
            }
        } else if (tool == TOOL_MULTI) {
            if (planned){
                start = -1;
                nStops = 0;
                planned = false;
            }
            if (start < 0)
                start = n;
            else if (nStops < MAXSTOPS)
                stops[nStops++] = n;
            else
                msg = "Six stops is the maximum.";
        }
    }

    //  1. BFS vs Dijkstra vs A*: same trip, three algorithms 
    void runCompare(const Graph &g){
        AlgoFn fns[3] = {fnBfs, fnDijkstra, fnAstar};
        for (int k = 0; k < 3; k++){
            AlgoResult &r = res[k];
            int ex = 0;
            fns[k](g, a, b, r.path, &ex);
            r.explored = ex;
            r.km = 0;
            r.hops = r.path.size() > 0 ? r.path.size() - 1 : -1;
            for (int i = 0; i + 1 < r.path.size(); i++)
                r.km += g.edgeWeight(r.path[i], r.path[i + 1]);
            double t0 = nowUs();
            for (int i = 0; i < 2000; i++){
                DynamicArray<int> q;
                fns[k](g, a, b, q, 0);
            }
            r.us = (nowUs() - t0) / 2000.0;
        }
        compared = true;
    }

    //  2. multi-stop planner (backtracking with pruning) 
    void runMulti(const Graph &g){
        if (start < 0 || nStops < 1){
            msg = "Click a start area and at least one stop first.";
            return;
        }
        if (!planStops(g, start, stops, nStops, plan, naive)){
            msg = "A stop is unreachable (a road may be closed).";
            return;
        }
        fullTree = 1;
        long level = 1;
        for (int d = 1; d <= nStops; d++){
            level *= (nStops - d + 1);
            fullTree += level;
        }
        route.clear();
        int cur = start;
        for (int i = 0; i < nStops; i++){
            int nxt = stops[plan.order[i] - 1];
            DynamicArray<int> seg;
            dijkstra(g, cur, nxt, seg);
            for (int k = (route.size() == 0 ? 0 : 1); k < seg.size(); k++)
                route.push_back(seg[k]);
            visit[i] = nxt;
            cur = nxt;
        }
        planned = true;
        msg = "";
    }

    //  3. hash table showdown 
    void runHash(){
        HashMode modes[3] = {CHAINING, LINEAR_PROBING, DOUBLE_HASHING};
        for (int m = 0; m < 3; m++)
        {
            HashTable<int> t(modes[m]);
            unsigned long seed = 12345; // same pseudo-random ids for every table
            for (int i = 0; i < hashSize(hashIdx); i++)
            {
                seed = (seed * 1103515245UL + 12345UL) & 0x7fffffffUL;
                t.insert((int)(seed % 1000000), i);
            }
            hCol[m] = t.getCollisions();
            hReh[m] = t.getRehashes();
            hCap[m] = t.capacity();
        }
        hashRan = true;
    }

    //  4. sorting benchmark 
    void runSort(){
        int n = sortSize(sortIdx);
        DynamicArray<int> base;
        unsigned long seed = 777;
        for (int i = 0; i < n; i++){
            seed = (seed * 1103515245UL + 12345UL) & 0x7fffffffUL;
            base.push_back((int)(seed % 1000000));
        }
        for (int s = 0; s < 5; s++){
            DynamicArray<int> arr = base;
            double t0 = nowUs();
            if (s == 0)
                bubbleSort(arr, guiIntLess);
            else if (s == 1)
                insertionSort(arr, guiIntLess);
            else if (s == 2)
                mergeSort(arr, guiIntLess);
            else if (s == 3)
                quickSort(arr, guiIntLess);
            else
                radixSort(arr);
            sortMs[s] = (nowUs() - t0) / 1000.0;
        }
        sortRan = true;
    }

    //  keyboard / mouse (called every frame while the tools are open) 
    void update(const Graph &g, Vector2 mouse, bool clicked){
        if (IsKeyPressed(KEY_ESCAPE)){
            if (tool == TOOL_MENU)
                open = false;
            else
                tool = TOOL_MENU;
            return;
        }
        if (tool == TOOL_MENU){
            for (int i = 0; i < 4; i++)
                if (IsKeyPressed(KEY_ONE + i) || (clicked && CheckCollisionPointRec(mouse, R(MAP_W + 12, TOOLS_ROW_TOP + i * TOOLS_ROW_H, 296, 36))))
                    show((Tool)(i + 1));
            return;
        }
        if (tool == TOOL_MULTI){
            if (IsKeyPressed(KEY_ENTER))
                runMulti(g);
            if (IsKeyPressed(KEY_BACKSPACE)){
                start = -1;
                nStops = 0;
                planned = false;
                msg = "";
            }
        }
        int dir = (IsKeyPressed(KEY_UP) || IsKeyPressedRepeat(KEY_UP)) ? 1 : ((IsKeyPressed(KEY_DOWN) || IsKeyPressedRepeat(KEY_DOWN)) ? -1 : 0);
        if (tool == TOOL_HASH && dir){
            hashIdx += dir;
            if (hashIdx < 0)
                hashIdx = 0;
            if (hashIdx > 4)
                hashIdx = 4;
            runHash();
        }
        if (tool == TOOL_SORT && dir){
            sortIdx += dir;
            if (sortIdx < 0)
                sortIdx = 0;
            if (sortIdx > 4)
                sortIdx = 4;
            runSort();
        }
    }

    // drawing on the map (inside BeginMode2D) 
    void drawMap(const Graph &g){
        if (tool == TOOL_COMPARE){
            Color cols[3] = {C(255, 150, 40), C(40, 105, 240), C(30, 170, 90)};
            float widths[3] = {20, 13, 6}; // BFS widest at the bottom, A* thinnest on top
            if (compared)
                for (int k = 0; k < 3; k++)
                    drawPath(g, res[k].path, widths[k], cols[k]);
            if (a >= 0)
                drawBadge(V(g.x(a), g.y(a) - 28), "A", C(24, 160, 88));
            if (b >= 0)
                drawBadge(V(g.x(b), g.y(b) - 28), "B", C(214, 40, 40));
        } else if (tool == TOOL_MULTI){
            if (planned){
                drawPath(g, route, 9, C(140, 70, 210));
                for (int i = 0; i < nStops; i++)
                    drawBadge(V(g.x(visit[i]), g.y(visit[i]) - 28), TextFormat("%d", i + 1), C(140, 70, 210));
            } else {
                for (int i = 0; i < nStops; i++)
                    drawBadge(V(g.x(stops[i]), g.y(stops[i]) - 28), TextFormat("%d", i + 1), C(110, 116, 130));
            }
            if (start >= 0)
                drawBadge(V(g.x(start), g.y(start) - 28), "0", C(24, 160, 88)); // 0 = where you start
        }
    }

    //  right-hand panel text 
    void drawPanel(const Graph &g, int px){
        int y = PANEL_TOP;
        Color dark = C(30, 34, 46), grey = C(100, 104, 116);
        if (tool == TOOL_MENU){
            drawText("TOOLS (simulation paused)", px, PANEL_TOP, pf(16), dark);
            const char *names[4] = {"1. Compare routing algorithms", "2. Multi-stop trip planner", "3. Hash table showdown", "4. Sorting benchmark"};
            for (int i = 0; i < 4; i++){
                Rectangle row = R(MAP_W + 12, TOOLS_ROW_TOP + i * TOOLS_ROW_H, 296, 36);
                DrawRectangleRounded(row, 0.2f, 6, CheckCollisionPointRec(GetMousePosition(), row) ? C(214, 228, 255) : C(238, 240, 246));
                drawText(names[i], px + 4, TOOLS_ROW_TOP + i * TOOLS_ROW_H + 8, pf(16), dark);
            }
            text("Click a tool or press 1-4. ESC goes back. T closes the tools.", px, TOOLS_ROW_TOP + 4 * TOOLS_ROW_H + 6, pf(13), grey);
        } else if (tool == TOOL_COMPARE) {
            y = text("Routing algorithms", px, y - 4, pf(18), dark);
            if (!compared)
                y = text(a < 0 ? "Click area A on the map." : "Now click area B.", px, y + 4, pf(14), grey);
            else {
                const char *nm[3] = {"BFS (orange)", "Dijkstra (blue)", "A* (green)"};
                Color cols[3] = {C(255, 150, 40), C(40, 105, 240), C(30, 170, 90)};
                y += 6;
                for (int k = 0; k < 3; k++) {
                    DrawRectangle(px, y + 2, 12, 12, cols[k]);
                    drawText(nm[k], px + 20, y, pf(15), dark);
                    if (res[k].hops < 0)
                        drawText("no route", px + 20, y + 22, pf(13), grey);
                    else {
                        drawText(TextFormat("%.1f km | %d roads", res[k].km, res[k].hops), px + 20, y + 22, pf(13), grey);
                        drawText(fitText(TextFormat("explored %d areas | %.1f us per run", res[k].explored, res[k].us), pf(13), 268).c_str(), px + 20, y + 41, pf(13), grey);
                    }
                    y += 68;
                }
                y = text("BFS counts roads, not km, so it can pick a longer route. A* looks at fewer areas than Dijkstra and still finds the shortest one. Click again to compare another trip.", px, y, pf(13), grey);
            }
        } else if (tool == TOOL_MULTI) {
            y = text("Multi-stop planner (backtracking)", px, y - 4, pf(18), dark);
            if (!planned){
                y = text("Click the START area (marked 0), then up to 6 stops. ENTER = plan, BACKSPACE = clear.", px, y + 4, pf(14), grey);
                if (start >= 0)
                    y = text("Start: " + g.name(start), px, y + 6, pf(14), dark);
                for (int i = 0; i < nStops; i++)
                    y = text(string(TextFormat("Stop %d: ", i + 1)) + g.name(stops[i]), px, y, pf(14), dark);
            } else {
                string order = g.name(start);
                for (int i = 0; i < nStops; i++)
                    order += " > " + g.name(visit[i]);
                y = text("Best order: " + order, px, y + 4, pf(14), dark);
                y = text(string(TextFormat("Total %.1f km (in the order you clicked: %.1f km)", plan.best, naive)), px, y + 6, pf(14), C(24, 100, 220));
                y = text(string(TextFormat("Backtracking looked at %ld partial routes instead of %ld, because it abandons a branch once it is longer than the best route found.", plan.visited, fullTree)), px, y + 6, pf(13), grey);
                y = text("Click any area to start a new plan.", px, y + 6, pf(13), grey);
            }
        } else if (tool == TOOL_HASH) {
            y = text("Hash table showdown", px, y - 4, pf(18), dark);
            y = text(string(TextFormat("Inserting %d user ids. UP / DOWN changes the number.", hashSize(hashIdx))), px, y + 4, pf(14), grey);
        } else if (tool == TOOL_SORT) {
            y = text("Sorting benchmark", px, y - 4, pf(18), dark);
            y = text(string(TextFormat("Sorting %d random fares. UP / DOWN changes the number.", sortSize(sortIdx))), px, y + 4, pf(14), grey);
        }
        if (msg.size())
            text(msg, px, y + 8, pf(14), C(200, 40, 40));
    }

    //  big charts drawn over the map 
    void drawCharts(){
        if (tool != TOOL_HASH && tool != TOOL_SORT)
            return;
        Rectangle box = R(60, 90, 840, tool == TOOL_HASH ? 400 : 500);
        DrawRectangleRounded(box, 0.03f, 8, Fade(WHITE, 0.97f));
        Color dark = C(30, 34, 46), grey = C(100, 104, 116);
        if (tool == TOOL_HASH && hashRan){
            drawText(TextFormat("Collisions while inserting %d ids (fewer = better)", hashSize(hashIdx)), 90, 112, 20, dark);
            const char *nm[3] = {"Chaining", "Linear probing", "Double hashing"};
            Color cols[3] = {C(40, 105, 240), C(255, 150, 40), C(30, 170, 90)};
            long mx = 1;
            for (int i = 0; i < 3; i++)
                if (hCol[i] > mx)
                    mx = hCol[i];
            for (int i = 0; i < 3; i++){
                int y = 170 + i * 80;
                drawText(nm[i], 90, y + 8, 18, dark);
                float w = 440.0f * (float)hCol[i] / (float)mx;
                DrawRectangle(290, y, (int)w + 2, 36, cols[i]);
                drawText(TextFormat("%ld", hCol[i]), 300 + (int)w, y + 8, 20, dark);
                drawText(TextFormat("rehashes: %d   final table size: %d", hReh[i], hCap[i]), 290, y + 42, 13, grey);
            }
            text("Chaining hangs a list off each slot. Linear probing walks to the next free slot (clusters form). Double hashing jumps by a second hash, which spreads keys out. The table grows to the next prime size when it gets too full.", 90, 420, 14, grey, 760);
        }
        if (tool == TOOL_SORT && sortRan){
            drawText(TextFormat("Time to sort %d numbers (bar length is a log scale)", sortSize(sortIdx)), 90, 112, 20, dark);
            const char *nm[5] = {"Bubble sort", "Insertion sort", "Merge sort", "Quick sort", "Radix sort"};
            Color cols[5] = {C(220, 60, 60), C(255, 150, 40), C(40, 105, 240), C(30, 170, 90), C(140, 70, 210)};
            for (int i = 0; i < 5; i++){
                int y = 160 + i * 66;
                drawText(nm[i], 90, y + 8, 18, dark);
                float us = (float)(sortMs[i] * 1000.0) + 1.0f; // microseconds, log scale
                float w = 440.0f * log10f(us) / 6.0f;          // 1 us .. 1 s
                if (w < 4)
                    w = 4;
                DrawRectangle(260, y, (int)w, 36, cols[i]);
                if (sortMs[i] < 1.0)
                    drawText(TextFormat("%.0f us", sortMs[i] * 1000.0), 270 + (int)w, y + 8, 20, dark);
                else
                    drawText(TextFormat("%.2f ms", sortMs[i]), 270 + (int)w, y + 8, 20, dark);
            }
            text("Bubble and insertion sort compare almost every pair: their time grows with n squared. Merge and quick sort divide the list: n log n. Radix sort never compares numbers; it sorts digit by digit.", 90, 500, 14, grey, 760);
        }
    }
};
