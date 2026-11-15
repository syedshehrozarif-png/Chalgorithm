// AVL.h - self-balancing BST for the rider leaderboard
// Ordered by rating, then id
#pragma once
#include <string>
#include "DynamicArray.h"
using namespace std;

struct LbEntry{
    float rating;
    int id;
    string name;
};

class AVL{
    struct N{
        LbEntry e;
        N *l, *r;
        int h;
        N(const LbEntry &x) : e(x), l(0), r(0), h(1) {}
    };
    N *root;
    int cnt;

    AVL(const AVL &);
    AVL &operator=(const AVL &);

    static bool lt(const LbEntry &a, const LbEntry &b) { 
        return a.rating < b.rating || (a.rating == b.rating && a.id < b.id); 
    }
    static int H(N *n) {
        return n ? n->h : 0; 
    }
    static void upd(N *n) { 
        n->h = 1 + (H(n->l) > H(n->r) ? H(n->l) : H(n->r)); 
    }
    static int bal(N *n) { 
        return n ? H(n->l) - H(n->r) : 0; 
    }
    static N *rotR(N *y){
        N *x = y->l;
        y->l = x->r;
        x->r = y;
        upd(y);
        upd(x);
        return x;
    }
    static N *rotL(N *x){
        N *y = x->r;
        x->r = y->l;
        y->l = x;
        upd(x);
        upd(y);
        return y;
    }
    static N *rebalance(N *n){
        upd(n);
        int b = bal(n);
        if (b > 1){
            if (bal(n->l) < 0)
                n->l = rotL(n->l);
            return rotR(n);
        } // LL or LR
        if (b < -1){
            if (bal(n->r) > 0)
                n->r = rotR(n->r);
            return rotL(n);
        } // RR or RL
        return n;
    }
    N *ins(N *n, const LbEntry &e){
        if (!n){
            cnt++;
            return new N(e);
        }
        if (lt(e, n->e))
            n->l = ins(n->l, e);
        else
            n->r = ins(n->r, e);
        return rebalance(n);
    }
    N *del(N *n, const LbEntry &e){
        if (!n)
            return 0;
        if (lt(e, n->e))
            n->l = del(n->l, e);
        else if (lt(n->e, e))
            n->r = del(n->r, e);
        else{
            if (!n->l || !n->r){
                N *c = n->l ? n->l : n->r;
                delete n;
                cnt--;
                return c;
            }
            N *m = n->r;
            while (m->l)
                m = m->l; // in-order successor
            n->e = m->e;
            n->r = del(n->r, m->e);
        }
        return rebalance(n);
    }
    void destroy(N *n){
        if (n){
            destroy(n->l);
            destroy(n->r);
            delete n;
        }
    }
    void top(N *n, DynamicArray<LbEntry> &out, int k) const { // reverse in-order = highest first
        if (!n || out.size() >= k)
            return;
        top(n->r, out, k);
        if (out.size() < k)
            out.push_back(n->e);
        top(n->l, out, k);
    }

public:
    AVL() : root(0), cnt(0) {}
    ~AVL() { 
        destroy(root); 
    }
    void insert(const LbEntry &e) { 
        root = ins(root, e); 
    }
    void remove(const LbEntry &e) { 
        root = del(root, e); 
    }
    DynamicArray<LbEntry> topK(int k) const {
        DynamicArray<LbEntry> out;
        top(root, out, k);
        return out;
    }
    int size() const { 
        return cnt; 
    }
    int height() const { 
        return H(root); 
    }
};
