// gui_main.cpp - live map. Uses Simulation.h: the GUI only draws what the simulation does.
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

        // ----- input -----
        Vector2 wmouse = GetScreenToWorld2D(mouse, cam);
        if (tools.open)
        {
            tools.update(gr, mouse, clicked);
            if (inMap && clicked)
            {
                int hit = nodeUnder(gr, wmouse);
                if (hit >= 0)
                    tools.pickNode(gr, hit);
            }
        }
        else if (g.phase == CHOOSE_ROLE)
        {
            if (IsKeyPressed(KEY_U) || (clicked && CheckCollisionPointRec(mouse, R(MAP_W / 2 - 330, 330 + startShift(), 200, 90))))
                g.chooseUser();
            if (IsKeyPressed(KEY_R) || (clicked && CheckCollisionPointRec(mouse, R(MAP_W / 2 - 100, 330 + startShift(), 200, 90))))
                g.chooseRider();
            if (IsKeyPressed(KEY_D) || (clicked && CheckCollisionPointRec(mouse, R(MAP_W / 2 + 130, 330 + startShift(), 200, 90))))
                g.chooseAdmin();
        }
        else
        {
            if (inMap && clicked)
            {
                int hit = nodeUnder(gr, wmouse), ra, rb;
                if (hit >= 0)
                    g.click(hit);
                else if (g.role == ADMIN_ROLE && roadUnder(gr, wmouse, 10 / cam.zoom, ra, rb))
                    g.toggleRoad(ra, rb); // close / reopen a road
            }
            if (IsKeyPressed(KEY_BACKSPACE) && (g.phase == U_DEST || g.phase == U_OFFER || g.phase == U_BIDDING || g.phase == R_BID))
                g.reset();
            if (g.phase == U_OFFER)
            {
                if (IsKeyPressed(KEY_UP) || IsKeyPressedRepeat(KEY_UP))
                    g.offer += 50;
                if ((IsKeyPressed(KEY_DOWN) || IsKeyPressedRepeat(KEY_DOWN)) && g.offer > 50)
                    g.offer -= 50;
                if (IsKeyPressed(KEY_ENTER))
                    g.request();
            }
            if (g.phase == U_BIDDING)
            {
                for (int i = 0; i < g.sim.offers.size() && i < offerRowsMax(); i++)
                    if (IsKeyPressed(KEY_ONE + i))
                    {
                        g.accept(i);
                        break;
                    }
                if (g.phase == U_BIDDING && clicked)
                    for (int i = 0; i < g.sim.offers.size() && i < offerRowsMax(); i++)
                        if (CheckCollisionPointRec(mouse, R(MAP_W + 12, offerRowsTop() + i * OFFER_ROW_H, 296, 40)))
                        {
                            g.accept(i);
                            break;
                        }
            }
            if (g.phase == R_BROWSE && clicked)
                for (int i = 0; i < g.sim.open.size() && i < waitRowsMax(); i++)
                    if (CheckCollisionPointRec(mouse, R(MAP_W + 12, waitRowsTop() + i * WAIT_ROW_H, 296, 60)))
                    {
                        g.selectRequest(i);
                        break;
                    }
            if (g.phase == R_BID)
            {
                if (!g.stillOpen(g.selReq))
                {
                    g.msg = "That customer is gone.";
                    g.selReq = 0;
                    g.phase = R_BROWSE;
                }
                else
                {
                    if (IsKeyPressed(KEY_UP) || IsKeyPressedRepeat(KEY_UP))
                        g.bidPrice += 10;
                    if ((IsKeyPressed(KEY_DOWN) || IsKeyPressedRepeat(KEY_DOWN)) && g.bidPrice > 10)
                        g.bidPrice -= 10;
                    if (IsKeyPressed(KEY_ENTER))
                        g.placeBid();
                }
            }
            if (g.phase == U_RIDING && g.myRide->phase == 2)
                g.phase = U_DONE;
            if (g.phase == R_DRIVING && g.myRide->phase == 2)
            {
                g.phase = R_DONE;
                g.earned += g.myRide->fare;
                g.rides++;
            }
            if ((g.phase == U_DONE || g.phase == R_DONE) && IsKeyPressed(KEY_ENTER))
                g.reset();
        }

        // ----- advance the simulation (frozen while a driver decides, in tools, or paused; NOT while you pick a rider) -----
        bool frozen = paused || tools.open || g.phase == CHOOSE_ROLE || g.phase == R_BID;
        if (!frozen)
        {
            acc += dt * speed;
            while (acc >= TICK)
            {
                acc -= TICK;
                g.sim.tick();
                g.drain();
            }
        }
        if (g.phase == U_BIDDING && !g.sim.bidding())
        {
            // every offer expired: back to the offer screen
            g.msg = "All riders lost interest. Raise your offer (UP) and try again.";
            g.phase = U_OFFER;
        }
        float t = acc / TICK; // how far through the current tick (for smooth driving)

        // ----- car positions: slide along the road the rider is on; a long road takes several ticks -----
        Vector2 carP[64];
        for (int i = 0; i < g.sim.riders.size() && i < 64; i++)
        {
            Rider *r = g.sim.riders[i];
            Vector2 a = V(gr.x(r->prevNode), gr.y(r->prevNode)), b = V(gr.x(r->node), gr.y(r->node));
            float frac = 1;
            if (r->prevNode != r->node)
            {
                frac = ((g.sim.clock - r->moveStart) + t) / (float)r->moveTicks;
                if (frac > 1)
                    frac = 1;
                if (frac < 0)
                    frac = 0;
                heading[i] = atan2f(b.y - a.y, b.x - a.x) * RAD2DEG;
            }
            carP[i] = V(a.x + (b.x - a.x) * frac, a.y + (b.y - a.y) * frac);
        }
        Rider *myRiderPtr = g.role == RIDER_ROLE ? g.meRider : (g.myRide ? g.myRide->rider : 0);
        int myIdx = -1;
        for (int i = 0; i < g.sim.riders.size(); i++)
            if (g.sim.riders[i] == myRiderPtr)
                myIdx = i;
        if (follow && myIdx >= 0 && !tools.open)
            cam.target = carP[myIdx];

        // which customer / destination to highlight
        int hiPickup = -1, hiDest = -1;
        if (g.role == USER_ROLE)
        {
            if (g.pickup >= 0 && g.phase != U_DONE && (g.phase != U_RIDING || g.myRide->phase == 0))
                hiPickup = g.pickup;
            if (g.dest >= 0 && g.phase != U_DONE)
                hiDest = g.dest;
        }
        else if (g.role == RIDER_ROLE)
        {
            if (g.phase == R_BID && g.selReq)
            {
                hiPickup = g.selReq->pickup;
                hiDest = g.selReq->dest;
            }
            else if (g.phase == R_DRIVING && g.myRide)
            {
                if (g.myRide->phase == 0)
                    hiPickup = g.myRide->user->pickup;
                hiDest = g.myRide->user->dest;
            }
        }

        // =============== DRAW ===============
        BeginDrawing();
        ClearBackground(C(236, 232, 224));
        BeginScissorMode(0, 0, MAP_W, WIN_H);
        BeginMode2D(cam);
        if (hasMap)
            DrawTextureEx(mapTex, V(0, 0), 0, mapScale, WHITE);
        for (int pass = 0; pass < 2; pass++) // roads: outline first, then fill
            for (int i = 0; i < gr.size(); i++)
                for (int k = 0; k < gr.degree(i); k++)
                {
                    int j = gr.edge(i, k).to;
                    if (j < i)
                        continue;
                    bool closed = gr.edge(i, k).blocked;
                    Vector2 a = V(gr.x(i), gr.y(i)), b = V(gr.x(j), gr.y(j));
                    if (pass == 0)
                        DrawLineEx(a, b, 13, closed ? C(170, 60, 60) : C(176, 180, 188));
                    else
                        DrawLineEx(a, b, 9, closed ? C(238, 100, 100) : C(255, 255, 255));
                }
        for (int i = 0; i < gr.size(); i++) // closed-road signs
            for (int k = 0; k < gr.degree(i); k++)
                if (gr.edge(i, k).to > i && gr.edge(i, k).blocked)
                {
                    int j = gr.edge(i, k).to;
                    Vector2 m = V((gr.x(i) + gr.x(j)) / 2, (gr.y(i) + gr.y(j)) / 2);
                    DrawCircleV(m, 12, C(210, 40, 40));
                    DrawRectangle((int)m.x - 8, (int)m.y - 2, 16, 4, WHITE);
                }
        for (int i = 0; i < gr.size(); i++)
        {
            // areas
            DrawCircleV(V(gr.x(i), gr.y(i)), 6, C(120, 126, 138));
            drawLabel(gr.name(i).c_str(), (int)gr.x(i) + 10, (int)gr.y(i) + 2, 14, C(50, 54, 64));
        }
        if (tools.open)
            tools.drawMap(gr);
        if (g.role == ADMIN_ROLE && !tools.open) // dispatcher: every ride's route and destination
            for (int i = 0; i < g.sim.active.size(); i++)
            {
                Ride *ride = g.sim.active[i];
                drawPath(gr, ride->path, 5, Fade(C(40, 105, 240), 0.55f));
                drawPin(V(gr.x(ride->user->dest), gr.y(ride->user->dest)), 0.6f);
            }

        if (g.myRide && g.myRide->phase < 2 && myIdx >= 0 && !tools.open)
        {
            // the route, like Google Maps
            Ride *ride = g.myRide;
            bool toPickup = ride->phase == 0;
            for (int pass = 0; pass < 2; pass++)
            {
                Vector2 a = carP[myIdx];
                for (int i = 0; i < ride->path.size(); i++)
                {
                    int n = ride->path[i];
                    Vector2 b = V(gr.x(n), gr.y(n));
                    bool jam = i > 0 && gr.isBlocked(ride->path[i - 1], n);
                    Color col = jam ? C(255, 150, 0) : (toPickup ? C(130, 160, 215) : C(40, 105, 240));
                    if (pass == 0)
                        DrawLineEx(a, b, 12, WHITE);
                    else
                        DrawLineEx(a, b, 8, col);
                    a = b;
                }
            }
        }

        if (hiDest >= 0)
            drawPin(V(gr.x(hiDest), gr.y(hiDest)), 1.0f);                 // destination pin
        bool seeCustomers = g.role == RIDER_ROLE || g.role == ADMIN_ROLE; // a user never sees other customers
        for (int i = 0; seeCustomers && i < g.sim.open.size(); i++)
        {
            // customers still waiting for a driver (blue blips)
            User *u = g.sim.open[i];
            if (u->pickup == hiPickup)
                continue;
            drawPerson(V(gr.x(u->pickup) + (i % 3 - 1) * 12.0f, gr.y(u->pickup)), C(80, 110, 190), (float)now + i * 0.3f);
        }
        for (int i = 0; i < g.sim.active.size(); i++)
        {
            Ride *ride = g.sim.active[i];
            if (g.role == ADMIN_ROLE && ride->phase == 0) // only the dispatcher sees customers who already have a driver
                drawPerson(V(gr.x(ride->user->pickup), gr.y(ride->user->pickup)), C(230, 150, 30), (float)now + i);
        }
        if (hiPickup >= 0)
            drawPerson(V(gr.x(hiPickup), gr.y(hiPickup)), C(24, 160, 88), (float)now); // YOUR customer (green)

        for (int i = 0; i < g.sim.riders.size() && i < 64; i++)
        {
            // other cars (grey = free, blue = busy)
            if (i == myIdx || g.role == RIDER_ROLE) // a rider sees no other riders
                continue;
            drawCar(carP[i], heading[i], g.sim.riders[i]->state == IDLE ? C(110, 118, 132) : C(70, 120, 200), 0.85f);
            if (g.role == ADMIN_ROLE)
            {
                // dispatcher: name tag on every car
                const string &full = g.sim.riders[i]->name;
                string nm = full.substr(0, full.find(' '));
                drawLabel(nm.c_str(), (int)carP[i].x - measureText(nm.c_str(), 13) / 2, (int)carP[i].y - 30 - (i % 2) * 14, 13, C(30, 34, 46)); // stagger tags so cars on one area stay readable
            }
        }
        if (myIdx >= 0)
            drawCar(carP[myIdx], heading[myIdx], C(222, 38, 38), 1.2f); // YOUR red car
        EndMode2D();
        EndScissorMode();

        if (g.phase == CHOOSE_ROLE)
        {
            // role selection screen
            DrawRectangle(0, 0, MAP_W, WIN_H, Fade(WHITE, 0.6f));
            int sy = startShift(); // keeps the start screen vertically centred in a taller window
            if (hasLogo)
                DrawTexture(logoBig, MAP_W / 2 - logoBig.width / 2, 145 + sy, WHITE);
            else
                drawText("CHALGORITHM", MAP_W / 2 - measureText("CHALGORITHM", 56) / 2, 200 + sy, 56, C(30, 34, 46));
            drawText("Who are you today?", MAP_W / 2 - measureText("Who are you today?", 22) / 2, 275 + sy, 22, C(100, 104, 116));
            Rectangle bu = R(MAP_W / 2 - 330, 330 + sy, 200, 90), br = R(MAP_W / 2 - 100, 330 + sy, 200, 90), bd = R(MAP_W / 2 + 130, 330 + sy, 200, 90);
            DrawRectangleRounded(bu, 0.2f, 8, CheckCollisionPointRec(mouse, bu) ? C(60, 130, 240) : C(40, 105, 220));
            DrawRectangleRounded(br, 0.2f, 8, CheckCollisionPointRec(mouse, br) ? C(240, 90, 90) : C(214, 50, 50));
            DrawRectangleRounded(bd, 0.2f, 8, CheckCollisionPointRec(mouse, bd) ? C(70, 80, 100) : C(45, 52, 68));
            drawText("USER (U)", (int)bu.x + 26, (int)bu.y + 16, 24, WHITE);
            drawText("I need a ride", (int)bu.x + 26, (int)bu.y + 52, 15, WHITE);
            drawText("RIDER (R)", (int)br.x + 26, (int)br.y + 16, 24, WHITE);
            drawText("I'm the driver", (int)br.x + 26, (int)br.y + 52, 15, WHITE);
            drawText("DISPATCHER (D)", (int)bd.x + 14, (int)bd.y + 16, 20, WHITE);
            drawText("I watch everything", (int)bd.x + 14, (int)bd.y + 52, 15, WHITE);
            {
                // how the road distances were obtained
                DynamicArray<string> nl = wrap(g.sim.graph.loadNote(), 14, 640);
                int w = 0;
                for (int i = 0; i < nl.size(); i++)
                    if (measureText(nl[i].c_str(), 14) > w)
                        w = measureText(nl[i].c_str(), 14);
                DrawRectangleRounded(R((float)(MAP_W / 2 - w / 2 - 14), (float)(446 + sy), (float)(w + 28), (float)(nl.size() * 18 + 12)), 0.4f, 6, Fade(WHITE, 0.92f));
                for (int i = 0; i < nl.size(); i++)
                    drawText(nl[i].c_str(), MAP_W / 2 - measureText(nl[i].c_str(), 14) / 2, 452 + sy + i * 18, 14, C(70, 74, 84));
            }
        }
        tools.drawCharts();

        // =============== PANEL ===============
        // Text sizes go through pf() (a little larger than before) and every y position comes from the constants in
        // gui_draw.h, so rows never overlap and the mouse hit-tests above use exactly the same geometry.
        int px = MAP_W + 16;
        DrawRectangle(MAP_W, 0, WIN_W - MAP_W, WIN_H, C(250, 250, 252));
        DrawLine(MAP_W, 0, MAP_W, WIN_H, C(205, 208, 215));
        if (hasLogo)
            DrawTexture(logoSmall, px, 6, WHITE);
        else
            drawText("CHALGORITHM", px, 10, pf(26), C(30, 34, 46));
        drawText(TextFormat("t=%d   speed x%d%s", g.sim.clock, speed, paused ? "   PAUSED" : (frozen ? "   (time stopped)" : "")), px, 42, pf(14), C(100, 104, 116));
        int nClosed = 0, ca = -1, cb = -1; // closed roads (random or manual): first one by name + a count
        for (int i = 0; i < gr.size(); i++)
            for (int k = 0; k < gr.degree(i); k++)
                if (gr.edge(i, k).to > i && gr.edge(i, k).blocked)
                {
                    if (nClosed++ == 0)
                    {
                        ca = i;
                        cb = gr.edge(i, k).to;
                    }
                }
        if (nClosed == 0)
            textMax2("All roads open", px, 64, pf(14), C(24, 140, 80));
        else
            textMax2(nClosed == 1 ? "CLOSED: " + gr.name(ca) + " - " + gr.name(cb)
                                  : "CLOSED: " + gr.name(ca) + " - " + gr.name(cb) + " (+" + to_string(nClosed - 1) + " more)",
                     px, 64, pf(14), C(200, 40, 40));

        int y = PANEL_TOP;
        Color dark = C(30, 34, 46), grey = C(100, 104, 116), blue = C(24, 100, 220);
        if (tools.open)
            tools.drawPanel(gr, px);
        else
        {
            if (g.role == RIDER_ROLE && g.meRider && g.phase != R_START)
            {
                drawText(TextFormat("DRIVER  rating %.2f | %d %s", g.meRider->rating, g.rides, g.rides == 1 ? "ride" : "rides"), px, PANEL_TOP, pf(13), C(24, 140, 80));
                drawText(TextFormat("Earned Rs %d", (int)g.earned), px, PANEL_TOP + 20, pf(13), C(24, 140, 80));
                y = RIDER_TOP;
            }
            if (g.phase == D_WATCH)
            {
                int nr = g.sim.riders.size();
                int idle = g.sim.idleRiders();

                drawText("DISPATCHER - view only", px, PANEL_TOP, pf(15), C(24, 140, 80));
                drawText(TextFormat("Riders %d (free %d, busy %d)", nr, idle, nr - idle), px, PANEL_TOP + 24, pf(13), dark);
                drawText(TextFormat("Waiting customers %d | rides running %d", g.sim.open.size(), g.sim.active.size()), px, PANEL_TOP + 44, pf(13), dark);
                drawText(TextFormat("Rides completed %d | closed roads %d", g.sim.history.size(), nClosed), px, PANEL_TOP + 64, pf(13), dark);

                // Driver list dimensionsa
                int driverTop = PANEL_TOP + 92;
                int driverViewHeight = 176;
                int driverRowHeight = 22;

                int driverTotalHeight = nr * driverRowHeight;
                int maxDriverScroll = max(0, driverTotalHeight - driverViewHeight);

                // Scroll when the mouse is over the driver list
                if (mouse.x >= MAP_W &&
                    mouse.y >= driverTop &&
                    mouse.y <= driverTop + driverViewHeight)
                {

                    driverScroll -= (int)(wheel * 40);
                }

                // Clamp scroll position
                if (driverScroll < 0)
                    driverScroll = 0;
                if (driverScroll > maxDriverScroll)
                    driverScroll = maxDriverScroll;

                // Draw driver rows inside a clipped viewport
                BeginScissorMode(px, driverTop,
                                 WIN_W - px - 16, driverViewHeight);

                for (int i = 0; i < nr; i++)
                {
                    Rider *r = g.sim.riders[i];

                    int rowY = driverTop + i * driverRowHeight - driverScroll;

                    const char *status =
                        r->state == IDLE ? "free" : (r->state == TO_PICKUP ? "to pickup" : "on trip");

                    string firstName = r->name.substr(0, r->name.find(' '));

                    drawText(fitText(firstName, pf(14), 72).c_str(), px, rowY, pf(14), dark);

                    drawText(status, px + 78, rowY, pf(14),
                             r->state == IDLE ? grey : blue);

                    drawText(TextFormat("%.1f", r->rating),
                             px + 152, rowY, pf(14), grey);

                    drawText(fitText(gr.name(r->node), pf(14), 100).c_str(),
                             px + 184, rowY, pf(14), grey);
                }

                EndScissorMode();

                // Scrollbar
                if (maxDriverScroll > 0)
                {
                    float barHeight =
                        (float)driverViewHeight * driverViewHeight /
                        driverTotalHeight;

                    float barY = driverTop +
                                 ((float)driverScroll / maxDriverScroll) * (driverViewHeight - barHeight);

                    DrawRectangle(WIN_W - 12, driverTop, 3,
                                  driverViewHeight, C(225, 228, 235));

                    DrawRectangle(WIN_W - 12, (int)barY, 3,
                                  (int)barHeight, C(120, 130, 150));
                }

                if (nr == 0)
                {
                    drawText("No drivers available.", px, driverTop, pf(11), grey);
                }

                text("Scroll to view drivers | Click a road to close/reopen it.", px, driverTop + driverViewHeight + 6, pf(12), grey);
            }
            else if (g.phase == U_PICKUP)
                y = text("Click your PICKUP area on the map.", px, y, pf(18), dark);
            else if (g.phase == U_DEST)
                y = text("Pickup: " + gr.name(g.pickup) + ". Now click your DESTINATION.", px, y, pf(18), dark);
            else if (g.phase == U_OFFER)
            {
                y = text(gr.name(g.pickup) + " -> " + gr.name(g.dest), px, y, pf(18), dark);
                y = text(string(TextFormat("%.1f km | surge x%.2f | suggested Rs %d", g.sim.distance(g.pickup, g.dest), g.sim.surge(g.pickup), (int)g.suggested)), px, y, pf(14), grey);
                y = text(string(TextFormat("Your offer: Rs %d   (UP / DOWN)", (int)g.offer)), px, y + 6, pf(18), blue);
                y = text("Press ENTER to request a ride.", px, y + 6, pf(14), dark);
            }
			else if (g.phase == U_BIDDING)
			{
                int rowsTop = offerRowsTop(), rowsMax = offerRowsMax();
                int nOff = g.sim.offers.size() < rowsMax ? g.sim.offers.size() : rowsMax;
                text("Offers arrive live - click one (or press 1-9) before it expires:", px, PANEL_TOP, pf(14), dark);
                for (int i = 0; i < nOff; i++)
                {                          // oldest offer first: it is also the first to leave
                    const Offer& o = g.sim.offers.at(i);
                    Rider* r = g.sim.riders[o.bid.riderIdx];
                    float left = (o.leaveTick - g.sim.clock) - t;         // ticks until this rider loses interest
                    if (left < 0) 
						left = 0;
                    float frac = left / OFFER_LIFE;
                    Color tc = frac > 0.5f ? C(30, 160, 90) : (frac > 0.25f ? C(240, 150, 40) : C(214, 50, 50));
                    int ry = rowsTop + i * OFFER_ROW_H;
                    Rectangle row = R(MAP_W + 12, ry, 296, 40);
                    DrawRectangleRounded(row, 0.2f, 6, CheckCollisionPointRec(mouse, row) ? C(214, 228, 255) : C(238, 240, 246));
                    drawText(TextFormat("%d. %s", i + 1, r->name.c_str()), px, ry + 4, pf(14), dark);
                    string secs = string(TextFormat("%.0fs", left * TICK / speed));
                    drawText(secs.c_str(), MAP_W + 12 + 296 - 8 - measureText(secs.c_str(), pf(14)), ry + 4, pf(14), tc);
                    drawText(fitText(string(TextFormat("Rs %d  |  %.1f km away  |  rating %.1f", (int)o.bid.price, o.bid.pickupDist, r->rating)), pf(12), 282).c_str(), px, ry + 22, pf(12), grey);
                    DrawRectangle(MAP_W + 12 + 6, ry + 37, (int)((296 - 12) * frac), 2, tc);   // countdown bar
                }
                y = rowsTop + nOff * OFFER_ROW_H + 2;
                if (g.sim.offers.size() > nOff) 
					y = text(string(TextFormat("+%d more offers waiting", g.sim.offers.size() - nOff)), px, y, pf(12), grey);
                if (g.sim.offers.size() == 0) 
					y = text("Waiting for riders to answer...", px, y, pf(13), grey);
                else if (g.sim.incoming.size() > 0) 
					y = text(string(TextFormat("%d more riders are still deciding...", g.sim.incoming.size())), px, y, pf(12), grey);
                y = text("BACKSPACE = cancel request", px, y + 2, pf(12), grey);
            }
            else if (g.phase == U_RIDING && g.myRide)
            {
                y = text("Driver: " + g.myRide->rider->name, px, y, pf(18), dark);
                y = text(g.myRide->phase == 0 ? "Your driver is coming to pick you up." : "On the way to your destination.", px, y, pf(14), blue);
                y = text(string(TextFormat("Fare Rs %d | roads left: %d%s", (int)g.myRide->fare, g.myRide->path.size() - 1 + (g.myRide->rider->prevNode != g.myRide->rider->node ? 1 : 0),
                    g.myRide->wait > 0 ? " | STUCK IN TRAFFIC" : "")), px, y + 4, pf(14), g.myRide->wait > 0 ? C(220, 120, 0) : grey);
            }
            else if (g.phase == U_DONE && g.myRide)
            {
                y = text("Ride complete!", px, y, pf(22), C(24, 140, 80));
                y = text(string(TextFormat("Fare Rs %d | drove %.1f km (best %.1f km) | traffic delay %d", (int)g.myRide->fare,
                    g.myRide->travelled, g.myRide->optimal, g.myRide->delay)), px, y + 4, pf(14), grey);
                y = text("Press ENTER to book another ride.", px, y + 6, pf(14), dark);
            }
            else if (g.phase == R_START) 
				y = text("Click the area where your shift starts.", px, y, pf(18), dark);
            else if (g.phase == R_BROWSE)
            {
                int rowsTop = waitRowsTop(), rowsMax = waitRowsMax();
                int nW = g.sim.open.size() < rowsMax ? g.sim.open.size() : rowsMax;
                if (g.sim.open.size() == 0) 
					y = text("No customers waiting yet. New ones appear every few seconds.", px, y, pf(14), grey);
                else 
					text("Customers waiting - click one (or its blip):", px, y, pf(14), dark);
                for (int i = 0; i < nW; i++)
                {
                    User* u = g.sim.open[i];
                    int ry = rowsTop + i * WAIT_ROW_H;
                    Rectangle row = R(MAP_W + 12, ry, 296, 60);
                    DrawRectangleRounded(row, 0.2f, 6, CheckCollisionPointRec(mouse, row) ? C(214, 228, 255) : C(238, 240, 246));
                    drawText(fitText(string(TextFormat("%s   offers Rs %d", u->name.c_str(), (int)u->offer)), pf(14), 282).c_str(), px, ry + 5, pf(14), dark);
                    textMax2(string(TextFormat("%s > %s | %.1f km away | trip %.1f km", gr.name(u->pickup).c_str(), gr.name(u->dest).c_str(),
                    	g.sim.distance(g.meRider->node, u->pickup), g.sim.distance(u->pickup, u->dest))), px, ry + 25, pf(11), grey, 280);
                }
                if (g.sim.open.size() > 0)
                {
                    y = rowsTop + nW * WAIT_ROW_H + 2;
                    if (g.sim.open.size() > nW) 
						y = text(string(TextFormat("+%d more waiting - click their blip on the map", g.sim.open.size() - nW)), px, y, pf(12), grey);
                }
            }
            else if (g.phase == R_BID && g.selReq)
            {
                y = text(g.selReq->name + ": " + gr.name(g.selReq->pickup) + " -> " + gr.name(g.selReq->dest), px, y, pf(16), dark);
                y = text(string(TextFormat("Their offer: Rs %d | your cost at Rs %d/km: Rs %d", (int)g.selReq->offer, (int)g.meRider->perKm, (int)g.bidCost)), px, y + 2, pf(14), grey);
                y = text(string(TextFormat("Your bid: Rs %d   (UP / DOWN)", (int)g.bidPrice)), px, y + 8, pf(18), blue);
                y = text("ENTER = send bid | BACKSPACE = back. Bid too high and they refuse; too low and you lose money.", px, y + 6, pf(13), dark);
            }
            else if (g.phase == R_DRIVING && g.myRide)
            {
                y = text(g.myRide->phase == 0 ? "Driving to your customer." : "Customer on board - driving to the destination.", px, y, pf(16), blue);
                y = text(string(TextFormat("Fare Rs %d | roads left: %d%s", (int)g.myRide->fare, g.myRide->path.size() - 1 + (g.meRider->prevNode != g.meRider->node ? 1 : 0),
                    g.myRide->wait > 0 ? " | STUCK IN TRAFFIC" : "")), px, y + 4, pf(14), g.myRide->wait > 0 ? C(220, 120, 0) : grey);
            }
            else if (g.phase == R_DONE && g.myRide)
            {
                y = text("Customer dropped off!", px, y, pf(22), C(24, 140, 80));
                y = text(string(TextFormat("You earned Rs %d | drove %.1f km (best %.1f km) | traffic delay %d", (int)g.myRide->fare,
                    g.myRide->travelled, g.myRide->optimal, g.myRide->delay)), px, y + 4, pf(14), grey);
                y = text("Press ENTER to look for the next customer.", px, y + 6, pf(14), dark);
            }
            if (g.msg.size()) 
				text(g.msg, px, y + 6, pf(14), C(200, 40, 40));
        }

        int logTop = g.role == ADMIN_ROLE ? LOG_TOP_ADMIN : LOG_TOP_USER;

