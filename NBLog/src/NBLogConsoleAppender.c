//
// Created by liuenbao on 18-9-21.
//

#include "NBLogPriv.h"

#if BUILD_TARGET_ANDROID
#include <android/log.h>
#else
#include <stdio.h>
#endif

int console_appender_append(NBLogCtx* ctx, void* apdCtx, const LogMessage* message) {
#if __ANDROID__
    switch(message->priority) {
        case NBLOG_PRI_VERBOSE:
            __android_log_print(ANDROID_LOG_VERBOSE, message->tag, "%.*s", message->msg_size, message->msg);
            break;
        case NBLOG_PRI_DEBUG:
            __android_log_print(ANDROID_LOG_DEBUG, message->tag, "%.*s", message->msg_size, message->msg);
            break;
        case NBLOG_PRI_INFO:
            __android_log_print(ANDROID_LOG_INFO, message->tag, "%.*s", message->msg_size, message->msg);
            break;
        case NBLOG_PRI_WARN:
            __android_log_print(ANDROID_LOG_WARN, message->tag, "%.*s", message->msg_size, message->msg);
            break;
        case NBLOG_PRI_ERROR:
            __android_log_print(ANDROID_LOG_ERROR, message->tag, "%.*s", message->msg_size, message->msg);
            break;
        case NBLOG_PRI_FATAL:
            __android_log_print(ANDROID_LOG_FATAL, message->tag, "%.*s", message->msg_size, message->msg);
            break;
    }
#else
    struct tm *ptm = NULL;
    time_t timep = message->msg_time.tv_sec;
    ptm = localtime(&timep);

    fprintf(stdout, "[%04d-%02d-%02d_%02d:%02d:%02d.%03d] %s <%s>: %.*s\n",
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
#endif
    return 0;
}

INBLogAppender logConsoleAppender = {
        .append = console_appender_append,
};
