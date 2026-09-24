#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>

// Initialize WiFi, Captive Portal, and Web Server
void web_server_init();

// Must be called in a loop to process DNS requests for Captive Portal
void web_server_loop();

#endif // WEB_SERVER_H
