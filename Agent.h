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
    int node;
    bool isNPC;
    AgentState state;
    LinkedList<RideRecord> history;

    Agent(int i, const string& n, int nd, bool npc) : id(i), name(n), node(nd), isNPC(npc), state(IDLE) 
    { }
    virtual ~Agent() 
    { }
};

class Rider : public Agent {
public:
    float rating;
    int ridesDone;
    float perKm;
    float maxPickup;
    float detourChance;
    float greed;
    float lbKey;
    int prevNode;
    int moveStart, moveTicks;

    Rider(int i, const string& n, int nd, bool npc)
        : Agent(i, n, nd, npc), rating(4.5f), ridesDone(0), perKm(60),
                maxPickup(12), detourChance(0), greed(0.2f), lbKey(0), prevNode(nd), moveStart(0), moveTicks(1) 
    { }
};

class User : public Agent {
public:
    int pickup, dest;
    float offer;
    int patience;

    User(int i, const string& n, int nd, bool npc) 
    : Agent(i, n, nd, npc), pickup(nd), dest(nd), offer(0), patience(10) 
    { }
};
