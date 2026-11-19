#include "raylib.h"
#include "ui_font.h"
#include <cmath>
#include <cstring>
#include <string>
#include <fstream>
#include "DynamicArray.h"
#include "Graph.h"
using namespace std;

static int WIN_W = 1280, WIN_H = 760;
const int BAR_H = 64, MAXN = 64;

static Vector2 V(float x, float y)
{
    Vector2 v;
    v.x = x;
    v.y = y;
    return v;
}

static Rectangle R(float x, float y, float w, float h)
{
    Rectangle r;
    r.x = x;
    r.y = y;
    r.width = w;
    r.height = h;
    return r;
}

static Color C(int r, int g, int b, int a = 255)
{
    Color c;
    c.r = (unsigned char)r;
    c.g = (unsigned char)g;
    c.b = (unsigned char)b;
    c.a = (unsigned char)a;
    return c;
}

struct ENode
{
    string name;
    float x, y;
};

struct EEdge
{
    int a, b;
};

struct EWeight
{
    int a, b;
    float ab, ba;
};       // real road km from weights.txt (a->b and b->a)

static float distToSegment(Vector2 p, Vector2 a, Vector2 b)
{
    float dx = b.x - a.x, dy = b.y - a.y, len2 = dx * dx + dy * dy;
    float t = len2 == 0 ? 0 : ((p.x - a.x) * dx + (p.y - a.y) * dy) / len2;
    if (t < 0)
    {
        t = 0;
    }
    if (t > 1)
    {
        t = 1;
    }
    float cx = a.x + t * dx - p.x, cy = a.y + t * dy - p.y;
    return sqrtf(cx * cx + cy * cy);
}

struct Editor
{
    DynamicArray<ENode> nodes;
    DynamicArray<EEdge> edges;
    int selected;                          // first area clicked while drawing a road
    bool typing;     
    string input, msg;
    float pendX, pendY;
    DynamicArray<EWeight> weights;
    bool haveWeights, weightsOk;           // file exists / file matches the map as it is now
    unsigned long weightsHash;

    Editor() : selected(-1), typing(false), pendX(0), pendY(0), haveWeights(false), weightsOk(false), weightsHash(0)
    {
    }

    int findEdge(int a, int b) const
    {
        for (int i = 0; i < edges.size(); i++)
        {
            if ((edges[i].a == a && edges[i].b == b) || (edges[i].a == b && edges[i].b == a))
            {
                return i;
            }
        }
        return -1;
    }

    void addNode(const string& name, float x, float y)
    {
        if (nodes.size() >= MAXN)
        {
            msg = "Limit reached: the game supports 64 areas.";
            return;
        }
        ENode n;
        n.name = name;
        n.x = x;
        n.y = y;
        nodes.push_back(n);
        refreshWeights();
    }

    void connect(int a, int b)
    {
        if (a == b)
        {
            return;
        }
        if (findEdge(a, b) >= 0)
        {
            msg = "Those areas already have a road.";
            return;
        }
        EEdge e;
        e.a = a;
        e.b = b;
        edges.push_back(e);
        refreshWeights();
    }

    void removeNode(int n)                 // also removes its roads and renumbers the rest
    {
        for (int k = edges.size() - 1; k >= 0; k--)
        {
            if (edges[k].a == n || edges[k].b == n)
            {
                edges.removeAt(k);
            }
            else
            {
                if (edges[k].a > n)
                {
                    edges[k].a--;
                }
                if (edges[k].b > n)
                {
                    edges[k].b--;
                }
            }
        }
        nodes.removeAt(n);
        selected = -1;
        refreshWeights();
    }

    int nodeAt(Vector2 w, float r) const
    {
        for (int i = 0; i < nodes.size(); i++)
        {
            float dx = nodes[i].x - w.x, dy = nodes[i].y - w.y;
            if (dx * dx + dy * dy <= r * r)
            {
                return i;
            }
        }
        return -1;
    }

    int edgeAt(Vector2 w, float tol) const
    {
        for (int i = 0; i < edges.size(); i++)
        {
            if (distToSegment(w, V(nodes[edges[i].a].x, nodes[edges[i].a].y), V(nodes[edges[i].b].x, nodes[edges[i].b].y)) <= tol)
            {
                return i;
            }
        }
        return -1;
    }

