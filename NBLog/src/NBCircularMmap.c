//
// Created by liu enbao on 2019-05-01.
//

#include "NBCircularMmap.h"
#include <stdatomic.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

// 2 byte for capacity, 2 byte for readIndex, 2 byte for writeIndex
#define MMAP_HEADER_SIZE 6

struct CircularMmap {
    atomic_ushort mReadIndex;
    atomic_ushort mWriteIndex;
    uint16_t mCapacity;

    char mMapFileName[PATH_MAX];
    uint8_t* mMapAddress;
    uint8_t* mBuffer;
    uint16_t mPeakSize;
    int mMapFd;
};

static uint16_t circularMmapReadUShort(uint8_t* mmapAddr, uint16_t off) {
    uint16_t value = 0;
    value = mmapAddr[off] << 8 | mmapAddr[off + 1];
    return value;
}

static void circularMmapWriteUShort(uint8_t* mmapAddr, uint16_t off, uint16_t value) {
    mmapAddr[off] = value >> 8;
    mmapAddr[off + 1] = value & 0xFF;
}

void circularMmapReadData(CircularMmap* cMmap, uint16_t readIndex, uint8_t* data, uint16_t size) {
    if (readIndex + size < cMmap->mCapacity) {
        memcpy(data, cMmap->mBuffer + readIndex, size);
    } else {
        uint16_t partialSize = (cMmap->mCapacity - readIndex);
        memcpy(data, cMmap->mBuffer + readIndex, partialSize);
        memcpy(data + partialSize, cMmap->mBuffer, size - partialSize);
    }
}

void circularMmapWriteData(CircularMmap* cMmap, uint16_t writeIndex, const uint8_t* data, uint16_t size) {
    if (writeIndex + size < cMmap->mCapacity) {
        memcpy(cMmap->mBuffer + writeIndex, data, size);
    } else {
        uint16_t partialSize = (cMmap->mCapacity - writeIndex);
        memcpy(cMmap->mBuffer + writeIndex, data, partialSize);
        memcpy(cMmap->mBuffer, data + partialSize, size - partialSize);
    }
}

void circularMmapCalcPeakSize(CircularMmap* cMmap, uint16_t readIndex, uint16_t writeIndex) {
    uint16_t size;
    if (writeIndex >= readIndex) {
        size = (writeIndex - readIndex);
    } else {
        size = (cMmap->mCapacity - (readIndex - writeIndex));
    }
    if (size > cMmap->mPeakSize) {
        cMmap->mPeakSize = size;
    }
}

CircularMmap* circularMmapNew(const char* mmapFileName, uint16_t capacity) {
    CircularMmap* cMmap = (CircularMmap*)malloc(sizeof(CircularMmap));
    if (cMmap == NULL) {
        return NULL;
    }
    memset(cMmap, 0, sizeof(*cMmap));

    cMmap->mCapacity = capacity;

    strcpy(cMmap->mMapFileName, mmapFileName);

    if (access(cMmap->mMapFileName, F_OK)) {
        cMmap->mMapFd = open(cMmap->mMapFileName, O_RDWR | O_CREAT);
        ftruncate(cMmap->mMapFd, cMmap->mCapacity + MMAP_HEADER_SIZE);
    } else {
        cMmap->mMapFd = open(cMmap->mMapFileName, O_RDWR);
    }

    if (cMmap->mMapFd == -1) {
        free(cMmap);
        return NULL;
    }

    cMmap->mMapAddress = mmap(NULL, cMmap->mCapacity + MMAP_HEADER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, cMmap->mMapFd, 0);
    if (cMmap->mMapAddress == NULL) {
        close(cMmap->mMapFd);
        free(cMmap);
        return NULL;
    }

    cMmap->mBuffer = cMmap->mMapAddress + MMAP_HEADER_SIZE;
//    cMmap->mCapacity = circularMmapReadUShort(cMmap->mMapAddress, 0);
    atomic_store(&cMmap->mReadIndex, circularMmapReadUShort(cMmap->mMapAddress, 2));
    atomic_store(&cMmap->mWriteIndex, circularMmapReadUShort(cMmap->mMapAddress, 4));

    circularMmapCalcPeakSize(cMmap, atomic_load(&cMmap->mReadIndex), atomic_load(&cMmap->mWriteIndex));

    return cMmap;
}

