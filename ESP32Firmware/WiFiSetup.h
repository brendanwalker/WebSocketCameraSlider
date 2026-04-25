#ifndef WIFI_SETUP_H__
#define WIFI_SETUP_H__

#include <Arduino.h>

class WiFiSetup
{
public:
    static WiFiSetup* getInstance() { return s_instance; }

    void setup();
    String getLocalIP() const;

private:
    static WiFiSetup* s_instance;
};

#endif // WIFI_SETUP_H__
