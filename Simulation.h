#pragma once
#include <string>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include "DynamicArray.h"
#include "Queue.h"
#include "MinHeap.h"
#include "HashTable.h"
#include "AVL.h"
#include "Graph.h"
#include "Pathfinding.h"
#include "Agent.h"
using namespace std;

const float REF_ROAD_KM = 4.05f;            // used for car speed in crossing roads per tick, 4.05 was the average road size of the sample city built in graph.h
const float RATE_PER_KM = 40.0f;            // suggested fare in Rs per km, before surge
const float MINIMUM_FARE = 300.0f;          // a minimum fare for even the smallest rides

struct Bid {
    int riderIdx;
    float price;
    float pickupDist;
    float rating;
    float score;                                   // lower = better
    bool operator<(const Bid& o) const 
    { 
        return score < o.score; 
    }
};

const int OFFER_LIFE = 7;        // ticks an offer stays alive for, FIFO makes it leave after a few seconds
const int OFFER_SPREAD = 5;      // offers arrive at a random tick between 1 and 5 after the request

struct Offer {
    Bid bid;
    int arriveTick;              // when the rider placed the offer
    int leaveTick;               // when the rider loses interest if he was not picked
};

struct Ride {
    Rider* rider;
    User* user;
    bool ownsUser;             // true for NPC users (the simulation deletes them)
    float fare;
    int phase;                 // 0 = heading to pickup, 1 = on trip, 2 = finished
    DynamicArray<int> path;
    int pos;
    float travelled, optimal;
    int detours;
    int wait, delay;           // ticks left to sit in a traffic jam / total ticks lost to traffic
    int transit;               // ticks left before the rider reaches the end of the current road
    bool crossing;             // true while waiting to cross a closed (jammed) road
    Ride() : rider(0), user(0), ownsUser(false), fare(0), phase(0), pos(0), travelled(0), optimal(0),
             detours(0), wait(0), delay(0), transit(0), crossing(false) {} //default constructor
};

struct Closure 
{ 
    int a, b, until;      // a randomly closed edge and the tick when it reopens
};

enum LogKind { LOG_REQUEST, LOG_MATCH, LOG_PICKUP, LOG_DROPOFF, LOG_DETOUR, LOG_TRAFFIC, LOG_REROUTE,
               LOG_CLOSED, LOG_REOPENED, LOG_CANCEL, LOG_SYSTEM };

struct LogEntry {                          // one event: a short headline plus an optional detail line
    int tick;
    LogKind kind;                          // decides the colour in the window and the tag in the console
    string text;                           // headline, e.g. "Bilal dropped Zara off at Airport"
    string detail;                         // second line, e.g. "Rs 1428, 18.2 km (best 18.2), rating 4.42"
    int riderId, userId;                   // who it is about; 0 = nobody (world events such as accidents)
};

static inline const char* logTag(LogKind k) 
{
    static const char* tags[] = {"REQUEST", "MATCH", "PICKUP", "DROP-OFF", "DETOUR", "TRAFFIC", "REROUTE", "ROAD", "ROAD", "CANCEL", "INFO"};
    return tags[k];
}
static inline string num1(float v) 
{ 
    char b[32]; 
    snprintf(b, sizeof(b), "%.1f", v); 
    return b; 
}

static inline string num2(float v) 
{ char b[32]; 
    snprintf(b, sizeof(b), "%.2f", v); 
    return b; 
}
static inline string rupees(float v) 
{ 
    return "Rs " + to_string((int)v); 
}

class Simulation {
public:
    Graph graph;
    DynamicArray<Rider*> riders;
    DynamicArray<User*> open;           // pending requests from NPC users
    DynamicArray<Ride*> active;
    DynamicArray<Ride*> history;        // finished rides
    DynamicArray<LogEntry> log;
    DynamicArray<Closure> closures;
    HashTable<Rider*> riderIndex;
    Queue<Offer> offers;
    DynamicArray<Offer> incoming;       // offers still on their way (each has a random arrival tick)
    User* pendingUser;
    AVL board;                          // rating leaderboard
    float demand[64];                   // recent requests per area (decays), drives surge pricing
    int clock;
    int nextId;
    float scaleK;                       // this map's size relative to the sample city
    float kmPerTick;                    // riders drive at a constant speed: a road of w km takes about w / kmPerTick ticks
    string loadError;                   // why the last load() failed

