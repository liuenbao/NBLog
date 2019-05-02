//
// Created by liuenbao on 18-9-21.
//

#include "NBLogPriv.h"
#include "libae/ae.h"

#include <NBLog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <dirent.h>
#include "NBCircularMmap.h"

#define MAX_LOOP_SIZE 10
#define TIME_IN_MILLISECOND 2000
#define MAX_DIRECTORY_COUNT 3

// the mmap length is 32K
#define MAX_MMAP_SIZE       32768
#define MAX_RECORD_SIZE     4096
#define MAX_PURGE_RECORD_COUNT    100

#define ASYNC_EVENT_TYPE_STOP   0xFF
#define ASYNC_EVENT_TYPE_PURGE  0x00

#define CIRCULAR_MMAP_FILENAME "core.mmap"

typedef struct LogFileCtx {
    int currentFd;
    char currentFile[FILE_MAX];

    char logRoot[FILE_MAX];
    char logDir[FILE_MAX];
    pthread_t bgThread;
    pthread_mutex_t log_mutex;

    // the event loop
    aeEventLoop* aeLoop;
    int asyncPipe[2];

    CircularMmap* circularMmap;
} LogFileCtx;

static void* BackgoundThreadLoop(void* params);
static void purgeMmapDataToFile(LogFileCtx* fileCtx);

void* file_appender_open(NBLogCtx* ctx) {
    char mapFileName[FILENAME_MAX] = {0};
    LogFileCtx* fileCtx = NULL;
    int lastPos = 0;
    const char* log_root = nb_log_get_root(ctx);
    if (log_root == NULL) {
        return NULL;
    }
    lastPos = strlen(log_root);

    fileCtx = (LogFileCtx*)malloc(sizeof(LogFileCtx));
    if (fileCtx == NULL) {
        return fileCtx;
    }
    memset(fileCtx, 0, sizeof(LogFileCtx));

    strncpy(fileCtx->logRoot, log_root, lastPos);
    if (fileCtx->logRoot[lastPos - 1] != '/') {
        fileCtx->logRoot[lastPos] = '/';
    }

    fileCtx->currentFd = -1;

    strcpy(mapFileName, fileCtx->logRoot);
    strcat(mapFileName, CIRCULAR_MMAP_FILENAME);
    fileCtx->circularMmap = circularMmapNew(mapFileName, MAX_MMAP_SIZE);

    fileCtx->aeLoop = aeCreateEventLoop(MAX_LOOP_SIZE);
    pipe(fileCtx->asyncPipe);

    pthread_mutex_init(&fileCtx->log_mutex, NULL);

    pthread_create(&fileCtx->bgThread, NULL, BackgoundThreadLoop, fileCtx);

    return fileCtx;
}

void file_appender_close(NBLogCtx* ctx, void* apdCtx) {
    LogFileCtx* fileCtx = (LogFileCtx*)apdCtx;
    if (fileCtx == NULL) {
        return ;
    }

    uint8_t stopSignal = ASYNC_EVENT_TYPE_STOP;
    write(fileCtx->asyncPipe[1], &stopSignal, 1);

    pthread_join(fileCtx->bgThread, NULL);

    pthread_mutex_destroy(&fileCtx->log_mutex);

    aeDeleteEventLoop(fileCtx->aeLoop);

    close(fileCtx->asyncPipe[0]);
    close(fileCtx->asyncPipe[1]);

    circularMmapDelete(fileCtx->circularMmap);

    free(fileCtx);
}

static ssize_t nb_fprintf(CircularMmap* circularMmap, const void *fmt, ...)
{
    ssize_t n;
    char *p = NULL;

    va_list args;
    va_start(args, fmt);
    size_t len  = nb_vscprintf(fmt, args);
    p = alloca(len + 1);
    vsnprintf(p, len + 1, fmt, args);
    p[len] = '\0';
    va_end(args);

    circularMmapPush(circularMmap, p, len);

    return len;
}

