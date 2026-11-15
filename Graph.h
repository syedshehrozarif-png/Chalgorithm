#pragma once
#include <string>
#include <cmath>
#include <fstream>
#include <sstream>
#include "DynamicArray.h"
using namespace std;

const float MAP_BOX_W = 2000, MAP_BOX_H = 1400; //size of map, all node coordinates sit in this

struct Edge {
    int to;
    float w;
    bool blocked;
};

static inline unsigned long fnv1a(const string& s) { //fingerprint of map, to see if it ever changed
    unsigned long h = 2166136261UL;
    for (size_t i = 0; i < s.size(); i++) 
    { 
        h ^= (unsigned char)s[i]; 
        h = (h * 16777619UL) & 0xFFFFFFFFUL; 
    }
    return h;
}
static inline bool readTextFile(const char* path, string& out)  //read file
{
    ifstream f(path, ios::binary);

    if (!f) 
    return false;

    ostringstream ss;
    ss << f.rdbuf();
    out = "";
    string raw = ss.str();

    for (size_t i = 0; i < raw.size(); i++) 
    if (raw[i] != '\r') out += raw[i];

    return true;
}

class Graph {
    static const int MAXN = 64; //,ax number of areas
    string names[MAXN];
    float xs[MAXN], ys[MAXN];
    Edge* adj[MAXN];
    int deg[MAXN];
    int cap[MAXN];

    int n;
    unsigned long mapHash;  // fingerprint of nodes.txt + edges.txt
    string note;
    int roads, realRoads;   // roads in the map / roads that use a real distance (for when script doesnt find real road)
    float hScale, avgKm;    // A* heuristic scale, average road length

    Graph(const Graph&);              // not copyable (owns raw memory)
    Graph& operator=(const Graph&);

    struct WRec 
    { 
        int a, b; 
        float ab, ba; 
    };

    bool valid(int a) const 
    { 
        return a >= 0 && a < n; 
    }

    void addOneWay(int a, int b, float w) 
    {
        if (deg[a] == cap[a]) 
        {
            Edge* ne = new Edge[cap[a] * 2];

            for (int i = 0; i < deg[a]; i++) 
            ne[i] = adj[a][i];

            delete[] adj[a];
            adj[a] = ne; 
            cap[a] *= 2;
        }

        Edge e; 
        e.to = b; 
        e.w = w; 
        e.blocked = false;
        adj[a][deg[a]++] = e;
    }


    bool readWeights(const char* file, DynamicArray<WRec>& recs) //weights.txt file reading
    {
        ifstream f(file);
        if (!f) 
        { 
            note = "No weights.txt: roads use pixel distances (estimates)."; 
            return false; 
        }

        string tag; 
        unsigned long h = 0;

        if (!(f >> tag >> h) || tag != "map") 
        { 
            note = "weights.txt is not in the expected format, so it was ignored."; 
            return false; 
        }

        if (h != mapHash) 
        { 
            note = "weights.txt was made for a different map (areas or roads changed). Re-run fetch_distances.py. Using pixel distances."; 
            return false; 
        }

        WRec r;
        while (f >> r.a >> r.b >> r.ab >> r.ba)
            if (r.ab == r.ab && r.ba == r.ba && r.ab > 0 && r.ba > 0)       // never writes non-numbers / negative numbers
            { 
                if (r.ab < 0.1f) 
                r.ab = 0.1f;

                if (r.ba < 0.1f) 
                r.ba = 0.1f;

                recs.push_back(r);
            }
        return true;
    }
public:
    Graph() : n(0), mapHash(0), roads(0), realRoads(0), hScale(1.0f), avgKm(1.0f) {}

    ~Graph() 
    { 
        for (int i = 0; i < n; i++) 
        delete[] adj[i]; 
    }

    int addNode(const string& name, float x, float y) 
    {
        if (n >= MAXN) 
        return -1;

        names[n] = name; 
        xs[n] = x; 
        ys[n] = y;
        adj[n] = new Edge[2]; 
        deg[n] = 0; 
        cap[n] = 2;
        return n++;
    }
    float pixelKm(int a, int b) const           //pytahgorean theorem to find distance on map, 40 used as pixel distance for km
    {
        float dx = xs[a] - xs[b], dy = ys[a] - ys[b];
        return sqrt(dx * dx + dy * dy) / 40.0f;
    }

    void addRoad(int a, int b, float kmAB, float kmBA) 
    { 
        if (!valid(a) || !valid(b) || a == b) return;
        addOneWay(a, b, kmAB);
        addOneWay(b, a, kmBA);
        roads++;
    }

    void addRoad(int a, int b)        // two-way, weight from coordinates
    { 
        addRoad(a, b, pixelKm(a, b), pixelKm(a, b)); 
    }
    
