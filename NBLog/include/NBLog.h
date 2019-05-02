//
// Created by liuenbao on 18-9-21.
//

#ifndef NBLOG_H
#define NBLOG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

//The global log context
typedef struct NBLogCtx NBLogCtx;

// log with mmap file appender , this mode need log_root must not be writable
#define NBLOG_MODE_MFILE       0x01
// log with console appender
#define NBLOG_MODE_CONSOLE     0x02

#ifndef NBLOG_DISABLED
#define NBLOG_INIT(config) nb_log_init(config)
#define NBLOG_DEINIT() nb_log_deinit()
#define NBLOG_VERBOSE(tag, fmt, args...) nb_log_print(NBLOG_PRI_VERBOSE, tag, fmt, ##args)
#define NBLOG_DEBUG(tag, fmt, args...) nb_log_print(NBLOG_PRI_DEBUG, tag, fmt, ##args)
#define NBLOG_INFO(tag, fmt, args...) nb_log_print(NBLOG_PRI_INFO, tag, fmt, ##args)
#define NBLOG_WARN(tag, fmt, args...) nb_log_print(NBLOG_PRI_WARN, tag, fmt, ##args)
#define NBLOG_ERROR(tag, fmt, args...) nb_log_print(NBLOG_PRI_ERROR, tag, fmt, ##args)
#define NBLOG_FATAL(tag, fmt, args...) nb_log_print(NBLOG_PRI_FATAL, tag, fmt, ##args)
#else
#define NBLOG_INIT(config)
#define NBLOG_DEINIT()
#define NBLOG_VERBOSE(tag, fmt...)
#define NBLOG_DEBUG(tag, fmt...)
#define NBLOG_INFO(tag, fmt...)
#define NBLOG_WARN(tag, fmt...)
#define NBLOG_ERROR(tag, fmt...)
#define NBLOG_FATAL(tag, fmt...)
#endif

/**
* NBLog priority values, in ascending priority order.
*/
typedef enum NBLogPriority {
    NBLOG_PRI_UNKNOWN = 0,
    NBLOG_PRI_DEFAULT,    /* only for SetMinPriority() */
    NBLOG_PRI_VERBOSE,
    NBLOG_PRI_DEBUG,
    NBLOG_PRI_INFO,
    NBLOG_PRI_WARN,
    NBLOG_PRI_ERROR,
    NBLOG_PRI_FATAL,
    NBLOG_PRI_SILENT,     /* only for SetMinPriority(); must be last */
} NBLogPriority;

//The log message define
typedef struct LogMessage {
    //the log message time value
    struct timeval msg_time;

    NBLogPriority priority;

    const char* tag;
    const char* msg;
    int msg_size;
} LogMessage;

typedef struct LogConfig {
    // the log root value
    const char* logRoot;

    // the log config mode
    uint32_t logMode;

    // the log max size
    uint32_t logMaxFileSize;

    // the log max size per log
    uint32_t logMaxPerFileSize;

    // the max file count
    uint32_t logMaxFileCount;
} LogConfig;

typedef struct INBLogAppender {
    void* (*open)(NBLogCtx* ctx);
    void (*close)(NBLogCtx* ctx, void* apdCtx);
    int (*append)(NBLogCtx* ctx, void* apdCtx, const LogMessage* message);
} INBLogAppender;

/**
 * NBLog register appender
 */
int nb_log_reg_appender(const char* name, INBLogAppender* appender);

/**
 * NBLog unregister appender
 */
void nb_log_unreg_appender(const char* name);

/**
 * NBLog init function
 * @param configure file path
 * @mode the log configure mode, see above
 * @return
 */
NBLogCtx* nb_log_init(const LogConfig* config);

/**
 * NBLog deinit function
 */
void nb_log_deinit();

/**
 * NBLog get function
 * return the global log instance
 */
NBLogCtx* nb_log_instance();

/**
 * NBLog get log root
 */
const char* nb_log_get_root();

/**
 * Set the log priority
 * @param ctx
 * @param logPriority
 */
void nb_log_set_priority(NBLogPriority logPriority);

/**
 * Send a simple string to the log.
 */
int nb_log_write(int prio, const char *tag, const char *text);

/**
 * Send a formatted string to the log, used like printf(fmt,...)
 */
int nb_log_print(int prio, const char *tag,  const char *fmt, ...)
#if defined(__GNUC__)
__attribute__ ((format(printf, 3, 4)))
#endif
;

/**
 * A variant of __android_log_print() that takes a va_list to list
 * additional parameters.
 */
int nb_log_vprint(int prio, const char *tag,
                         const char *fmt, va_list ap);


/**
 * Log an assertion failure and SIGTRAP the process to have a chance
 * to inspect it, if a debugger is attached. This uses the FATAL priority.
 */
void nb_log_assert(const char *cond, const char *tag,
                          const char *fmt, ...)
#if defined(__GNUC__)
__attribute__ ((noreturn))
__attribute__ ((format(printf, 3, 4)))
#endif
;

#ifdef __cplusplus
}
#endif

#endif //NBLOG_H