int file_appender_append(NBLogCtx* ctx, void* apdCtx, const LogMessage* message) {
    LogFileCtx* fileCtx = (LogFileCtx*)apdCtx;
    if (fileCtx == NULL) {
        return -1;
    }

    if (fileCtx->circularMmap == NULL) {
        return -1;
    }

    struct tm *ptm = NULL;

    time_t timep = message->msg_time.tv_sec;
    ptm = localtime(&timep);

    nb_fprintf(fileCtx->circularMmap, "[%04d-%02d-%02d_%02d:%02d:%02d.%03d] %s <%s>: %.*s\n",
            ptm->tm_year + 1900,
            ptm->tm_mon + 1,
            ptm->tm_mday,
            ptm->tm_hour,
            ptm->tm_min,
            ptm->tm_sec,
            message->msg_time.tv_usec / 1000,
            message->tag ? message->tag : "none",
            getPriorityLable(message->priority),
            message->msg_size,
            message->msg);

    // try to purge the mmap data to file if mmap is half enough
    if (circularMmapSize(fileCtx->circularMmap) >= circularMmapCapacity(fileCtx->circularMmap) / 2) {
        uint8_t eventType = ASYNC_EVENT_TYPE_PURGE;
        write(fileCtx->asyncPipe[1], &eventType, 1);
    }
    return 0;
}

INBLogAppender logMFileAppender = {
        .open = file_appender_open,
        .append = file_appender_append,
        .close = file_appender_close,
};

/////////////////////////// event loop thread begin //////////////////////////////

static int remove_directory(const char *dirname) {
    DIR *dir = NULL;
    struct dirent *entry = NULL;
    char path[FILE_MAX] = {0};

    dir = opendir(dirname);
    if (dir == NULL) {
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
            snprintf(path, (size_t) FILE_MAX, "%s/%s", dirname, entry->d_name);
            if (entry->d_type == DT_DIR) {
                remove_directory(path);
            } else {
                // delete file
                unlink(path);
            }
        }
    }
    closedir(dir);

    // now we can delete the empty dir
    rmdir(dirname);
    return 0;
}

// delete directory if have more than three day log directory
static int removeDirectoryIfNeeded(LogFileCtx* fileCtx) {
    DIR* dir = NULL;
    struct dirent* dirt = NULL;
    int dirtCount = 0;
    char path[FILE_MAX] = {0};

    dir = opendir(fileCtx->logRoot);
    while((dirt = readdir(dir)) != NULL) {
        if(strcmp(dirt->d_name,".")==0||strcmp(dirt->d_name,"..")==0)
            continue;

        ++ dirtCount;
        printf("d_name : %s", dirt->d_name);
    }

    seekdir(dir, 0);

    while((dirt = readdir(dir)) != NULL) {
        if(strcmp(dirt->d_name,".")==0||strcmp(dirt->d_name,"..")==0)
            continue;

        if (dirtCount > MAX_DIRECTORY_COUNT) {
            snprintf(path, (size_t) FILE_MAX, "%s/%s", fileCtx->logRoot, dirt->d_name);
            remove_directory(path);
        } else {
            break;
        }
    }

    closedir(dir);
    return 0;
}

static int createDirectoryIfNeeded(LogFileCtx* fileCtx, struct tm* ptm) {
    strcpy(fileCtx->logDir, fileCtx->logRoot);
    int lastPos = strlen(fileCtx->logDir);
    sprintf(fileCtx->logDir + lastPos, "%04d-%02d-%02d/", ptm->tm_year, ptm->tm_mon, ptm->tm_mday);

    if (access(fileCtx->logDir, F_OK) != 0) {
        mkdir(fileCtx->logDir, S_IRWXU | S_IRWXG | S_IRWXO);

        // remove the file directory if needed
        removeDirectoryIfNeeded(fileCtx);
    }
    return 0;
}

