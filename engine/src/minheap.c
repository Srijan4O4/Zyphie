#include "minheap.h"
#include "stdio.h"
#include "stdlib.h"

MinHeapNode* newNode(char data, unsigned freq){
  MinHeapNode* temp = (MinHeapNode*) malloc (sizeof(MinHeapNode));
  temp->left = temp->right = NULL;
  temp->data=data;
  temp->freq=freq;
  return temp;
}

MinHeap* createMinHeap(unsigned capacity){
  MinHeap* minHeap=(MinHeap*)malloc(sizeof(MinHeap));
  minHeap->size=0;
  minHeap->capacity=capacity;
  minHeap->array= (MinHeapNode**)malloc(minHeap->capacity*sizeof(MinHeapNode*));
  return minHeap;
}