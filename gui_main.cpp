// gui_main.cpp - live map. Uses Simulation.h UNCHANGED: the GUI only draws what the simulation does.
// Choose a role at the start (each role sees a different map):
//   USER  : click pickup, click destination, ENTER, riders' offers arrive live - pick one before it expires (time keeps running).
//   RIDER : click where your shift starts, click a waiting customer, set your bid, ENTER, then drive them.
//           You see waiting customers and your own car only.
//   DISPATCHER : sees every car, customer, ride and event. Cannot book or drive; click a road to close or reopen it.
// Mouse wheel = zoom | right-drag = pan | P pause | +/- speed | F follow car | TAB leaderboard (dispatcher) | T tools
// Optional: map.png (+ nodes.txt / edges.txt from the map editor) next to the program. Run with --demo to watch it play.
#include "raylib.h"
#include <cmath>
#include <cstring>
#include <string>
#include "Simulation.h"
#include "gui_draw.h"
#include "gui_tools.h"
#include <algorithm>
using namespace std;

const float TICK = 0.9f; // seconds of real time per simulation tick

enum Role
{
    NO_ROLE,
    USER_ROLE,
    RIDER_ROLE,
    ADMIN_ROLE
}; // ADMIN = the dispatcher
enum Phase
{
    CHOOSE_ROLE,
    U_PICKUP,
    U_DEST,
    U_OFFER,
    U_BIDDING,
    U_RIDING,
    U_DONE, // you are the customer
    R_START,
    R_BROWSE,
    R_BID,
    R_DRIVING,
    R_DONE, // you are the driver
    D_WATCH
}; // you are the dispatcher

// ---------- the game: what the player is doing right now ----------
struct Game
{
    Simulation sim;
    Role role;
    Phase phase;
    User *me;       // you, when you are the customer
    Rider *meRider; // you, when you are the driver (owned by sim.riders)
    int pickup, dest;
    float offer, suggested; // customer: the fare you offer / the suggested fare
    User *selReq;           // driver: the customer you are bidding on
    float bidPrice, bidCost;
    Ride *myRide;
    float earned;
    int rides;
    string msg;
    DynamicArray<LogEntry> lines; // every event; each role only reads its own lines

    Game() : role(NO_ROLE), phase(CHOOSE_ROLE), me(new User(sim.nextId++, "You", 0, false)), meRider(0),
             pickup(-1), dest(-1), offer(0), suggested(0), selReq(0), bidPrice(0), bidCost(0), myRide(0), earned(0), rides(0) {}
    ~Game() { delete me; }

    void drain()
    {
        for (int i = 0; i < sim.log.size(); i++)
            lines.push_back(sim.log[i]);
        sim.log.clear();
    }
    void chooseUser()
    {
        role = USER_ROLE;
        phase = U_PICKUP;
    }
    void chooseRider()
    {
        role = RIDER_ROLE;
        phase = R_START;
    }
    void chooseAdmin()
    {
        role = ADMIN_ROLE;
        phase = D_WATCH;
    }
    bool seesLog(const LogEntry &e) const
    { // who may read this event?
        if (role == ADMIN_ROLE)
            return true;
        if (role == USER_ROLE)
            return e.userId == me->id;
        if (role == RIDER_ROLE)
            return meRider && e.riderId == meRider->id;
        return false;
    }
    void toggleRoad(int a, int b) { sim.blockRoad(a, b, !sim.graph.isBlocked(a, b)); }
    void reset()
    {
        msg = "";
        sim.stopPlayerRequest();
        myRide = 0;
        selReq = 0;
        pickup = dest = -1;
        phase = role == RIDER_ROLE ? R_BROWSE : U_PICKUP;
    }
    bool stillOpen(User *u)
    {
        for (int i = 0; i < sim.open.size(); i++)
            if (sim.open[i] == u)
                return true;
        return false;
    }

    void click(int n)
    { // player clicked area n on the map
        if (phase == U_PICKUP)
        {
            pickup = n;
            phase = U_DEST;
            msg = "";
        }
        else if (phase == U_DEST && n != pickup)
        {
            suggested = sim.suggestedFare(pickup, n);
            if (suggested < 0)
            {
                msg = "No route to that area.";
                return;
            }
            dest = n;
            offer = (float)((int)(suggested / 10) * 10);
            phase = U_OFFER;
            msg = "";
        }
        else if (phase == R_START)
        {
            meRider = sim.addPlayerRider("You", n);
            phase = R_BROWSE;
            msg = "";
        }
        else if (phase == R_BROWSE)
            for (int i = 0; i < sim.open.size(); i++)
                if (sim.open[i]->pickup == n)
                {
                    selectRequest(i);
                    break;
                }
    }