static int createCurrentFileIfNeeded(LogFileCtx* fileCtx, struct tm* ptm) {
    strcpy(fileCtx->currentFile, fileCtx->logDir);
    int lastPos = strlen(fileCtx->currentFile);

    sprintf(fileCtx->currentFile + lastPos, "%02d.log", ptm->tm_hour);

    if (access(fileCtx->currentFile, F_OK) != 0) {
        if (fileCtx->currentFd != -1) {
            close(fileCtx->currentFd);
        }

        // Todo
        // remove the file if needed

        fileCtx->currentFd = open(fileCtx->currentFile, O_RDWR | O_CREAT, 0644);
    } else {
        if (fileCtx->currentFd == -1) {
            fileCtx->currentFd = open(fileCtx->currentFile, O_RDWR | O_APPEND, 0644);
        }
    }
    return 0;
}

static int wirteCurrentLogBuffer(LogFileCtx* fileCtx, uint8_t* recordBuffer, uint16_t reacordSize) {
    struct tm tm = {0};
    long millisecond = 0;
    sscanf((const char*)recordBuffer, "[%04d-%02d-%02d_%02d:%02d:%02d.%03ld]",
           &tm.tm_year,
           &tm.tm_mon,
           &tm.tm_mday,
           &tm.tm_hour,
           &tm.tm_min,
           &tm.tm_sec,
           &millisecond);

    createDirectoryIfNeeded(fileCtx, &tm);

    createCurrentFileIfNeeded(fileCtx, &tm);

    size_t left = reacordSize;
    size_t step = 4 * 1024;
    int n;
    if (fileCtx->currentFd == -1 || recordBuffer == NULL || left == 0) {
        printf("%s paraments invalid, ", __func__);
        return -1;
    }
    while (left > 0) {
        if (left < step)
            step = left;
        n = write(fileCtx->currentFd, (void *)recordBuffer, step);
        if (n > 0) {
            recordBuffer += n;
            left -= n;
            continue;
        } else if (n == 0) {
            break;
        }
        if (errno == EINTR || errno == EAGAIN) {
            continue;
        } else {
            printf("write failed: %d\n", errno);
            break;
        }
    }
    return (reacordSize - left);
}

static void purgeMmapDataToFile(LogFileCtx* fileCtx) {
    uint8_t recordBuffer[MAX_RECORD_SIZE] = {0};
    int recordCount = 0;

    while (++recordCount < MAX_PURGE_RECORD_COUNT) {
        uint16_t recordSize = circularMmapPeek(fileCtx->circularMmap, recordBuffer, MAX_RECORD_SIZE);

        if (recordSize <= 0) {
            break;
        }

        wirteCurrentLogBuffer(fileCtx, recordBuffer, recordSize);

        circularMmapForward(fileCtx->circularMmap, recordSize);
    }
}

static int fileTimerCallback(struct aeEventLoop *loop, long long id, void *clientData) {
    LogFileCtx* fileCtx = (LogFileCtx*)clientData;
    purgeMmapDataToFile(fileCtx);
    // return value is call back in millisecond
    return TIME_IN_MILLISECOND;
}

static void eventAsyncHandler(aeEventLoop *loop, int fd, void *clientdata, int mask) {
    LogFileCtx* fileCtx = (LogFileCtx*)clientdata;
    uint8_t eventType = 0;
    if (read(fd, &eventType, 1) <= 0) {
        return ;
    }

    switch (eventType) {
        case ASYNC_EVENT_TYPE_STOP:
            aeStop(loop);
            break;
        case ASYNC_EVENT_TYPE_PURGE:
            purgeMmapDataToFile(fileCtx);
            break;
        default:
            break;
    }
}

static void* BackgoundThreadLoop(void* params) {
    LogFileCtx* fileCtx = (LogFileCtx*)params;

    long long timeId = aeCreateTimeEvent(fileCtx->aeLoop, TIME_IN_MILLISECOND, fileTimerCallback, fileCtx, NULL);

    aeCreateFileEvent(fileCtx->aeLoop, fileCtx->asyncPipe[0], AE_READABLE, eventAsyncHandler, fileCtx);

    aeMain(fileCtx->aeLoop);

    aeDeleteFileEvent(fileCtx->aeLoop, fileCtx->asyncPipe[0], AE_READABLE);

    aeDeleteTimeEvent(fileCtx->aeLoop, timeId);

    return NULL;
}

/////////////////////////// event loop thread end //////////////////////////////
