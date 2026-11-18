#pragma once
#include <string>
#include "LinkedList.h"
using namespace std;

struct RideRecord {
    string from, to;
    float fare, km;
    int stars;
};

enum AgentState { IDLE, REQUESTING, TO_PICKUP, ON_TRIP, DONE, CANCELLED };

class Agent {
public:
    int id;
    string name;
    int node;            // current location on the graph
    bool isNPC;
    AgentState state;
    LinkedList<RideRecord> history;     // doubly linked list of finished rides

    Agent(int i, const string& n, int nd, bool npc) : id(i), name(n), node(nd), isNPC(npc), state(IDLE) 
    { }
    virtual ~Agent() 
    { }
};

class Rider : public Agent {
public:
    float rating;         // 3.8 - 5.0
    int ridesDone;
    float perKm;          // minimum price per km
    float maxPickup;      // won't bid on pickups further than this (km)
    float detourChance;   // 0 = always shortest route; higher = "bad driver"
    float greed;          // bids perKm-cost * (1 + random(0..greed))
    float lbKey;          // rating currently stored in the AVL leaderboard
    int prevNode;         // start of the road the rider is driving on (the GUI slides the car from here)
    int moveStart, moveTicks;   // tick when the rider entered that road, and how many ticks it takes

    Rider(int i, const string& n, int nd, bool npc)
        : Agent(i, n, nd, npc), rating(4.5f), ridesDone(0), perKm(60),
                maxPickup(12), detourChance(0), greed(0.2f), lbKey(0), prevNode(nd), moveStart(0), moveTicks(1) 
    { }
};

class User : public Agent {
public:
    int pickup, dest;
    float offer;          // fare the user is offering
    int patience;         // ticks before the user cancels the request

    User(int i, const string& n, int nd, bool npc) 
    : Agent(i, n, nd, npc), pickup(nd), dest(nd), offer(0), patience(10) 
    { }
};