    string nodesText() const
    {
        string t;
        for (int i = 0; i < nodes.size(); i++)
        {
            string nm = nodes[i].name;
            for (size_t k = 0; k < nm.size(); k++)
            {
                if (nm[k] == ' ')
                {
                    nm[k] = '_';
                }
            }
            t += nm + " " + to_string((int)nodes[i].x) + " " + to_string((int)nodes[i].y) + "\n";
        }
        return t;
    }

    string edgesText() const
    {
        string t;
        for (int i = 0; i < edges.size(); i++)
        {
            t += to_string(edges[i].a) + " " + to_string(edges[i].b) + "\n";
        }
        return t;
    }

    void loadWeights()
    {
        weights.clear();
        haveWeights = false;
        weightsHash = 0;
        ifstream f("weights.txt");
        string tag;
        if (!f || !(f >> tag >> weightsHash) || tag != "map")
        {
            return;
        }
        haveWeights = true;
        EWeight w;
        while (f >> w.a >> w.b >> w.ab >> w.ba)
        {
            weights.push_back(w);
        }
    }

    void refreshWeights()
    {
        weightsOk = haveWeights && weightsHash == fnv1a(nodesText() + edgesText());
    }

    bool realKm(int edge, float& ab, float& ba) const          // real distance of road 'edge', if weights.txt is valid for this map
    {
        if (!weightsOk)
        {
            return false;
        }
        int a = edges[edge].a, b = edges[edge].b;
        for (int i = 0; i < weights.size(); i++)
        {
            if (weights[i].a == a && weights[i].b == b)
            {
                ab = weights[i].ab;
                ba = weights[i].ba;
                return true;
            }
            if (weights[i].a == b && weights[i].b == a)
            {
                ab = weights[i].ba;
                ba = weights[i].ab;
                return true;
            }
        }
        return false;
    }

    int realCount() const
    {
        float x, y;
        int c = 0;
        for (int i = 0; i < edges.size(); i++)
        {
            if (realKm(i, x, y))
            {
                c++;
            }
        }
        return c;
    }

    string weightsHint() const              // what to tell the user about weights.txt after loading / saving
    {
        if (!haveWeights)
        {
            return " Run fetch_distances.py to get real road distances.";
        }
        if (!weightsOk)
        {
            return " The road distances are out of date: run fetch_distances.py.";
        }
        int missing = edges.size() - realCount();
        if (missing > 0)
        {
            return " " + to_string(missing) + " road(s) have no real distance yet: run fetch_distances.py.";
        }
        return "";
    }

    bool save()
    {
        ofstream fn("nodes.txt"), fe("edges.txt");
        if (!fn || !fe)
        {
            return false;
        }
        fn << nodesText();
        fe << edgesText();
        fn.close();
        fe.close();
        refreshWeights();
        return true;
    }

    bool load()
    {
        ifstream fn("nodes.txt"), fe("edges.txt");
        if (!fn || !fe)
        {
            return false;
        }
        nodes.clear();
        edges.clear();
        selected = -1;
        string nm;
        float x, y;
        int a, b;
        while (fn >> nm >> x >> y)
        {
            for (size_t k = 0; k < nm.size(); k++)
            {
                if (nm[k] == '_')
                {
                    nm[k] = ' ';
                }
            }
            addNode(nm, x, y);
        }
        while (fe >> a >> b)
        {
            if (a >= 0 && b >= 0 && a < nodes.size() && b < nodes.size())
            {
                connect(a, b);
            }
        }
        loadWeights();
        refreshWeights();
        return true;
    }