if (showBoard && g.role == ADMIN_ROLE)
{
    DynamicArray<LbEntry> top = g.sim.board.topK(5);
    drawMedium("LEADERBOARD (AVL tree)", px, logTop - 26, pf(15), dark);
    for (int i = 0; i < top.size(); i++)
        drawText(TextFormat("#%d  %s   %.2f",
                 i + 1, top[i].name.c_str(), top[i].rating),
                 px, logTop + 4 + i * 26, pf(16), C(60, 64, 76));
}
else
{
    DrawLine(px, logTop - 32, WIN_W - 16, logTop - 32, C(225, 228, 235));
    drawMedium(g.role == ADMIN_ROLE ? "ALL EVENTS" :
               (g.role == RIDER_ROLE ? "YOUR SHIFT" : "YOUR TRIP"),
               px, logTop - 26, pf(12), C(120, 124, 136));
    drawText("newest first",
             WIN_W - 16 - measureText("newest first", pf(11)),
             logTop - 25, pf(11), C(150, 154, 166));

    // Calculate total height of visible log entries
    int totalHeight = 0;
    for (int i = g.lines.size() - 1; i >= 0; i--)
    {
        const LogEntry& e = g.lines[i];
        if (!g.seesLog(e)) 
			continue;
        DynamicArray<string> head = wrap(e.text, pf(13), 232);
        DynamicArray<string> det = wrap(e.detail, pf(11), 240);
        if (e.detail.empty()) 
			det.clear();
        totalHeight += head.size() * (pf(13) + 3) + det.size() * (pf(11) + 2) + 10;
    }

    // Scrollable viewport dimensions
    int viewHeight = WIN_H - logTop - 46;
    int maxScroll = totalHeight - viewHeight;
    if (maxScroll < 0) 
		maxScroll = 0;

    // Mouse wheel only scrolls when hovering over the log panel
    if (mouse.x >= MAP_W && mouse.y >= logTop && mouse.y <= WIN_H - 46)
    {
	    logScroll -= (int)(wheel * 40);
    }

    // Keep scroll position within valid limits
    if (logScroll < 0) 
		logScroll = 0;
    if (logScroll > maxScroll) 
		logScroll = maxScroll;

    // Clip drawing to the log panel
    BeginScissorMode(px, logTop, WIN_W - px - 16, viewHeight);

    int ey = logTop - logScroll;
    bool newest = true;

    for (int i = g.lines.size() - 1; i >= 0; i--)
    {
        const LogEntry& e = g.lines[i];
        if (!g.seesLog(e)) 
			continue;

        DynamicArray<string> head = wrap(e.text, pf(13), 232);
        DynamicArray<string> det = wrap(e.detail, pf(11), 240);

        if (e.detail.empty()) 
			det.clear();

        int h = head.size() * (pf(13) + 3) + det.size() * (pf(11) + 2) + 10;
        if (ey + h > logTop && ey < logTop + viewHeight)
        {
            Color kc = logColor(e.kind);
            if (newest)
                DrawRectangleRounded(
                    R((float)(px - 6), (float)(ey - 3),
                    (float)(WIN_W - px - 4), (float)(h - 2)),
                    0.15f, 6, C(238, 242, 250));

            DrawRectangle(px, ey, 3, h - 8, kc);
            drawText(TextFormat("t%d", e.tick),
                     px + 9, ey + 2, pf(11), C(150, 154, 166));
            for (int k = 0; k < head.size(); k++)
                drawMedium(head[k].c_str(),
                           px + 46, ey + k * (pf(13) + 3), pf(13),
                           newest ? dark : C(70, 74, 86));

            for (int k = 0; k < det.size(); k++)
                drawText(det[k].c_str(),
                         px + 46,
                         ey + head.size() * (pf(13) + 3) + k * (pf(11) + 2),
                         pf(11), C(120, 124, 136));
        }
        ey += h;
        newest = false;
    }
    EndScissorMode();

    // Scroll position indicator
    if (maxScroll > 0)
    {
        float barHeight = (float)viewHeight * viewHeight / totalHeight;
        float barY = logTop +
                     ((float)logScroll / maxScroll) *
                     (viewHeight - barHeight);

        DrawRectangle(WIN_W - 12, logTop, 3, viewHeight,
                      C(225, 228, 235));

        DrawRectangle(WIN_W - 12, (int)barY, 3,
                      (int)barHeight, C(120, 130, 150));
    }
}
        drawText(g.role == ADMIN_ROLE ? "P pause | +/- speed | TAB leaderboard | T tools" : "P pause | +/- speed | F follow | T tools", px, WIN_H - 34, 12, C(130, 134, 146));
        drawText("wheel zoom | right-drag pan | BACKSPACE back", px, WIN_H - 18, 12, C(130, 134, 146));
        EndDrawing();

        if (demo)
        {                                                              // screenshots for checking the look
            static const double atNormal[] = {3.2, 6.0, 8.5, 11.0, 14.5}, atTools[] = {1.0, 2.0, 3.0, 4.0, 5.0};
            const double* at = dm == "-tools" ? atTools : atNormal;
            static bool roleShot = false, bidShot = false;
            if (!roleShot && g.phase == CHOOSE_ROLE && now > 0.2)
            {
                TakeScreenshot("role.png"); roleShot = true;
            }
            
            if (!bidShot && g.phase == R_BID)
            {
                TakeScreenshot("bid.png"); bidShot = true;
            }

            if (shots < 5 && now > at[shots])
            {
                TakeScreenshot(TextFormat("shot%d.png", shots + 1)); shots++;
            }

            if (shots >= 5) 
				break;
        }
    }
    if (hasMap) 
		UnloadTexture(mapTex);
    if (hasLogo)
    {
        UnloadTexture(logoBig); UnloadTexture(logoSmall);
    }
    unloadUiFonts();
    CloseWindow();
    return 0;
}
