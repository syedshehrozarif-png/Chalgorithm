#include <iostream>
using namespace std;
#pragma once

enum HashMode{ CHAINING, LINEAR_PROBING, DOUBLE_HASHING };

template <typename V>
class HashTable{
    struct Node{
        int key;
        V val;
        Node *next;
    };
    HashMode mode;
    int cap, cnt, rehashes;
    long collisions;
    Node **chains; // used when CHAINING
    int *keys;
    V *vals;
    bool *used; // used for open addressing

    HashTable(const HashTable &);
    HashTable &operator=(const HashTable &);

    static int nextPrime(int c){
        static const int P[] = {11, 23, 47, 97, 197, 397, 797, 1597, 3203, 6421, 12853, 25717, 51437, 102877};
        for (int i = 0; i < 14; i++)
            if (P[i] > c)
                return P[i];
        return c * 2 + 1;
    }
    int h1(int k) const {
        return (int)(((long)k % cap + cap) % cap);
    }
    int step(int k) const {
        if (mode != DOUBLE_HASHING)
            return 1;
        return 1 + (int)(((long)k % (cap - 1) + (cap - 1)) % (cap - 1)); // second hash
    }
    void alloc(int c){
        cap = c;
        cnt = 0;
        chains = 0;
        keys = 0;
        vals = 0;
        used = 0;
        if (mode == CHAINING){
            chains = new Node *[cap];
            for (int i = 0; i < cap; i++)
                chains[i] = 0;
        }
        else {
            keys = new int[cap];
            vals = new V[cap];
            used = new bool[cap];
            for (int i = 0; i < cap; i++)
                used[i] = false;
        }
    }
    void release(){
        if (mode == CHAINING){
            for (int i = 0; i < cap; i++)
                while (chains[i]){
                    Node *t = chains[i];
                    chains[i] = t->next;
                    delete t;
                }
            delete[] chains;
        }
        else {
            delete[] keys;
            delete[] vals;
            delete[] used;
        }
    }
    void insertRaw(int k, const V &v){
        if (mode == CHAINING){
            int i = h1(k);
            if (chains[i])
                collisions++;
            Node *n = new Node;
            n->key = k;
            n->val = v;
            n->next = chains[i];
            chains[i] = n;
        } 
        else {
            int i = h1(k), s = step(k);
            while (used[i])
            {
                collisions++;
                i = (i + s) % cap;
            }
            used[i] = true;
            keys[i] = k;
            vals[i] = v;
        }
        cnt++;
    }
    void rehash(){
        int oldCap = cap;
        Node **oc = chains;
        int *ok = keys;
        V *ov = vals;
        bool *ou = used;
        alloc(nextPrime(cap));
        rehashes++;
        if (mode == CHAINING){
            for (int i = 0; i < oldCap; i++){
                Node *p = oc[i];
                while (p){
                    insertRaw(p->key, p->val);
                    Node *t = p;
                    p = p->next;
                    delete t;
                }
            }
            delete[] oc;
        }
        else {
            for (int i = 0; i < oldCap; i++)
                if (ou[i])
                    insertRaw(ok[i], ov[i]);
            delete[] ok;
            delete[] ov;
            delete[] ou;
        }
    }

public:
    HashTable(HashMode m = CHAINING) : mode(m), rehashes(0), collisions(0) { alloc(11); }
    ~HashTable() {
        release(); 
    }

    void insert(int k, const V &v){ // keys are assumed unique (ids)
        bool tooFull = (mode == CHAINING) ? (cnt + 1) * 4 > cap * 3 : (cnt + 1) * 2 > cap;
        if (tooFull)
            rehash();
        insertRaw(k, v);
    }
    bool get(int k, V &out) const {
        if (mode == CHAINING){
            for (Node *p = chains[h1(k)]; p; p = p->next)
                if (p->key == k){
                    out = p->val;
                    return true;
                }
            return false;
        }
        int i = h1(k), s = step(k);
        for (int t = 0; t < cap && used[i]; t++){
            if (keys[i] == k)
            {
                out = vals[i];
                return true;
            }
            i = (i + s) % cap;
        }
        return false;
    }
    int size() const {
        return cnt; 
    }
    int capacity() const { 
        return cap; 
    }
    long getCollisions() const { 
        return collisions; 
    }
    int getRehashes() const { 
        return rehashes; 
    }
    const char *modeName() const {
        return mode == CHAINING ? "Chaining" : (mode == LINEAR_PROBING ? "Linear probing" : "Double hashing");
    }
};