    // ----- customer side -----
    void request()
    { // ENTER: post the request, riders bid
        me->pickup = pickup;
        me->dest = dest;
        me->offer = offer;
        me->node = pickup;
        if (!sim.startPlayerRequest(me))
        {
            msg = "No riders bid. Raise your offer (UP) and try again.";
            return;
        }
        phase = U_BIDDING;
        msg = ""; // the game keeps running: offers arrive and expire in real time
    }
    void accept(int i)
    { // i = place in the offer queue (0 = the oldest offer)
        Ride *r = sim.acceptOffer(i);
        if (!r)
        {
            msg = "That rider is no longer available.";
            return;
        }
        myRide = r;
        phase = U_RIDING;
        msg = "";
    }

    // ----- driver side -----
    void selectRequest(int i)
    {
        if (i < 0 || i >= sim.open.size())
            return;
        User *u = sim.open[i];
        float pd = sim.distance(meRider->node, u->pickup), trip = sim.distance(u->pickup, u->dest);
        if (pd < 0 || trip < 0)
        {
            msg = "No route to that customer.";
            return;
        }
        selReq = u;
        bidCost = (pd + trip) * meRider->perKm;
        bidPrice = (float)((int)(bidCost / 10) * 10 + 10);
        phase = R_BID;
        msg = "";
    }
    void placeBid()
    {
        if (!stillOpen(selReq))
        {
            msg = "That customer is gone.";
            selReq = 0;
            phase = R_BROWSE;
            return;
        }
        if (sim.playerBid(meRider, selReq, bidPrice))
        {
            myRide = sim.active[sim.active.size() - 1];
            selReq = 0;
            phase = R_DRIVING;
            msg = "";
        }
        else
        {
            msg = "The customer said no. Try a lower price, or pick someone else.";
            selReq = 0;
            phase = R_BROWSE;
        }
    }
};

static int startShift() { return (WIN_H - 760) / 2 > 0 ? (WIN_H - 760) / 2 : 0; } // start screen was laid out for a 760 px tall window

static int nodeUnder(const Graph &gr, Vector2 w)
{
    for (int i = 0; i < gr.size(); i++)
    {
        float dx = gr.x(i) - w.x, dy = gr.y(i) - w.y;
        if (dx * dx + dy * dy <= 22 * 22)
            return i;
    }
    return -1;
}

static Color logColor(LogKind k)
{ // one colour per kind of event
    switch (k)
    {
    case LOG_REQUEST:
        return C(150, 130, 210); // new customer
    case LOG_MATCH:
        return C(60, 120, 220); // driver accepted
    case LOG_PICKUP:
        return C(30, 160, 170);
    case LOG_DROPOFF:
        return C(30, 160, 90); // ride finished
    case LOG_DETOUR:
        return C(240, 150, 40);
    case LOG_TRAFFIC:
        return C(225, 110, 20);
    case LOG_REROUTE:
        return C(230, 185, 30);
    case LOG_CLOSED:
        return C(214, 50, 50);
    case LOG_REOPENED:
        return C(100, 185, 120);
    case LOG_CANCEL:
        return C(160, 160, 170);
    default:
        return C(130, 134, 146);
    }
}

static bool roadUnder(const Graph &gr, Vector2 p, float tol, int &ra, int &rb)
{ // dispatcher: which road was clicked?
    for (int i = 0; i < gr.size(); i++)
        for (int k = 0; k < gr.degree(i); k++)
        {
            int j = gr.edge(i, k).to;
            if (j < i)
                continue;
            float ax = gr.x(i), ay = gr.y(i), dx = gr.x(j) - ax, dy = gr.y(j) - ay, len2 = dx * dx + dy * dy;
            float t = len2 == 0 ? 0 : ((p.x - ax) * dx + (p.y - ay) * dy) / len2;
            if (t < 0)
                t = 0;
            if (t > 1)
                t = 1;
            float cx = ax + t * dx - p.x, cy = ay + t * dy - p.y;
            if (cx * cx + cy * cy <= tol * tol)
            {
                ra = i;
                rb = j;
                return true;
            }
        }
    return false;
}

