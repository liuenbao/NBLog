#include <iostream>
#include <NBLog.h>
#include <unistd.h>

#define LOG_TAG "Main"

int main() {
    LogConfig config = {0};
    config.logRoot = ".";
    config.logMode = NBLOG_MODE_CONSOLE | NBLOG_MODE_MFILE;

    NBLOG_INIT(&config);

    while (1) {
        NBLOG_INFO(LOG_TAG, "HelloWorld : %d", 3);
        usleep(10000);
    }

    NBLOG_DEINIT();

    return 0;
}