    Simulation() : pendingUser(0), clock(0), nextId(1), scaleK(1), kmPerTick(3) 
    {
        if (!graph.loadFromFiles("nodes.txt", "edges.txt"))     //custom map not found, use default from graph.h
            graph.buildDefaultCity();

        scaleK = graph.averageRoadKm() / REF_ROAD_KM;
        if (scaleK < 0.05f) 
            scaleK = 0.05f;

        kmPerTick = 3.0f * scaleK;
        for (int i = 0; i < 64; i++) 
            demand[i] = 0;

        srand(42);                                   // fixed seed = reproducible bugs


        //LIST OF RIDER NAMES
        static const char* names[] = {"Ali", "Bilal", "Chand", "Danish", "Eman", "Faraz", "Gul", "Hamza", "Ahmed", "Mekaal", "Usaim", "Maryam", 
                                      "Shaheer", "Haziq", "Memoona", "Haniya", "Uzair", "Talha", "Mubashir", "Mustafa", "Parshant", "Naresh", "Samad", 
                                      "Miss Izzah Salam", "Owais", "Rameez", "Amaan", "Saud", "Mustafeez", "Shahwaiz"};
        for (int i = 0; i < 20; i++) //the number of drivers we have chosen to keep for the city we will make
        {
            Rider* r = new Rider(nextId++, names[i], rand() % graph.size(), true);
            r->rating = 3.8f + (rand() % 13) / 10.0f;  // 3.8 - 5.0
            r->ridesDone = 5;               // so one ride doesn't wipe the rating
            r->perKm = 30 + rand() % 20;    //pricing logic
            r->greed = 0.05f + (rand() % 30) / 100.0f;      
            r->detourChance =   (r->rating >= 4.5f) ? 0.0f :
                                (r->rating >= 4.3f) ? 0.02f :
                                (r->rating >= 4.0f) ? 0.06f :
                                0.08f;   // bad riders wander
            r->maxPickup = 12.0f * scaleK;                          // will not drive further than ~3 average roads to a pickup
            registerRider(r);
        }
    }
    ~Simulation()   //destructor
    {
        for (int i = 0; i < riders.size(); i++) 
            delete riders[i];

        for (int i = 0; i < open.size(); i++) 
            delete open[i];

        for (int i = 0; i < active.size(); i++) 
        { 
            if (active[i]->ownsUser) 
                delete active[i]->user; 
                
            delete active[i]; 
        }

        for (int i = 0; i < history.size(); i++) 
        { 
            if (history[i]->ownsUser) 
                delete history[i]->user; 
            
            delete history[i]; 
        }
    }

    void say(LogKind kind, const string& text, const string& detail = "", int riderId = 0, int userId = 0) 
    {
        LogEntry e;
        e.tick = clock; 
        e.kind = kind; 
        e.text = text; 
        e.detail = detail;
        e.riderId = riderId; 
        e.userId = userId;
        log.push_back(e);
    }


    string who(const Agent* a, bool sentenceStart = false) const 
    { 
        return a->isNPC ? a->name : (sentenceStart ? "You" : "you");    //choosing name to address by
    }
    string poss(const Agent* a) const 
    { 
        return a->isNPC ? a->name + "'s" : "your"; 
    }

    void registerRider(Rider* r) 
    {
        riders.push_back(r);

        riderIndex.insert(r->id, r);

        r->lbKey = r->rating;

        LbEntry e; 
        e.rating = r->rating; 
        e.id = r->id; 
        e.name = r->name;
        board.insert(e);
    }
    void refreshBoard(Rider* r)     //setting leaderboard back to their new values
    {
        LbEntry e; 
        e.id = r->id; 
        e.name = r->name;
        e.rating = r->lbKey; 
        board.remove(e);
        r->lbKey = r->rating; 
        e.rating = r->rating; 
        board.insert(e);
    }
    Rider* addPlayerRider(const string& name, int startNode) 
    {
        Rider* r = new Rider(nextId++, name, startNode, false);
        r->rating = 4.8f; 
        r->ridesDone = 5; 
        r->perKm = 38; 
        r->maxPickup = 12.0f * scaleK;
        registerRider(r);
        return r;
    }