int main(int argc, char **argv)
{
    bool demo = argc > 1 && strncmp(argv[1], "--demo", 6) == 0;
    string dm = demo ? string(argv[1] + 6) : ""; // "" user ride | "2" traffic | "-rider" | "-tools"
    Game g;
    Tools tools;
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(WIN_W, WIN_H, "Chalgorithm");
    fitWindowToScreen(WIN_W, WIN_H);
    MAP_W = WIN_W - PANEL_W; // use most of the screen: the map area grows, the panel stays 320 wide
    SetExitKey(KEY_NULL);    // ESC is used by the tools; close the window with its X button
    SetTargetFPS(60);
    loadUiFonts(); // Inter, from the fonts/ folder

    bool hasMap = FileExists("map.png");
    Texture2D mapTex;
    memset(&mapTex, 0, sizeof(mapTex));
    float mapScale = 1; // map.png is fitted into the MAP_BOX_W x MAP_BOX_H box (Graph.h), same as in the editor
    if (hasMap)
    {
        mapTex = LoadTexture("map.png");
        mapScale = fminf(MAP_BOX_W / mapTex.width, MAP_BOX_H / mapTex.height);
    }

    // logo.png (optional, next to the program): shown instead of the "CHALGORITHM" text. It is resized once here
    // (480 px wide for the start screen, 34 px tall for the panel) so it stays sharp. Without the file the text is used.
    bool hasLogo = FileExists("logo.png");
    Texture2D logoBig, logoSmall;
    memset(&logoBig, 0, sizeof(logoBig));
    memset(&logoSmall, 0, sizeof(logoSmall));
    if (hasLogo)
    {
        Image li = LoadImage("logo.png");
        Image big = ImageCopy(li), small = ImageCopy(li);
        ImageResize(&big, 480, (int)(480.0f * li.height / li.width));
        ImageResize(&small, (int)(34.0f * li.width / li.height), 34);
        logoBig = LoadTextureFromImage(big);
        logoSmall = LoadTextureFromImage(small);
        UnloadImage(big);
        UnloadImage(small);
        UnloadImage(li);
    }

    Camera2D cam;
    cam.offset = V(MAP_W / 2.0f, WIN_H / 2.0f);
    float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f; // zoom so the whole map fits the window
    for (int i = 0; i < g.sim.graph.size(); i++)
    {
        if (g.sim.graph.x(i) < x0)
            x0 = g.sim.graph.x(i);
        if (g.sim.graph.x(i) > x1)
            x1 = g.sim.graph.x(i);
        if (g.sim.graph.y(i) < y0)
            y0 = g.sim.graph.y(i);
        if (g.sim.graph.y(i) > y1)
            y1 = g.sim.graph.y(i);
    }
    cam.target = V((x0 + x1) / 2, (y0 + y1) / 2);
    float zx = MAP_W / (x1 - x0 + 200), zy = WIN_H / (y1 - y0 + 200);
    cam.rotation = 0;
    cam.zoom = zx < zy ? zx : zy;

    float acc = 0; // time since the last simulation tick
    int speed = 1;
    bool paused = false, follow = false, showBoard = false;
    int logScroll = 0;
    int driverScroll = 0;
    float heading[64];
    for (int i = 0; i < 64; i++)
        heading[i] = 0;
    int shots = 0, demoStep = 0;
    bool closedForDemo = false;
    double lastAct = 0;

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();
        if (dt > 0.1f)
            dt = 0.1f;
        double now = GetTime();
        const Graph &gr = g.sim.graph;
        Vector2 mouse = GetMousePosition();
        bool inMap = mouse.x < MAP_W;
        bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        float wheel = GetMouseWheelMove();
        int nAreas = gr.size();

        // ----- demo autopilot (for --demo...): plays by itself so the look can be checked -----
        if (demo)
        {
            bool riderDemo = dm == "-rider", toolsDemo = dm == "-tools", adminDemo = dm == "-admin", traffic = dm == "2" && nAreas >= 13;
            if (now > 0.3 && g.phase == CHOOSE_ROLE)
            {
                if (riderDemo)
                    g.chooseRider();
                else if (adminDemo)
                    g.chooseAdmin();
                else
                    g.chooseUser();
            }
            if (toolsDemo)
            {
                static const double stepAt[] = {0.6, 1.4, 2.4, 3.4, 4.4};
                if (demoStep < 5 && now > stepAt[demoStep])
                {
                    int far = nAreas > 7 ? 7 : nAreas - 1;
                    if (demoStep == 0)
                    {
                        tools.open = true;
                        tools.tool = TOOL_MENU;
                    }
                    else if (demoStep == 1)
                    {
                        tools.show(TOOL_COMPARE);
                        tools.pickNode(gr, 0);
                        tools.pickNode(gr, far);
                    }
                    else if (demoStep == 2)
                    {
                        tools.show(TOOL_MULTI);
                        tools.pickNode(gr, 0);
                        int st[4] = {far, nAreas > 3 ? 3 : 1, nAreas > 12 ? 12 : 2, nAreas > 4 ? 4 : 1};
                        for (int i = 0; i < 4; i++)
                            tools.pickNode(gr, st[i]);
                        tools.runMulti(gr);
                    }
                    else if (demoStep == 3)
                        tools.show(TOOL_HASH);
                    else
                        tools.show(TOOL_SORT);
                    demoStep++;
                }
            }
            else if (riderDemo)
            {
                if (now > 0.8 && g.phase == R_START)
                    g.click(0);
                if (now - lastAct > 0.8)
                {
                    if (g.phase == R_BROWSE && g.sim.open.size() > 0 && now > 2.0)
                    {
                        g.selectRequest(0);
                        lastAct = now;
                    }
                    else if (g.phase == R_BID)
                    {
                        g.placeBid();
                        lastAct = now;
                    }
                    else if (g.phase == R_DONE)
                    {
                        g.reset();
                        lastAct = now;
                    }
                }
            }
            else if (adminDemo)
            { // dispatcher closes a road, later reopens it
                if (now > 4.0 && demoStep == 0)
                {
                    g.toggleRoad(0, 11);
                    demoStep = 1;
                }
                if (now > 9.0 && demoStep == 1)
                {
                    g.toggleRoad(0, 11);
                    demoStep = 2;
                }
            }
            else
            {
                int from = traffic ? 12 : 0, to = traffic ? 3 : (nAreas > 7 ? 7 : nAreas - 1);
                if (traffic && now > 0.5 && !closedForDemo)
                { // cut Orangi off completely: no way around
                    g.sim.blockRoad(12, 6, true);
                    g.sim.blockRoad(12, 8, true);
                    closedForDemo = true;
                    for (int i = 0; i < g.sim.riders.size(); i++)
                        g.sim.riders[i]->maxPickup = 100;
                }
                if (now > 1.0 && g.phase == U_PICKUP)
                    g.click(from);
                if (now > 1.6 && g.phase == U_DEST)
                    g.click(to);
                if (now > 2.2 && g.phase == U_OFFER)
                {
                    if (traffic)
                        g.offer = 9000;
                    g.request();
                }
                if (now > 3.0 && g.phase == U_BIDDING && g.sim.offers.size() > 0)
                    g.accept(0);
                if (!traffic && !closedForDemo && g.myRide && g.myRide->phase == 1 && g.myRide->path.size() >= 3)
                { // accident ahead
                    g.sim.blockRoad(g.myRide->path[1], g.myRide->path[2], true);
                    closedForDemo = true;
                }
            }
            if (now > 60)
                break;
        }

        // ----- camera and global keys -----
        if (inMap)
        {
            if (wheel != 0)
            {
                cam.zoom += wheel * 0.1f * cam.zoom;
                if (cam.zoom < 0.3f)
                    cam.zoom = 0.3f;
                if (cam.zoom > 5)
                    cam.zoom = 5;
            }
            if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
            {
                Vector2 d = GetMouseDelta();
                cam.target = V(cam.target.x - d.x / cam.zoom, cam.target.y - d.y / cam.zoom);
                follow = false;
            }
        }
        if (IsKeyPressed(KEY_P))
            paused = !paused;
        if (IsKeyPressed(KEY_F))
            follow = !follow;
        if (IsKeyPressed(KEY_TAB))
            showBoard = !showBoard;
        if (IsKeyPressed(KEY_T) && g.phase != CHOOSE_ROLE)
        {
            tools.open = !tools.open;
            tools.tool = TOOL_MENU;
        }
        if ((IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) && speed < 4)
            speed *= 2;
        if ((IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) && speed > 1)
            speed /= 2;

        return 0;
    }
}