void circularMmapDelete(CircularMmap* cMmap) {
    if (cMmap == NULL) {
        return ;
    }

    munmap(cMmap->mMapAddress, cMmap->mCapacity + MMAP_HEADER_SIZE);

    close(cMmap->mMapFd);

    free(cMmap);
}

uint16_t circularMmapSize(CircularMmap* cMmap) {
    const uint16_t readIndex = atomic_load(&cMmap->mReadIndex);
    const uint16_t writeIndex = atomic_load(&cMmap->mWriteIndex);

    if (writeIndex >= readIndex) {
        return (writeIndex - readIndex);
    } else {
        return (cMmap->mCapacity - (readIndex - writeIndex));
    }
}

uint16_t circularMmapCapacity(CircularMmap* cMmap) {
    return cMmap->mCapacity;
}

uint16_t circularMmapPeakSize(CircularMmap* cMmap) {
    return cMmap->mPeakSize;
}

bool circularMmapEmpty(CircularMmap* cMmap) {
    return atomic_load(&cMmap->mReadIndex) == atomic_load(&cMmap->mWriteIndex);
}

bool circularMmapFull(CircularMmap* cMmap) {
    return (atomic_load(&cMmap->mWriteIndex) + 1) % cMmap->mCapacity == atomic_load(&cMmap->mReadIndex);
}

void circularMmapClear(CircularMmap* cMmap) {
    atomic_exchange(&cMmap->mReadIndex, 0);
    atomic_exchange(&cMmap->mWriteIndex, 0);
    cMmap->mPeakSize = 0;
}

uint16_t circularMmapPeek(CircularMmap* cMmap, void* data, uint16_t size) {
    if (cMmap == NULL) {
        return 0;
    }

    if (data == NULL) {
        return 0;
    }

    const uint16_t readIndex = atomic_load(&cMmap->mReadIndex);
    const uint16_t writeIndex = atomic_load(&cMmap->mWriteIndex);

    if (readIndex == writeIndex) {
        return 0;
    }

    uint16_t dataSize;
    circularMmapReadData(cMmap, readIndex, (uint8_t*) &dataSize, 2);
    if (size >= dataSize) {
        circularMmapReadData(cMmap, (readIndex + 2) % cMmap->mCapacity, (uint8_t*) data, dataSize);
        return dataSize;
    } else {
        return -dataSize;
    }
}

void circularMmapForward(CircularMmap* cMmap, uint16_t size) {
    if (cMmap == NULL) {
        return ;
    }

    const uint16_t readIndex = atomic_load(&cMmap->mReadIndex);
    const uint16_t writeIndex = atomic_load(&cMmap->mWriteIndex);

    if (readIndex == writeIndex) {
        return ;
    }

    uint16_t newReadIndex = (readIndex + size + 2) % cMmap->mCapacity;
    circularMmapWriteUShort(cMmap->mMapAddress, 2, newReadIndex);
    atomic_store(&cMmap->mReadIndex, newReadIndex);
}

bool circularMmapPush(CircularMmap* cMmap, const void* data, uint16_t size) {
    if (data == NULL) {
        return (size == 0);
    }
    if (size == 0) {
        return true;
    }
    if (cMmap->mCapacity <= (size + 2)) {
        return false;
    }

    const uint16_t readIndex = atomic_load(&cMmap->mReadIndex);
    const uint16_t writeIndex = atomic_load(&cMmap->mWriteIndex);

    bool hasFreeSpace;
    if (writeIndex >= readIndex) {
        hasFreeSpace = (cMmap->mCapacity - (writeIndex - readIndex)) > (size + 2);
    } else {
        hasFreeSpace = (readIndex - writeIndex) > (size + 2);
    }

    if (!hasFreeSpace) {
        return false;
    }

    uint16_t newWriteIndex = (writeIndex + size + 2) % cMmap->mCapacity;
    circularMmapWriteData(cMmap, writeIndex, (uint8_t*) &size, 2);
    circularMmapWriteData(cMmap, (writeIndex + 2) % cMmap->mCapacity, (uint8_t*) data, size);
    circularMmapWriteUShort(cMmap->mMapAddress, 4, newWriteIndex);
    atomic_store(&cMmap->mWriteIndex, newWriteIndex);
    circularMmapCalcPeakSize(cMmap, readIndex, newWriteIndex);
    return true;
}