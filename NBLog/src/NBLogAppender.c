//
// Created by liuenbao on 18-9-21.
//

#include "NBLogAppender.h"
#include "NBLogPriv.h"
#include <NBLog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * NBLog register appender
 */
int nb_log_reg_appender(const char* name, INBLogAppender* appender) {
    NBLogCtx* ctx = nb_log_instance();

    if (ctx == NULL) {
        return -1;
    }

    if (name == NULL) {
        return -1;
    }

    if (appender == NULL
        || (appender->open == NULL
            && appender->append == NULL
            && appender->close == NULL)) {
        return -1;
    }

    NBLogAppender* currAppender = ctx->appender_list;
    while (currAppender != NULL) {
        if (strcmp(currAppender->name, name) == 0) {
            break;
        }
        currAppender = currAppender->next;
    }

    //We already registed the same name appender
    if (currAppender != NULL) {
        return -1;
    }

    //add to the list first
    currAppender = (NBLogAppender*)malloc(sizeof(NBLogAppender));
    memset(currAppender, 0, sizeof(NBLogAppender));
    currAppender->isOpened = false;
    currAppender->name = strdup(name);
    currAppender->logAppender = *appender;

    //Link the to list
    if (ctx->appender_list == NULL) {
        currAppender->next = NULL;
        ctx->appender_list = currAppender;
    } else {
        currAppender->next = ctx->appender_list;
        ctx->appender_list = currAppender;
    }

    return 0;
}

/**
 * NBLog unregister appender
 */
void nb_log_unreg_appender(const char* name) {
    NBLogCtx* ctx = nb_log_instance();

    if (ctx == NULL) {
        return ;
    }

    if (ctx->appender_list == NULL) {
        return ;
    }

    NBLogAppender* currAppender = ctx->appender_list;
    NBLogAppender* prevAppender = NULL;
    while (currAppender != NULL) {
        if (strcmp(currAppender->name, name) == 0) {
            free((void*)currAppender->name);
            //Delete the listitem with name
            if (prevAppender == NULL) {
                ctx->appender_list = currAppender->next;
            } else {
                prevAppender->next = currAppender->next;
            }
            free(currAppender);
            break;
        }
        prevAppender = currAppender;
        currAppender = currAppender->next;
    }
}

//The open&close function
int logAppenderOpen(NBLogCtx* ctx, NBLogAppender* appender) {
    INBLogAppender* iLogAppender = &appender->logAppender;
    if (appender->isOpened) {
        return -1;
    }

    if (iLogAppender->open == NULL) {
        return -1;
    }

    if ((appender->appenderData = iLogAppender->open(ctx)) != NULL) {
        appender->isOpened = true;
    }

    return 0;
}

void logAppenderClose(NBLogCtx* ctx, NBLogAppender* appender) {
    INBLogAppender* iLogAppender = &appender->logAppender;
    if (!appender->isOpened) {
        return ;
    }

    if (iLogAppender->close != NULL) {
        iLogAppender->close(ctx, appender->appenderData);
    }

    appender->isOpened = false;
}