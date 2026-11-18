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
    
	Ride* startRide(Rider* r, User* u, float fare)
	{
	    Ride* ride = new Ride();
	    ride->rider = r;
	    ride->user = u;
	    ride->fare = fare;
	    ride->ownsUser = u->isNPC;
	    ride->optimal = distance(r->node, u->pickup) + distance(u->pickup, u->dest);
	    r->state = TO_PICKUP;
	    u->state = TO_PICKUP;
	
	    route(r->node, u->pickup, ride->path);
	    ride->pos = 0;
	    active.push_back(ride);
	
	    //outputs information to the log
	    say(LOG_MATCH, who(r, true) + " accepted " + poss(u) + " request", rupees(fare) + ", heading to " + graph.name(u->pickup), r->id, u->id);
	    arrivalCheck(ride);
	    return ride;
	}
	
	void spawnNpcUser()
	{
	    //initializes customer names
	    static const char* names[] = {
	        "Zara", "Yusuf", "Nida", "Omar", "Mina", "Kamran", "Sana", "Tariq",
	        "Hira", "Bilqis", "Imran", "Farah", "Usman", "Laiba", "Waqar",
	        "Rabia", "Junaid", "Anum", "Saad", "Maham", "Arham", "Dua", "Zain",
	        "Ayesha", "Rayan", "Iqra", "Fahad", "Mehwish", "Huzaifa", "Alina"
	    };
	
	    string nm;
	
	    // avoid two customers with the same name at the same time
	    for (int tries = 0; tries < 30; tries++)
	    {
	        nm = names[rand() % 20];
	        bool taken = false;
	
	        for (int i = 0; i < open.size(); i++)
	        {
	            if (open[i]->name == nm)
	            {
	                taken = true;
	            }
	        }
	
	        for (int i = 0; i < active.size(); i++)
	        {
	            if (active[i]->user->name == nm)
	            {
	                taken = true;
	            }
	        }
	
	        if (!taken)
	        {
	            break;
	        }
	    }
	
	    User* u = new User(nextId++, nm, rand() % graph.size(), true);
	    u->pickup = u->node;
	
	    do
	    {
	        u->dest = rand() % graph.size();
	    }
	    while (u->dest == u->pickup);
	
	    noteDemand(u->pickup);
	
	    float f = suggestedFare(u->pickup, u->dest);
	
	    // unreachable (roads blocked)
	    if (f < 0)
	    {
	        delete u;
	        return;
	    }
	
	    u->offer = f * (0.85f + (rand() % 30) / 100.0f);
	    u->patience = 12 + rand() % 8;
	    u->state = REQUESTING;
	    open.push_back(u);
	
	    //outputs information to the log
	    say(LOG_REQUEST, u->name + " wants a ride", graph.name(u->pickup) + " to " + graph.name(u->dest) + ", offers " + rupees(u->offer), 0, u->id);
	}
	
	// admin: close / reopen a road. Active rides replan automatically on their next step
	bool blockRoad(int a, int b, bool block)
	{
	    if (graph.edgeWeight(a, b) < 0)
	    {
	        return false;
	    }
	
	    graph.setBlocked(a, b, block);
	
	    //outputs information to the log
	    say(block ? LOG_CLOSED : LOG_REOPENED, block ? "Road closed" : "Road reopened", graph.name(a) + " - " + graph.name(b));
	    return true;
	}
	
	// save / load: rider stats and closed roads (riders matched by id; busy riders keep their position)
	// The first line holds the map fingerprint, a save only loads on the map it was made on, because it stores
	// area numbers, and those mean something different on another map
	bool save(const char* file)
	{
	    ofstream f(file);
	
	    if (!f)
	    {
	        return false;
	    }
	
	    f << "map " << graph.fingerprint() << "\n";
	    f << "clock " << clock << "\n";
	
	    for (int i = 0; i < riders.size(); i++)
	    {
	        Rider* r = riders[i];
	        f << "rider " << r->id << " " << r->node << " " << r->rating << " " << r->ridesDone << " " << r->perKm << "\n";
	    }
	
	    for (int i = 0; i < graph.size(); i++)
	    {
	        for (int k = 0; k < graph.degree(i); k++)
	        {
	            if (graph.edge(i, k).to > i && graph.edge(i, k).blocked)
	            {
	                f << "blocked " << i << " " << graph.edge(i, k).to << "\n";
	            }
	        }
	    }
	
	    return true;
	}
	
	bool load(const char* file)
	{
	    loadError = "";
	    ifstream f(file);
	
	    if (!f)
	    {
	        loadError = "No save file found.";
	        return false;
	    }
	
	    string tag;
	    unsigned long h = 0;
	
	    if (!(f >> tag >> h) || tag != "map")
	    {
	        loadError = "This save file is from an older version (it has no map id).";
	        return false;
	    }
	
	    if (h != graph.fingerprint())
	    {
	        loadError = "This save was made for a different map, so it cannot be loaded.";
	        return false;
	    }
	
	    for (int i = 0; i < graph.size(); i++)
	    {
	        for (int k = 0; k < graph.degree(i); k++)
	        {
	            graph.setBlocked(i, graph.edge(i, k).to, false);
	        }
	    }
	
	    while (f >> tag)
	    {
	        if (tag == "clock")
	        {
	            f >> clock;
	        }
	        else if (tag == "rider")
	        {
	            int id, node, done;
	            float rating, perKm;
	
	            f >> id >> node >> rating >> done >> perKm;
	
	            Rider* r;
	
	            if (riderIndex.get(id, r) && node >= 0 && node < graph.size())
	            {
	                if (r->state == IDLE)
	                {
	                    r->node = r->prevNode = node;
	                }
	
	                r->rating = rating;
	                r->ridesDone = done;
	                r->perKm = perKm;
	                refreshBoard(r);
	            }
	        }
	        else if (tag == "blocked")
	        {
	            int a, b;
	            f >> a >> b;
	
	            if (graph.edgeWeight(a, b) >= 0)
	            {
	                graph.setBlocked(a, b, true); // only roads that exist on this map
	            }
	        }
	    }
	
		//outputs information to the log
	    say(LOG_SYSTEM, "Game loaded");
	    return true;
	}
	
	// One tick is one step of the world
	void tick()
	{
	    clock++;
	    updateClosures();
	
	    for (int i = 0; i < graph.size(); i++)
	    {
	        demand[i] *= 0.85f; // demand fades over time
	    }
	
	    if (rand() % 100 < 100 && open.size() < 20)
	    {
	        spawnNpcUser();
	    }
	
	    for (int i = open.size() - 1; i >= 0; i--)
	    {
	        // patience runs out
	        if (--open[i]->patience <= 0)
	        {
	        	//outputs information to the log
	            say(LOG_CANCEL, open[i]->name + " gave up waiting", "no driver took the request", 0, open[i]->id);
	            delete open[i];
	            open.removeAt(i);
	        }
	    }
	
	    for (int i = open.size() - 1; i >= 0; i--)
	    {
	        // NPC riders grab requests
	        if (rand() % 100 >= 33)
	        {
	            continue;
	        }
	
	        MinHeap<Bid> heap;
	        User* u = open[i];
	
	        if (collectBids(u, heap) == 0)
	        {
	            continue;
	        }
	
	        Bid best = heap.pop();
	
	        if (best.price > u->offer * 1.50f)
	        {
	            continue; // NPC user finds it too expensive
	        }
	
	        open.removeAt(i);
	        startRide(riders[best.riderIdx], u, best.price);
	    }
	
	    for (int i = active.size() - 1; i >= 0; i--)
	    {
	        // move every ride one step
	        advance(active[i]);
	
	        if (active[i]->phase == 2)
	        {
	            history.push_back(active[i]);
	            active.removeAt(i);
	        }
	    }
	
	    stepOffers(); // after the NPC riders had their chance to grab jobs
	}
	
	private:
	
	// One tick of the player's request: new offers arrive, taken riders drop out, the oldest offers expire (FIFO).
	void stepOffers()
	{
	    if (!pendingUser)
	    {
	        return;
	    }
	
	    for (int i = 0; i < incoming.size();)
	    {
	        // 1. offers whose random arrival tick has come
	        if (incoming[i].arriveTick > clock)
	        {
	            i++;
	            continue;
	        }
	
	        Offer o = incoming[i];
	        incoming.removeAt(i);
	
	        Rider* r = riders[o.bid.riderIdx];
	
	        if (r->state != IDLE)
	        {
	            continue; // got another job on the way
	        }
	
	        offers.enqueue(o);
	
			//outputs information to the log
	        say(LOG_MATCH, r->name + " offered " + rupees(o.bid.price), num1(o.bid.pickupDist) + " km away, rating " + num1(r->rating),
	            r->id, pendingUser->id);
	    }
	
	    for (int i = offers.size() - 1; i >= 0; i--)
	    {
	        // 2. a rider who took another job is gone
	        Rider* r = riders[offers.at(i).bid.riderIdx];
	
	        if (r->state == IDLE)
	        {
	            continue;
	        }
	
			//outputs information to the log
	        say(LOG_CANCEL, r->name + " took another ride", "offer is off the board", r->id, pendingUser->id);
	        offers.removeAt(i);
	    }
	
	    while (!offers.empty() && offers.front().leaveTick <= clock)
	    {
	        // 3. FIFO: the oldest offer is the first to leave
	        Offer o = offers.dequeue();
	        Rider* r = riders[o.bid.riderIdx];
	
			//outputs information to the log
	        say(LOG_CANCEL, r->name + " lost interest", "you did not pick this offer in time", r->id, pendingUser->id);
	    }
	
	    if (offers.empty() && incoming.empty())
	    {
	        // 4. everybody left: the request is over
			//outputs information to the log
	        say(LOG_CANCEL, "Nobody was picked in time", "all riders lost interest", 0, pendingUser->id);
	        stopPlayerRequest();
	    }
	}
	
	int target(Ride* r)
	{
	    return r->phase == 0 ? r->user->pickup : r->user->dest;
	}
	
	void arrivalCheck(Ride* r)
	{
	    while (r->phase < 2 && r->rider->node == target(r))
	    {
	        if (r->phase == 0)
	        {
	            r->phase = 1;
	            r->rider->state = ON_TRIP;
	            r->user->state = ON_TRIP;
	
	            route(r->rider->node, r->user->dest, r->path);
	            r->pos = 0;
	
				//outputs information to the log
	            say(LOG_PICKUP, who(r->rider, true) + " picked up " + who(r->user), "at " + graph.name(r->user->pickup), r->rider->id, r->user->id);
	        }
	        else
	        {
	            finish(r);
	        }
	    }
	}
	
	void finish(Ride* r)
	{
	    r->phase = 2;
	    Rider* d = r->rider;
	
	    int stars = r->detours == 0 ? 5 : (5 - r->detours < 1 ? 1 : 5 - r->detours);
	
	    d->rating = (d->rating * d->ridesDone + stars) / (d->ridesDone + 1);
	    d->ridesDone++;
	
	    d->state = IDLE;
	    r->user->state = DONE;
	    r->user->node = r->user->dest;
	
	    RideRecord rec; // both sides keep the trip in their history list
	
	    rec.from = graph.name(r->user->pickup);
	    rec.to = graph.name(r->user->dest);
	    rec.fare = r->fare;
	    rec.km = r->travelled;
	    rec.stars = stars;
	
	    d->history.push_back(rec);
	    r->user->history.push_back(rec);
	
	    refreshBoard(d);
	
	    string detail = rupees(r->fare) + ", " + num1(r->travelled) + " km (best " + num1(r->optimal) + ")";
	
	    if (r->detours > 0)
	    {
	        detail += ", " + to_string(r->detours) + (r->detours == 1 ? " wrong turn" : " wrong turns");
	    }
	
	    if (r->delay > 0)
	    {
	        detail += ", +" + to_string(r->delay) + " ticks in traffic";
	    }
	
	    detail += ", rating " + num2(d->rating);
	
		//outputs information to the log
	    say(LOG_DROPOFF, who(d, true) + " dropped " + who(r->user) + " off at " + graph.name(r->user->dest), detail, d->id, r->user->id);
	}
	
	// Rare random accidents causes at most 2 roads closed at a time, each reopens after 10-25 ticks.
	void updateClosures()
	{
	    for (int i = closures.size() - 1; i >= 0; i--)
	    {
	        if (closures[i].until > clock)
	        {
	            continue;
	        }
	
	        if (graph.isBlocked(closures[i].a, closures[i].b))
	        {
	            graph.setBlocked(closures[i].a, closures[i].b, false);
		
				//outputs information to the log
	            say(LOG_REOPENED, "Road reopened", graph.name(closures[i].a) + " - " + graph.name(closures[i].b));
	        }
	
	        closures.removeAt(i);
	    }
	
	    if (closures.size() >= 2 || rand() % 100 >= 3)
	    {
	        return;
	    }
	
	    int a = rand() % graph.size();
	
	    if (graph.degree(a) == 0)
	    {
	        return;
	    }
	
	    int b = graph.edge(a, rand() % graph.degree(a)).to;
	
	    if (graph.isBlocked(a, b))
	    {
	        return;
	    }
	
	    Closure c;
	    c.a = a;
	    c.b = b;
	    c.until = clock + 10 + rand() % 15;
	
	    graph.setBlocked(a, b, true);
	    closures.push_back(c);
	
		//outputs information to the log
	    say(LOG_CLOSED, "Accident: road closed", graph.name(a) + " - " + graph.name(b));
	}
	
	void advance(Ride* r)
	{
	    Rider* d = r->rider;
	    int tgt = target(r);
	
	    if (r->wait > 0)
	    {
	        r->wait--;
	        return; // sitting in a traffic jam
	    }
	
	    if (r->transit > 0)
	    {
	        // still driving along a long road
	        if (--r->transit == 0)
	        {
	            arrivalCheck(r); // last tick on this road means we have arrived
	        }
	
	        return;
	    }
	
	    int next = -1;
	
	    if (d->detourChance > 0 && (rand() % 100) < (int)(d->detourChance * 100) && graph.degree(d->node) > 1)
	    {
	        for (int tries = 0; tries < 6 && next == -1; tries++)
	        {
	            // wander to a random open road
	            const Edge& e = graph.edge(d->node, rand() % graph.degree(d->node));
	            int ahead = r->path.size() > 1 ? r->path[1] : -1;
	
	            if (!e.blocked && e.to != ahead)
	            {
	                next = e.to;
	            }
	        }
	
	        if (next != -1)
	        {
	            r->detours++;
				//outputs information to the log
	            say(LOG_DETOUR, who(d, true) + " took a wrong turn", "now heading to " + graph.name(next), d->id, r->user->id);
	        }
	    }
	
	    if (next == -1)
	    {
	        bool jam = false;
	
	        if (r->path.size() < 2)
	        {
	            route(d->node, tgt, r->path, &jam);
	            r->pos = 0;
	            return;
	        }
	
	        next = r->path[1];
	
	        if (graph.isBlocked(d->node, next) && !r->crossing)
	        {
	            // a closure appeared on our route
	            route(d->node, tgt, r->path, &jam); // -> next-best path (or traffic if none)
	            r->pos = 0;
	
	            if (!jam)
	            {
	      			//outputs information to the log
		            say(LOG_REROUTE, who(d, true) + (d->isNPC ? " takes" : " take") + " the next-best route", "around a closed road", d->id, r->user->id);
	                return;
	            }
	
	            r->crossing = true;
	            r->wait = 1;
	            r->delay += 2; // no way around: crawl through it
	
				//outputs information to the log
	            say(LOG_TRAFFIC, who(d, true) + (d->isNPC ? " is" : " are") + " stuck in traffic", "no way around the closed road, +2 ticks", d->id, r->user->id);
	            return;
	        }
	    }
	
	    float w = graph.edgeWeight(d->node, next);
	    r->travelled += w;
	
	    d->prevNode = d->node;
	    d->node = next; // enter the road; the GUI slides the car along it
	
	    d->moveStart = clock;
	    d->moveTicks = (int)(w / kmPerTick + 0.5f); // longer road = more ticks = constant speed
	
	    if (d->moveTicks < 1)
	    {
	        d->moveTicks = 1;
	    }
	
	    r->transit = d->moveTicks - 1;
	    r->crossing = false;
	
	    route(d->node, tgt, r->path); // (re)plan from where we are
	    r->pos = 0;
	
	    if (r->transit == 0)
	    {
	        arrivalCheck(r); // 1-tick road: arrived already
	    }
	}
};