    void setBlocked(int a, int b, bool v) 
    {
        if (!valid(a) || !valid(b)) 
        return;

        for (int i = 0; i < deg[a]; i++) 
        if (adj[a][i].to == b) adj[a][i].blocked = v;

        for (int i = 0; i < deg[b]; i++) 
        if (adj[b][i].to == a) adj[b][i].blocked = v;
    }
    bool isBlocked(int a, int b) const 
    {
        if (!valid(a) || !valid(b)) 
        return false;

        for (int i = 0; i < deg[a]; i++) 
        if (adj[a][i].to == b) return adj[a][i].blocked;

        return false;
    }
    float edgeWeight(int a, int b) const                //non-symmetrical edges
    {
        if (!valid(a) || !valid(b)) 
        return -1;

        for (int i = 0; i < deg[a]; i++) 
        if (adj[a][i].to == b) return adj[a][i].w;

        return -1;
    }

    //GETTERS

    int size() const 
    { 
        return n; 
    }

    const string& name(int i) const 
    { 
        return names[i]; 
    }

    float x(int i) const 
    { 
        return xs[i]; 
    }

    float y(int i) const 
    { 
        return ys[i]; 
    }

    int degree(int i) const 
    { 
        return deg[i]; 
    }

    const Edge& edge(int i, int k) const 
    { 
        return adj[i][k]; 
    }

    unsigned long fingerprint() const 
    { 
        return mapHash; 
    }

    const string& loadNote() const
    { 
        return note; 
    }

    int roadCount() const 
    { 
        return roads; 
    }

    int realRoadCount() const 
    { 
        return realRoads; 
    }

    float heuristicScale() const 
    { 
        return hScale; 
    }

    float averageRoadKm() const 
    { 
        return avgKm; 
    }



    void updateScales() // call after all roads are added: computes hScale (for A*) and avgKm
    {
        float minRatio = 1e9f, sum = 0; 
        int cnt = 0;

        for (int i = 0; i < n; i++)
            for (int k = 0; k < deg[i]; k++) 
            {
                float p = pixelKm(i, adj[i][k].to);
                sum += adj[i][k].w; cnt++;

                if (p > 0.01f && adj[i][k].w / p < minRatio)
                 minRatio = adj[i][k].w / p;
            }
        hScale = minRatio > 1e8f ? 1.0f : minRatio * 0.999f;

        avgKm = cnt ? sum / cnt : 1.0f;
    }


    bool loadFromFiles(const char* nodesFile, const char* edgesFile, const char* weightsFile = "weights.txt") 
    {
        string nt, et;

        if (!readTextFile(nodesFile, nt) || !readTextFile(edgesFile, et)) 
        return false;

        mapHash = fnv1a(nt + et);
        istringstream fn(nt), fe(et);
        string nm; 
        float x, y;

        while (fn >> nm >> x >> y) 
        {
            for (size_t i = 0; i < nm.size(); i++) 
            if (nm[i] == '_') nm[i] = ' ';

            addNode(nm, x, y);
        }

        DynamicArray<WRec> recs;

        bool haveWeights = readWeights(weightsFile, recs);
        int a, b;
        while (fe >> a >> b) 
        {
            if (!valid(a) || !valid(b) || a == b) 
            continue;

            bool found = false;

            for (int i = 0; i < recs.size() && !found; i++) 
            {
                if (recs[i].a == a && recs[i].b == b) 
                { 
                    addRoad(a, b, recs[i].ab, recs[i].ba);
                     found = true; 
                }

                else if (recs[i].a == b && recs[i].b == a) 
                { 
                    addRoad(a, b, recs[i].ba, recs[i].ab); 
                    found = true; 
                }

            }

            if (found) 
            realRoads++; 
            
            else 
            addRoad(a, b);

        }
        if (haveWeights) 
        {
            note = realRoads == roads ? "Road distances: real, from weights.txt (all " + to_string(roads) + " roads)."
                 : "Road distances: real for " + to_string(realRoads) + " of " + to_string(roads) + " roads; the rest use pixel estimates.";
        }
        updateScales();
        return n > 0;
    }


//default city for when map isnt made

    void buildDefaultCity() {
        mapHash = fnv1a("builtin-sample-city");
        note = "Built-in sample city (pixel distances).";
        addNode("Saddar", 480, 420);          // 0
        addNode("Clifton", 520, 560);         // 1
        addNode("DHA", 680, 560);             // 2
        addNode("Korangi", 800, 470);         // 3
        addNode("Gulshan", 600, 250);         // 4
        addNode("Airport", 760, 230);         // 5
        addNode("NorthNazimabad", 420, 150);  // 6
        addNode("Malir", 880, 320);           // 7
        addNode("Lyari", 330, 400);           // 8
        addNode("Johar", 700, 320);           // 9
        addNode("Nazimabad", 400, 270);       // 10
        addNode("Karsaz", 640, 410);          // 11
        addNode("Orangi", 230, 230);          // 12
        int e[][2] = {{0,1},{0,8},{0,11},{0,10},{1,2},{2,3},{2,11},{11,3},{11,4},{4,9},{9,5},
                      {9,7},{5,7},{7,3},{4,6},{6,10},{10,8},{12,6},{12,8},{10,4}};
        for (int i = 0; i < 20; i++) addRoad(e[i][0], e[i][1]);
        updateScales();
    }
};
