// Pathfinding.h - Dijkstra (heap based) and BFS. Pure logic: no printing, no drawing.
#pragma once
#include "Graph.h"
#include "MinHeap.h"
#include "Queue.h"
#include "DynamicArray.h"
#include <iostream>
using namespace std;

struct HeapItem {
    float d;
    int node;
    bool operator<(const HeapItem& o) const 
    { 
        return d < o.d; 
    }
};

const float TRAFFIC_MULT = 3.0f;      // a closed road driven through as heavy traffic costs 3x

inline float heuristic(const Graph& g, int a, int b) 
{
    float dx = g.x(a) - g.x(b), dy = g.y(a) - g.y(b);
    return g.heuristicScale() * sqrt(dx * dx + dy * dy) / 40.0f;
}

inline float searchCore(const Graph& g, int src, int dst, DynamicArray<int>& path, int* explored, bool useH, bool traffic = false) 
{
    const float INF = 1e9f;
    int n = g.size();
    float* dist = new float[n];
    int* prev = new int[n];
    bool* done = new bool[n];
    for (int i = 0; i < n; i++) 
    { 
        dist[i] = INF; 
        prev[i] = -1; 
        done[i] = false; }

    MinHeap<HeapItem> pq;
    dist[src] = 0;
    HeapItem s; s.d = 0; s.node = src;
    pq.push(s);
    int count = 0;

    while (!pq.empty()) 
    {
        HeapItem cur = pq.pop();

        if (done[cur.node]) 
        continue;

        done[cur.node] = true;
        count++;

        if (cur.node == dst)
        break;

        for (int k = 0; k < g.degree(cur.node); k++) 
        {
            const Edge& e = g.edge(cur.node, k);
            float w = e.w;
            if (e.blocked) 
            { 
                if (!traffic) 
                continue; 
                
                w *= TRAFFIC_MULT; 
            }   

            float nd = dist[cur.node] + w;

            if (nd < dist[e.to]) 
            {
                dist[e.to] = nd;
                prev[e.to] = cur.node;
                HeapItem h; 

                h.d = nd + (useH ? heuristic(g, e.to, dst) : 0.0f); 

                h.node = e.to;
                pq.push(h);
            }
        }
    }

    if (explored) 
    *explored = count;

    path.clear();
    float result = -1;

    if (dist[dst] < INF)
    {
        result = dist[dst];

        for (int v = dst; v != -1; v = prev[v]) 
        path.push_back(v);

        path.reverse();
    }

    delete[] dist; 
    delete[] prev; 
    delete[] done;

    return result;
}

inline float dijkstra(const Graph& g, int src, int dst, DynamicArray<int>& path, int* explored = 0, bool traffic = false) 
{
    return searchCore(g, src, dst, path, explored, false, traffic);
}

inline float astar(const Graph& g, int src, int dst, DynamicArray<int>& path, int* explored = 0) 
{
    return searchCore(g, src, dst, path, explored, true);
}

inline int bfs(const Graph& g, int src, int dst, DynamicArray<int>& path, int* explored = 0) 
{
    int n = g.size();
    int* prev = new int[n];
    bool* seen = new bool[n];

    for (int i = 0; i < n; i++) 
    { 
        prev[i] = -1; 
        seen[i] = false; 
    }

    Queue<int> q;

    q.enqueue(src); 
    seen[src] = true;
    int count = 0;

    while (!q.empty()) 
    {
        int u = q.dequeue();
        count++;

        if (u == dst) 
        break;

        for (int k = 0; k < g.degree(u); k++) 
        {
            const Edge& e = g.edge(u, k);

            if (e.blocked || seen[e.to]) 
            continue;

            seen[e.to] = true; 
            prev[e.to] = u; 
            q.enqueue(e.to);

        }
    }
    if (explored) 
    *explored = count;

    path.clear();
    int hops = -1;
    if (seen[dst]) 
    {
        for (int v = dst; v != -1; v = prev[v]) 
        path.push_back(v);

        path.reverse();
        hops = path.size() - 1;
    }
    delete[] prev; 
    delete[] seen;
    return hops;
}

const int MAXSTOPS = 6;

struct StopPlan { float best; int order[MAXSTOPS]; long visited; };

inline void planRec(float D[MAXSTOPS + 1][MAXSTOPS + 1], int k, bool* used, int cur, int depth,
                    float cost, int* order, StopPlan& plan) 
{
    plan.visited++;

    if (cost >= plan.best) 
    return;                                  // prune

    if (depth == k) 
    { 
        plan.best = cost; 
        
        for (int i = 0; i < k; i++) 
        plan.order[i] = order[i]; 
        
        return; 
    }

    for (int i = 1; i <= k; i++) 
    {

        if (used[i]) 
        continue;

        used[i] = true; 
        order[depth] = i;                           // choose

        planRec(D, k, used, i, depth + 1, cost + D[cur][i], order, plan);
        used[i] = false;                                            // un-choose (backtrack)
    }
}

inline bool planStops(const Graph& g, int start, const int* stops, int k, StopPlan& plan, float& naive) 
{
    int pts[MAXSTOPS + 1];
    pts[0] = start;

    for (int i = 0; i < k; i++) 
    pts[i + 1] = stops[i];

    float D[MAXSTOPS + 1][MAXSTOPS + 1];
    DynamicArray<int> tmp;

    for (int a = 0; a <= k; a++)
        for (int b = 0; b <= k; b++) 
        {
            D[a][b] = (a == b) ? 0.0f : dijkstra(g, pts[a], pts[b], tmp);

            if (D[a][b] < 0) 
            return false;
        }

    naive = 0;

    for (int i = 1; i <= k; i++) 
    naive += D[i - 1][i];

    plan.best = 1e9f; 
    plan.visited = 0;
    bool used[MAXSTOPS + 1] = {false};
    int order[MAXSTOPS];

    planRec(D, k, used, 0, 0, 0.0f, order, plan);
    return true;
}
