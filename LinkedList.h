// LinkedList.h - doubly linked list with a cursor (Week 3). Used for ride history.
#include <iostream>
using namespace std;
#pragma once

template <typename T>
class LinkedList {
    struct N {
        T v; N* prev; N* next;
        N(const T& x) : v(x), prev(0), next(0) {}
    };
    N *head, *tail, *cur;
    int cnt;

    void clearAll() {
        while (head) { N* t = head; head = head->next; delete t; }
        tail = cur = 0; cnt = 0;
    }
public:
    LinkedList() : head(0), tail(0), cur(0), cnt(0) {}
    LinkedList(const LinkedList& o) : head(0), tail(0), cur(0), cnt(0) {
        for (N* p = o.head; p; p = p->next) push_back(p->v);
    }
    LinkedList& operator=(const LinkedList& o) {
        if (this != &o) { clearAll(); for (N* p = o.head; p; p = p->next) push_back(p->v); }
        return *this;
    }
    ~LinkedList() { clearAll(); }

    void push_back(const T& x) {                  // newest entry becomes the cursor
        N* n = new N(x);
        if (!tail) head = tail = n; else { tail->next = n; n->prev = tail; tail = n; }
        cur = tail; cnt++;
    }
    int size() const { return cnt; }
    bool empty() const { return cnt == 0; }
    T& current() { return cur->v; }
    bool movePrev() { if (cur && cur->prev) { cur = cur->prev; return true; } return false; }
    bool moveNext() { if (cur && cur->next) { cur = cur->next; return true; } return false; }
    void toNewest() { cur = tail; }
};
