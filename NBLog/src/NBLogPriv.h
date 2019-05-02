//
// Created by liuenbao on 18-9-21.
//

#ifndef NBLOGPRIV_H
#define NBLOGPRIV_H

#include <NBLog.h>

//The appender context
typedef struct NBLogAppender NBLogAppender;

struct NBLogCtx {
    const char* log_root;

    uint32_t log_mode;

    uint32_t log_max_filesize;

    uint32_t log_max_per_filesize;

    uint32_t log_max_file_count;

    //The appender list
    NBLogAppender* appender_list;

    NBLogPriority logPriority;
};

// the current max file name
#define FILE_MAX    512

static inline int nb_vscprintf(const char * fmt, va_list pargs)
{
    int retval = 0;
    va_list argcopy;
    va_copy(argcopy, pargs);
    retval = vsnprintf(NULL, 0, fmt, argcopy);
    va_end(argcopy);
    return retval;
}

extern const char* arrPriorityLabel[];

static inline const char* getPriorityLable(NBLogPriority priority) {
    return arrPriorityLabel[priority];
}

#endif //NBLOGPRIV_H
