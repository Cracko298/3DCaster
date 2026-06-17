#include <3ds.h>

void __appInit(void) {
    srvInit();
    aptInit();
    hidInit();
    fsInit();
}

void __appExit(void) {
    fsExit();
    hidExit();
    aptExit();
    srvExit();
}
