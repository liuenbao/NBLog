//
// Created by liuenbao on 18-9-21.
//

#include "NBLog.h"
#include "NBLogPriv.h"
#include "NBLogAppender.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/time.h>

#define DEFAULT_LOG_PRIORITY NBLOG_PRI_DEFAULT

//NBLOG_UNKNOWN = 0,
//NBLOG_DEFAULT,    /* only for SetMinPriority() */
//NBLOG_VERBOSE,
//NBLOG_DEBUG,
//NBLOG_INFO,
//NBLOG_WARN,
//NBLOG_ERROR,
//NBLOG_FATAL,
//NBLOG_SILENT,     /* only for SetMinPriority(); must be last */

const char* arrPriorityLabel[] = {
        "UNKNOWN",
        "DEFAULT",
        "VERBOSE",
        "DEBUG",
        "INFO",
        "WARN",
        "ERROR",
        "FATAL",
        "SLIENT"
};

static NBLogCtx* global_logCtx = NULL;

/**
 * NBLog init function
 * @param configure file path
 * @return
 */
NBLogCtx* nb_log_init(const LogConfig* config) {
    NBLogCtx* ctx = malloc(sizeof(NBLogCtx));
    if (ctx == NULL) {
        return ctx;
    }
    memset(ctx, 0, sizeof(struct NBLogCtx));
    ctx->logPriority = DEFAULT_LOG_PRIORITY;

    if (config->logRoot != NULL) {
        ctx->log_root = strdup(config->logRoot);
    }

    // Save the global log context
    global_logCtx = ctx;

    if (config->logMode & NBLOG_MODE_MFILE) {
        if (config->logRoot != NULL) {
            extern INBLogAppender logMFileAppender;
            nb_log_reg_appender("file", &logMFileAppender);
        }
    }

    if (config->logMode & NBLOG_MODE_CONSOLE) {
        extern INBLogAppender logConsoleAppender;
        nb_log_reg_appender("console", &logConsoleAppender);
    }

    ctx->log_max_filesize = config->logMaxFileSize;
    ctx->log_max_per_filesize = config->logMaxPerFileSize;
    ctx->log_max_file_count = config->logMaxFileCount;

    return ctx;
}

NBLogCtx* nb_log_instance() {
    return global_logCtx;
}

/**
 * NBLog deinit function
 */
void nb_log_deinit() {
    NBLogCtx* ctx = nb_log_instance();
    if (ctx == NULL) {
        return ;
    }

    for (NBLogAppender* currAppender = ctx->appender_list;
         currAppender != NULL; currAppender = currAppender->next) {

        //Open the appender if needed
        if (currAppender->isOpened) {
            logAppenderClose(ctx, currAppender);
        }

        nb_log_unreg_appender(currAppender->name);
    }

    if (ctx->log_root != NULL) {
        free((void*)ctx->log_root);
    }

    free(ctx);

    global_logCtx = NULL;
}

/**
 * NBLog get log root
 */
const char* nb_log_get_root(NBLogCtx* ctx) {
    return ctx->log_root;
}

/**
 * Set the log priority
 * @param ctx
 * @param logPriority
 */
void nb_log_set_priority(NBLogPriority logPriority) {
    nb_log_instance()->logPriority = logPriority;
}

/*
 * Send a simple string to the log.
 */
int nb_log_write(int prio, const char *tag, const char *text) {
    return nb_log_print(prio, tag, "%s", text);
}

/*
 * Send a formatted string to the log, used like printf(fmt,...)
 */
int nb_log_print(int prio, const char *tag,  const char *fmt, ...) {
    int rc = 0;
    va_list arg;
    va_start(arg, fmt);
    rc = nb_log_vprint(prio, tag, fmt, arg);
    va_end(arg);
    return rc;
}

/*
 * A variant of __android_log_print() that takes a va_list to list
 * additional parameters.
 */
int nb_log_vprint(int prio, const char *tag,
                  const char *fmt, va_list ap) {
    // Do not need print it
    if (nb_log_instance() == NULL || prio < nb_log_instance()->logPriority) {
        return -1;
    }

    LogMessage message = {0};

    // get current message time
    gettimeofday(&message.msg_time, NULL);

    int act_size = nb_vscprintf(fmt, ap);
    char* msg = alloca(act_size + 1);
    vsnprintf(msg, act_size + 1, fmt, ap);
    msg[act_size] = '\0';
    message.msg_size = act_size;
    message.msg = msg;
    message.priority = prio;
    message.tag = tag;

    for (NBLogAppender* currAppender = nb_log_instance()->appender_list;
                currAppender != NULL; currAppender = currAppender->next) {

        INBLogAppender* iAppender = &currAppender->logAppender;

        //Open the appender if needed
        if (!currAppender->isOpened) {
            logAppenderOpen(nb_log_instance(), currAppender);
        }

        iAppender->append(nb_log_instance(), currAppender->appenderData, &message);
    }
    return 0;
}


/*
 * Log an assertion failure and SIGTRAP the process to have a chance
 * to inspect it, if a debugger is attached. This uses the FATAL priority.
 */
void nb_log_assert(const char *cond, const char *tag,
                   const char *fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    nb_log_vprint(NBLOG_PRI_FATAL, tag, fmt, arg);
    va_end(arg);

    assert(0);
}
