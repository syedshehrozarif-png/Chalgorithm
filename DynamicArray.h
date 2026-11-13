
#pragma once
#include <iostream>
#include <cstdlib>
using namespace std;

template <typename T>
class DynamicArray {
    T* data;
    int sz;
    int cap;

    void grow()
    {
        int newCap = cap * 2;
        T* nd = new T[newCap];
        for (int i = 0; i < sz; i++) nd[i] = data[i];
        delete[] data;
        data = nd;
        cap = newCap;
    }
public:
    DynamicArray(int c = 4) : data(new T[c > 0 ? c : 1]), sz(0), cap(c > 0 ? c : 1) 
    {

    }

    DynamicArray(const DynamicArray& o) : data(new T[o.cap]), sz(o.sz), cap(o.cap) 
    
    {
        for (int i = 0; i < sz; i++) data[i] = o.data[i];
    }
    DynamicArray& operator=(const DynamicArray& o) 
    {
        if (this != &o) 
        {
            T* nd = new T[o.cap];
            for (int i = 0; i < o.sz; i++) nd[i] = o.data[i];

            delete[] data;

            data = nd;
            sz = o.sz;
            cap = o.cap;
        }
        return *this;
    }
    ~DynamicArray()
    { 
        delete[] data; 
    }

    void push_back(const T& x) 
    { 
        if (sz == cap) 
        grow(); 
        
        data[sz++] = x; 
    }

    void pop_back() 
    { 
        if (sz > 0)
        sz--; 
    }

    void removeAt(int i) 
    {
        check(i);

        for (int k = i; k < sz - 1; k++) 
        data[k] = data[k + 1];

        sz--;
    }
    void reverse() {

        for (int i = 0, j = sz - 1; i < j; i++, j--) 
        { 
            T t = data[i]; 
            data[i] = data[j]; 
            data[j] = t; 
        }
    }

    void clear() 
    { 
        sz = 0; 
    }

    int size() const 

    { 
        return sz; 
    }

    bool empty() const 
    { 
        return sz == 0;
    }

    T& operator[](int i) 
    { 
        check(i);
        return data[i]; 
    }

    const T& operator[](int i) const 
    { 
        check(i); 
        return data[i]; 
    }

private:
    void check(int i) const 
    {
        if (i < 0 || i >= sz) 
        { 
            cerr << "DynamicArray: index " << i << " out of range\n"; 
            exit(1); 
        }
    }
};
