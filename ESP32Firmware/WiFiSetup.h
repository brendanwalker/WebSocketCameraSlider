#ifndef WIFI_SETUP_H__
#define WIFI_SETUP_H__

#include <Arduino.h>

class WiFiSetup
{
public:
    static WiFiSetup* getInstance() { return s_instance; }

    void setup(class Adafruit_SSD1306* display, bool forceAP);
    String getLocalIP() const;
    bool isConnected() const;

private:
    static WiFiSetup* s_instance;

    void runCaptivePortal(class Adafruit_SSD1306* display);
    bool connectToSavedCredentials(class Adafruit_SSD1306* display);
    void showAPModeScreen(class Adafruit_SSD1306* display);
    void showConnectingScreen(class Adafruit_SSD1306* display, const String& ssid);
    void showConnectedScreen(class Adafruit_SSD1306* display);
    void showFailedScreen(class Adafruit_SSD1306* display);

    bool m_connected = false;
};

#endif // WIFI_SETUP_H__
