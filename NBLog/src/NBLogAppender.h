//
// Created by liuenbao on 18-9-21.
//

#ifndef NBLOGAPPENDER_H
#define NBLOGAPPENDER_H

#include <NBLog.h>

//The appender context
typedef struct NBLogAppender {
    //The appender name
    const char* name;

    //The appender operation
    INBLogAppender logAppender;
    void* appenderData;

    //The appender list
    struct NBLogAppender* next;

    bool isOpened;
} NBLogAppender;

int logAppenderOpen(NBLogCtx* ctx, NBLogAppender* appender);
void logAppenderClose(NBLogCtx* ctx, NBLogAppender* appender);

#endif //NBLOGAPPENDER_H
