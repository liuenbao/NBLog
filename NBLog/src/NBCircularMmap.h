//
// Created by liu enbao on 2019-05-01.
//

#ifndef NBCIRCULARBUFFER_H
#define NBCIRCULARBUFFER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct CircularMmap CircularMmap;

CircularMmap* circularMmapNew(const char* mmaPath, uint16_t capacity);
void circularMmapDelete(CircularMmap* mmap);
uint16_t circularMmapSize(CircularMmap* cMmap);
uint16_t circularMmapCapacity(CircularMmap* cMmap);
uint16_t circularMmapPeakSize(CircularMmap* cMmap);
bool circularMmapEmpty(CircularMmap* cMmap);
bool circularMmapFull(CircularMmap* cMmap);
void circularMmapClear(CircularMmap* cMmap);
uint16_t circularMmapPeek(CircularMmap* cMmap, void* data, uint16_t size);
void circularMmapForward(CircularMmap* cMmap, uint16_t size);
bool circularMmapPush(CircularMmap* cMmap, const void* data, uint16_t size);

#endif //NBCIRCULARBUFFER_H
