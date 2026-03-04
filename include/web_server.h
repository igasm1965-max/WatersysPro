#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "config.h"

void initWebServer();
void restartWebServer();

// Called periodically from main loop
void webServerPeriodic();

#endif // WEB_SERVER_H