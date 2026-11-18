#pragma once
#include <iostream>
#include <cstdlib>
using namespace std;

template <typename T>
class Queue {
    T* data;
    int cap, head, cnt;
    void grow() {
        T* nd = new T[cap * 2];
        for (int i = 0; i < cnt; i++) 
            nd[i] = data[(head + i) % cap];
        delete[] data;
        data = nd; 
        head = 0; 
        cap *= 2;
    }
public:
    Queue(int c = 8) : data(new T[c > 0 ? c : 1]), cap(c > 0 ? c : 1), head(0), cnt(0) 
    {}
    Queue(const Queue& o) : data(new T[o.cap]), cap(o.cap), head(0), cnt(o.cnt) {
        for (int i = 0; i < cnt; i++) 
            data[i] = o.data[(o.head + i) % o.cap];
    }
    Queue& operator=(const Queue& o) {
        if (this != &o) {
            T* nd = new T[o.cap];
            for (int i = 0; i < o.cnt; i++) 
                nd[i] = o.data[(o.head + i) % o.cap];
            delete[] data;
            data = nd; 
            cap = o.cap; 
            head = 0; 
            cnt = o.cnt;
        }
        return *this;
    }
    ~Queue() { 
        delete[] data; 
    }

    void enqueue(const T& x) { 
        if (cnt == cap) 
            grow(); 
        data[(head + cnt) % cap] = x; 
        cnt++; 
    }
    T dequeue() {
        if (cnt == 0) { 
            cerr << "Queue underflow\n"; 
            exit(1); 
        }
        T x = data[head];
        head = (head + 1) % cap; 
        cnt--;
        return x;
    }
    bool empty() const { 
        return cnt == 0; 
    }
    int size() const { 
        return cnt; 
    }

    // added for the live offer board. Looks at / remove entries without losing the FIFO order
    const T& front() const {                       // oldest entry, not removed
        if (cnt == 0) { 
            cerr << "Queue is empty\n"; 
            exit(1); 
        }
        return data[head];
    }

    const T& at(int i) const {                     // i-th entry from the front (0 = oldest), for drawing the whole queue
        if (i < 0 || i >= cnt) { 
            cerr << "Queue index out of range\n"; 
            exit(1); 
        }
        return data[(head + i) % cap];
    }

    void removeAt(int i) {                         // remove the i-th entry (a customer picks from the middle). The rest keep their order
        if (i < 0 || i >= cnt) 
            return;
        for (int k = i; k < cnt - 1; k++) 
            data[(head + k) % cap] = data[(head + k + 1) % cap];
        cnt--;
    }
    void clear() { 
        head = 0; 
        cnt = 0; 
    }
};