    float route(int a, int b, DynamicArray<int>& p, bool* viaTraffic = 0) 
    {
        float d = dijkstra(graph, a, b, p);
        if (viaTraffic) 
            *viaTraffic = false;

        if (d < 0) 
        { 
            d = dijkstra(graph, a, b, p, 0, true); 
            if (viaTraffic) 
                *viaTraffic = true; 
        }
        return d;
    }
    float distance(int a, int b) 
    { 
        DynamicArray<int> p; 
        return route(a, b, p); 
    }

    int idleRiders() 
    {
        int c = 0;
        for (int i = 0; i < riders.size(); i++) 
            if (riders[i]->state == IDLE) 
                c++;

        return c;
    }

    void noteDemand(int node) 
    { 
        demand[node] += 1.0f; 
    }

    float surge(int node) 
    {
        float m = 1.0f + 0.35f * demand[node];
        if (idleRiders() == 0)
             m += 0.3f;

        return m > 2.0f ? 2.0f : m;
    }
    float suggestedFare(int a, int b) 
    {
        float d = distance(a, b);

        if (d < 0) 
            return -1;

        float fare = d * RATE_PER_KM * surge(a);

        return fare < MINIMUM_FARE ? MINIMUM_FARE : fare;
    }

    int collectBids(User* u, MinHeap<Bid>& heap) 
    {
        int count = 0;
        float trip = distance(u->pickup, u->dest);
        if (trip < 0) 
            return 0;

        float sg = surge(u->pickup);

        for (int i = 0; i < riders.size(); i++) 
        {
            Rider* r = riders[i];

            if (!r->isNPC || r->state != IDLE) 
                continue;

            float pd = distance(r->node, u->pickup);

            if (pd < 0 || pd > r->maxPickup) 
                continue;

            float cost = (pd + trip) * r->perKm;

            if (u->offer < cost * 0.7f) 
                continue;                 // fare far too low, rider refuses

            Bid b;
            b.riderIdx = i;
            b.price = cost * (1.0f + r->greed * (rand() % 100) / 100.0f) * sg;
            b.pickupDist = pd;
            b.rating = r->rating;
            b.score = b.price + 5.0f * pd - 15.0f * scaleK * r->rating;
            heap.push(b);
            count++;
        }
        return count;
    }

    bool bidding() const 
    { 
        return pendingUser != 0; 
    }

    bool startPlayerRequest(User* u) 
    {
        stopPlayerRequest();
        MinHeap<Bid> heap;
        if (collectBids(u, heap) == 0) 
            return false;               // nobody wants the job at this price

        noteDemand(u->pickup);

        while (!heap.empty()) 
        {
            Offer o;
            o.bid = heap.pop();
            o.arriveTick = clock + 1 + rand() % OFFER_SPREAD;      // random moment the rider answers
            o.leaveTick = o.arriveTick + OFFER_LIFE;
            incoming.push_back(o);
        }

        pendingUser = u;
        u->state = REQUESTING;
        return true;
    }

    // i = position in the queue (0 = oldest offer).
    Ride* acceptOffer(int i) 
    {
        if (!pendingUser || i < 0 || i >= offers.size()) 
            return 0;

        Offer o = offers.at(i);
        Rider* r = riders[o.bid.riderIdx];

        if (r->state != IDLE) 
        { offers.removeAt(i); return 0; }    // he took another job in the meantime

        User* u = pendingUser;
        stopPlayerRequest();
        return startRide(r, u, o.bid.price);
    }
    void stopPlayerRequest() 
    {
        offers.clear();
        incoming.clear();
        if (pendingUser && pendingUser->state == REQUESTING) 
            pendingUser->state = IDLE;

        pendingUser = 0;
    }

    // Player is a rider
    bool playerBid(Rider* r, User* u, float price) 
    {
        bool ok = price <= u->offer * 1.25f && (rand() % 100) < 85;

        if (!ok) 
            return false;

        for (int i = 0; i < open.size(); i++) 
            if (open[i] == u) 
            { 
                open.removeAt(i); 
                break; 
            }

        startRide(r, u, price);
        return true;
    }

};