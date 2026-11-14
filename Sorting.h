#pragma once
#include "DynamicArray.h"
#include <iostream>
using namespace std;


/*
    All below mentioned sorting algorithms use a pointer function
    bool (*lt)(const T&, const T&) which is a comparison function 
    passed as an argument to the sorting function. This function returns a boolean
    and can be of any nature, namely ascending, descending, or any
    custom comparison logic defined by the user. Each sorting algorithm
    use this function to compare two elements of the array and determine 
    their order in the sorted array. 
*/
template <typename T>
void bubbleSort(DynamicArray<T>& a, bool (*lt)(const T&, const T&)) {
    int n = a.size();
    for (int i = 0; i < n - 1; i++) {
        bool swapped = false;
        for (int j = 0; j < n - 1 - i; j++)
            if (lt(a[j + 1], a[j])) { 
                T t = a[j]; 
                a[j] = a[j + 1]; 
                a[j + 1] = t; 
                swapped = true; 
            }
        if (!swapped) break;
    }
}

template <typename T>
void insertionSort(DynamicArray<T>& a, bool (*lt)(const T&, const T&)) {
    for (int i = 1; i < a.size(); i++) {
        T key = a[i];
        int j = i - 1;
        while (j >= 0 && lt(key, a[j])) 
        { 
            a[j + 1] = a[j]; 
            j--; 
        }
        a[j + 1] = key;
    }
}

template <typename T>
void mergeRec(DynamicArray<T>& a, DynamicArray<T>& tmp, int lo, int hi, bool (*lt)(const T&, const T&)) {
    if (lo >= hi) 
        return;
    int mid = (lo + hi) / 2;
    mergeRec(a, tmp, lo, mid, lt);
    mergeRec(a, tmp, mid + 1, hi, lt);
    int i = lo;
    int j = mid + 1;
    int k = lo;
    while (i <= mid && j <= hi) 
        tmp[k++] = lt(a[j], a[i]) ? a[j++] : a[i++]; 
    while (i <= mid) 
        tmp[k++] = a[i++];
    while (j <= hi) 
        tmp[k++] = a[j++];  
    for (k = lo; k <= hi; k++) 
        a[k] = tmp[k];
}
template <typename T>
void mergeSort(DynamicArray<T>& a, bool (*lt)(const T&, const T&)) {
    DynamicArray<T> tmp;
    for (int i = 0; i < a.size(); i++) 
        tmp.push_back(a[i]);
    mergeRec(a, tmp, 0, a.size() - 1, lt);
}

template <typename T>
void quickRec(DynamicArray<T>& a, int lo, int hi, bool (*lt)(const T&, const T&)) {
    if (lo >= hi) return;
    T p = a[(lo + hi) / 2];
    int i = lo;
    int j = hi;
    while (i <= j) {
        while (lt(a[i], p)) 
            i++;
        while (lt(p, a[j])) 
            j--;
        if (i <= j) { 
            T t = a[i]; 
            a[i] = a[j]; 
            a[j] = t; 
            i++; 
            j--; 
        }
    }
    quickRec(a, lo, j, lt);
    quickRec(a, i, hi, lt);
}
template <typename T>
void quickSort(DynamicArray<T>& a, bool (*lt)(const T&, const T&)) 
{ 
    quickRec(a, 0, a.size() - 1, lt); 
}

inline void radixSort(DynamicArray<int>& a) {
    int n = a.size();
    if (n < 2) 
        return;
    int mx = a[0];
    for (int i = 1; i < n; i++) 
        if (a[i] > mx) 
            mx = a[i];
    int* out = new int[n];
    for (int exp = 1; mx / exp > 0; exp *= 10) {
        int count[10] = {0};
        for (int i = 0; i < n; i++) 
            count[(a[i] / exp) % 10]++;
        for (int d = 1; d < 10; d++) 
            count[d] += count[d - 1];
        for (int i = n - 1; i >= 0; i--) 
            out[--count[(a[i] / exp) % 10]] = a[i];
        for (int i = 0; i < n; i++) 
            a[i] = out[i];
    }
    delete[] out;
}
