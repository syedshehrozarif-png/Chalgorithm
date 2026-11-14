#pragma once
#include "DynamicArray.h"
#include <iostream>
using namespace std;

template <typename T>
class MinHeap {
    DynamicArray<T> a;

    void siftUp(int i) {
        while (i > 0) {
            int p = (i - 1) / 2;
            if (a[i] < a[p]) { 
                T t = a[i]; 
                a[i] = a[p]; 
                a[p] = t; 
                i = p; 
            }
            else break;
        }
    }
    void siftDown(int i) {
        int n = a.size();
        while (true) {
            int l = 2 * i + 1;
            int r = 2 * i + 2;
            int m = i;
            if (l < n && a[l] < a[m]) 
                m = l;
            if (r < n && a[r] < a[m]) 
                m = r;
            if (m == i) break;
            T t = a[i]; 
            a[i] = a[m]; 
            a[m] = t;
            i = m;
        }
    }
public:
    void push(const T& x) { 
        a.push_back(x); 
        siftUp(a.size() - 1); 
    }
    T pop() {
        T top = a[0];
        a[0] = a[a.size() - 1];
        a.pop_back();
        if (!a.empty()) 
            siftDown(0);
        return top;
    }
    const T& top() const { 
        return a[0]; 
    }
    bool empty() const { 
        return a.empty(); 
    }
    int size() const { 
        return a.size(); 
    }
};