    string check() const            // every area must be reachable from every other, or trips can fail
    {
        int n = nodes.size();
        if (n == 0)
        {
            return "The map is empty.";
        }
        bool* reached = new bool[n];
        for (int i = 0; i < n; i++)
        {
            reached[i] = (i == 0);
        }
        for (bool changed = true; changed;)
        {
            changed = false;
            for (int k = 0; k < edges.size(); k++)
            {
                if (reached[edges[k].a] != reached[edges[k].b])
                {
                    reached[edges[k].a] = reached[edges[k].b] = true;
                    changed = true;
                }
            }
        }
        int cut = 0;
        string first;
        for (int i = 0; i < n; i++)
        {
            if (!reached[i])
            {
                if (!cut)
                {
                    first = nodes[i].name;
                }
                cut++;
            }
        }
        delete[] reached;
        string s = to_string(n) + " areas, " + to_string(edges.size()) + " roads. ";
        if (cut == 0)
        {
            return s + "OK: every area is connected.";
        }
        return s + "WARNING: " + to_string(cut) + " area(s) are cut off from the rest (e.g. " + first + "). Add a road to join them.";
    }
};

int main(int argc, char** argv)
{
    bool demo = argc > 1 && strcmp(argv[1], "--demo") == 0;
    Editor ed;
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(WIN_W, WIN_H, "Chalgorithm - map editor");
    fitWindowToScreen(WIN_W, WIN_H);               // use most of the screen: a bigger canvas to place areas on
    SetExitKey(KEY_NULL);                          // ESC must not close the window while typing
    SetTargetFPS(60);
    loadUiFonts();

    bool hasMap = FileExists("map.png");
    Texture2D tex;
    memset(&tex, 0, sizeof(tex));
    float scale = 1;                               // map.png is fitted into the MAP_BOX_W x MAP_BOX_H box (Graph.h, same rule as gui_main)
    if (hasMap)
    {
        tex = LoadTexture("map.png");
        scale = fminf(MAP_BOX_W / tex.width, MAP_BOX_H / tex.height);
    }

    Camera2D cam;
    cam.offset = V(WIN_W / 2.0f, (WIN_H - BAR_H) / 2.0f);
    cam.target = V(MAP_BOX_W / 2, MAP_BOX_H / 2);
    cam.rotation = 0;
    cam.zoom = fminf(WIN_W / MAP_BOX_W, (WIN_H - BAR_H) / MAP_BOX_H) * 0.95f;   // start with the whole map area in view, as big as fits

    if (ed.load())
    {
        ed.msg = "Loaded your saved map. " + ed.check() + ed.weightsHint();
    }
    else
    {
        ed.msg = hasMap ? "Click on the map to place your first area." : "No map.png found - drawing on a blank canvas.";
    }
    int frame = 0;

    while (!WindowShouldClose())
    {
        frame++;
        Vector2 mouse = GetMousePosition();
        Vector2 w = GetScreenToWorld2D(mouse, cam);
        bool inCanvas = mouse.y < WIN_H - BAR_H;

        if (demo)                                  // builds a small map by itself (used to check the editor works)
        {
            if (frame == 5 && ed.nodes.size() == 0)
            {
                const char* nm[] = {"Saddar", "Clifton", "Gulshan", "Airport", "North Karachi"};
                float xs[] = {300, 380, 560, 760, 480}, ys[] = {420, 560, 250, 300, 120};
                for (int i = 0; i < 5; i++)
                {
                    ed.addNode(nm[i], xs[i], ys[i]);
                }
                ed.connect(0, 1);
                ed.connect(0, 2);
                ed.connect(2, 3);
                ed.connect(2, 4);
                ed.connect(0, 4);
                ed.removeNode(1);                  // delete Clifton again: roads and numbers must stay consistent
                ed.addNode("DHA", 600, 520);
                ed.connect(0, 4 /* DHA is now index 4 */);
                ed.selected = 2;
                ed.save();
                ed.msg = "Saved. " + ed.check() + ed.weightsHint();
            }
            if (frame == 20)
            {
                TakeScreenshot("editor_shot.png");
                break;
            }
        }

        if (IsKeyPressed(KEY_F12))
        {
            TakeScreenshot("editor_shot.png");
        }
        float wheel = GetMouseWheelMove();
        if (wheel != 0)
        {
            cam.zoom += wheel * 0.1f * cam.zoom;
            if (cam.zoom < 0.15f)
            {
                cam.zoom = 0.15f;
            }
            if (cam.zoom > 5)
            {
                cam.zoom = 5;
            }
        }
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
        {
            Vector2 d = GetMouseDelta();
            cam.target = V(cam.target.x - d.x / cam.zoom, cam.target.y - d.y / cam.zoom);
        }

        if (ed.typing)                             // naming a new area
        {
            int ch;
            while ((ch = GetCharPressed()) > 0)
            {
                if (ch >= 32 && ch < 127 && ed.input.size() < 22)
                {
                    ed.input += (char)ch;
                }
            }
            if (IsKeyPressed(KEY_BACKSPACE) && !ed.input.empty())
            {
                ed.input.pop_back();
            }
            if (IsKeyPressed(KEY_ESCAPE))
            {
                ed.typing = false;
            }
            if (IsKeyPressed(KEY_ENTER))
            {
                ed.addNode(ed.input.empty() ? "Area " + to_string(ed.nodes.size() + 1) : ed.input, ed.pendX, ed.pendY);
                ed.typing = false;
                ed.msg = "";
            }
        }
        else
        {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && inCanvas)
            {
                int hit = ed.nodeAt(w, 16 / cam.zoom + 4);
                if (hit >= 0)
                {
                    if (ed.selected < 0)
                    {
                        ed.selected = hit;
                        ed.msg = "Now click the area this road should connect to.";
                    }
                    else if (ed.selected == hit)
                    {
                        ed.selected = -1;
                        ed.msg = "";
                    }
                    else
                    {
                        ed.msg = "";
                        ed.connect(ed.selected, hit);
                        ed.selected = -1;
                    }
                }
                else if (ed.selected >= 0)
                {
                    ed.selected = -1;
                    ed.msg = "";
                }
                else if (ed.nodes.size() < MAXN)
                {
                    ed.typing = true;
                    ed.input = "";
                    ed.pendX = w.x;
                    ed.pendY = w.y;
                }
                else
                {
                    ed.msg = "Limit reached: the game supports 64 areas.";
                }
            }
            if (IsKeyPressed(KEY_X) || IsKeyPressed(KEY_DELETE))
            {
                int n = ed.nodeAt(w, 16 / cam.zoom + 4), e = ed.edgeAt(w, 8 / cam.zoom);
                if (n >= 0)
                {
                    ed.removeNode(n);
                    ed.msg = "Area deleted (and its roads).";
                }
                else if (e >= 0)
                {
                    ed.edges.removeAt(e);
                    ed.refreshWeights();
                    ed.msg = "Road deleted.";
                }
            }
            if (IsKeyPressed(KEY_S))
            {
                ed.msg = ed.save() ? "Saved nodes.txt and edges.txt. " + ed.check() + ed.weightsHint() : "Could not write the files.";
            }
            if (IsKeyPressed(KEY_L))
            {
                ed.msg = ed.load() ? "Loaded. " + ed.check() + ed.weightsHint() : "No saved map found.";
            }
            if (IsKeyPressed(KEY_V))
            {
                ed.msg = ed.check();
            }
            if (IsKeyPressed(KEY_ESCAPE))
            {
                ed.selected = -1;
                ed.msg = "";
            }
        }

        BeginDrawing();
        ClearBackground(C(58, 62, 72));
        BeginScissorMode(0, 0, WIN_W, WIN_H - BAR_H);
        BeginMode2D(cam);
            DrawRectangle(0, 0, (int)MAP_BOX_W, (int)MAP_BOX_H, C(236, 232, 224));   // the map area (same beige as the game's background)
            if (hasMap)
            {
                DrawTextureEx(tex, V(0, 0), 0, scale, WHITE);
            }
            int hoverN = ed.typing ? -1 : ed.nodeAt(w, 16 / cam.zoom + 4);
            int hoverE = (hoverN < 0 && !ed.typing) ? ed.edgeAt(w, 8 / cam.zoom) : -1;
            for (int i = 0; i < ed.edges.size(); i++)                   // roads with their length in km
            {
                Vector2 a = V(ed.nodes[ed.edges[i].a].x, ed.nodes[ed.edges[i].a].y), b = V(ed.nodes[ed.edges[i].b].x, ed.nodes[ed.edges[i].b].y);
                DrawLineEx(a, b, 11, i == hoverE ? C(230, 90, 90) : C(176, 180, 188));
                DrawLineEx(a, b, 7, i == hoverE ? C(255, 190, 190) : WHITE);
                float ab, ba;
                string lab;
                Color labCol = C(90, 94, 106);
                if (ed.realKm(i, ab, ba))                                // real road distance from weights.txt
                {
                    lab = fabsf(ab - ba) > 0.05f ? string(TextFormat("%.1f | %.1f km", ab, ba)) : string(TextFormat("%.1f km", ab));
                    labCol = C(20, 110, 60);
                }
                else                                                     // estimate from the picture
                {
                    float px = sqrtf((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y)) / 40.0f;
                    lab = string(TextFormat("~%.1f km", px));
                }
                drawText(lab.c_str(), (int)((a.x + b.x) / 2) + 4, (int)((a.y + b.y) / 2) - 16, 12, labCol);
            }
            if (ed.selected >= 0 && hoverN != ed.selected)                // rubber-band line while drawing a road
            {
                DrawLineEx(V(ed.nodes[ed.selected].x, ed.nodes[ed.selected].y), w, 4, C(60, 130, 240));
            }
            for (int i = 0; i < ed.nodes.size(); i++)
            {
                Color c = i == ed.selected ? C(250, 190, 30) : (i == hoverN ? C(60, 130, 240) : C(110, 116, 130));
                DrawCircleV(V(ed.nodes[i].x, ed.nodes[i].y), 9, WHITE);
                DrawCircleV(V(ed.nodes[i].x, ed.nodes[i].y), 6.5f, c);
                drawLabel(ed.nodes[i].name.c_str(), (int)ed.nodes[i].x + 12, (int)ed.nodes[i].y - 9, 15, C(40, 44, 54));
            }
            if (ed.typing)
            {
                DrawCircleV(V(ed.pendX, ed.pendY), 8, C(24, 160, 88));
            }
        EndMode2D();
        EndScissorMode();

        DrawRectangle(0, WIN_H - BAR_H, WIN_W, BAR_H, C(30, 33, 42));
        drawText("click empty = new area | click area, then another = road | X = delete under mouse | S save | L load | V check | wheel zoom | right-drag pan",
                 12, WIN_H - BAR_H + 8, 14, C(190, 195, 210));
        drawText(ed.msg.c_str(), 12, WIN_H - BAR_H + 32, 16, ed.msg.find("WARNING") != string::npos || ed.msg.find("Limit") != string::npos ? C(255, 150, 90) : C(140, 230, 170));
        drawText(TextFormat("%d areas  %d roads", ed.nodes.size(), ed.edges.size()), WIN_W - 150, 10, 16, C(230, 232, 240));
        {
            string ws;
            Color wc;
            if (ed.weightsOk)
            {
                ws = string(TextFormat("Real road distances: %d of %d roads", ed.realCount(), ed.edges.size()));
                wc = C(140, 230, 170);
            }
            else if (ed.haveWeights)
            {
                ws = "Distances out of date - run fetch_distances.py";
                wc = C(255, 170, 100);
            }
            else
            {
                ws = "No weights.txt - distances are estimates (~)";
                wc = C(190, 195, 210);
            }
            int tw = measureText(ws.c_str(), 13);
            DrawRectangleRounded(R((float)(WIN_W - tw - 26), 30, (float)(tw + 20), 24), 0.4f, 6, C(30, 33, 42, 235));      // dark pill so it is readable over the map
            drawText(ws.c_str(), WIN_W - tw - 16, 34, 13, wc);
        }
        if (ed.typing)
        {
            DrawRectangle(WIN_W / 2 - 230, 20, 460, 70, C(250, 250, 252));
            drawText("Name this area (ENTER = ok, ESC = cancel):", WIN_W / 2 - 215, 28, 16, C(40, 44, 54));
            drawText(TextFormat("%s_", ed.input.c_str()), WIN_W / 2 - 215, 54, 22, C(24, 100, 220));
        }
        EndDrawing();
    }
    if (hasMap)
    {
        UnloadTexture(tex);
    }
    unloadUiFonts();
    CloseWindow();
    return 0;
}