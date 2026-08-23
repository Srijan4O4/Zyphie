#ifndef MINHEAP_H
#define MINHEAP_H

typedef struct MinHeapNode
{
  char data;
  unsigned freq;
  struct MinHeapNode *left, *right;
} MinHeapNode;

typedef struct MinHeap
{
  unsigned size;
  unsigned capacity;
  struct MinHeapNode **array;
} MinHeap;

MinHeapNode *newNode(char data, unsigned freq);
MinHeap *createMinHeap(unsigned capacity);
void swapMinHeapNode(MinHeapNode **a, MinHeapNode **b);
void minHeapify(MinHeap *minHeap, int idx);
MinHeapNode *extractMin(MinHeap *minHeap);
void insertMinHeap(MinHeap *minHeap, MinHeapNode *minHeapNode);

#endif
