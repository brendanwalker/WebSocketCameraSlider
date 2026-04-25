#include <WiFi.h>
#include "WiFiSetup.h"
#include "wifi_config.h"

WiFiSetup* WiFiSetup::s_instance = nullptr;

void WiFiSetup::setup()
{
    s_instance = this;

    Serial.printf("WiFiSetup - Connecting to: %s\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40)
    {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.printf("WiFiSetup - Connected! IP address: %s\n", WiFi.localIP().toString().c_str());
    }
    else
    {
        Serial.println("WiFiSetup - ERROR: Failed to connect to WiFi!");
    }
}

String WiFiSetup::getLocalIP() const
{
    return WiFi.localIP().toString();
}
