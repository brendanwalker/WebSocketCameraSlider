#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Adafruit_SSD1306.h>
#include "WiFiSetup.h"
#include "ConfigManager.h"

WiFiSetup* WiFiSetup::s_instance = nullptr;

// ---------------------------------------------------------------------------
// Captive portal HTML
// ---------------------------------------------------------------------------

static const char* const CAPTIVE_PORTAL_HTML = R"(<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Camera Slider Setup</title>
<style>
  body{font-family:sans-serif;max-width:400px;margin:40px auto;padding:0 20px;background:#111;color:#eee}
  h1{font-size:1.4em;margin-bottom:8px}
  p{color:#aaa;font-size:.9em;margin-bottom:24px}
  label{display:block;margin-bottom:4px;font-size:.9em;color:#ccc}
  input{width:100%;box-sizing:border-box;padding:10px;font-size:1em;border:1px solid #444;
        border-radius:6px;background:#222;color:#eee;margin-bottom:16px}
  button{width:100%;padding:12px;font-size:1em;background:#0a84ff;color:#fff;
         border:none;border-radius:6px;cursor:pointer}
  button:active{background:#006edc}
</style>
</head>
<body>
<h1>Camera Slider Setup</h1>
<p>Enter your WiFi credentials to connect the slider to your network.</p>
<form method="POST" action="/save">
  <label for="ssid">Network Name (SSID)</label>
  <input type="text" id="ssid" name="ssid"
         autocomplete="off" autocapitalize="none" spellcheck="false">
  <label for="password">Password</label>
  <input type="password" id="password" name="password" autocomplete="off">
  <button type="submit">Save &amp; Connect</button>
</form>
</body>
</html>)";

static const char* const CAPTIVE_PORTAL_SUCCESS_HTML = R"(<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Saved</title>
<style>
  body{font-family:sans-serif;max-width:400px;margin:40px auto;padding:0 20px;
       background:#111;color:#eee;text-align:center}
  h1{color:#30d158}
</style>
</head>
<body>
<h1>Saved!</h1>
<p>The slider is connecting to your network.</p>
<p>You can reconnect to your normal WiFi now.</p>
</body>
</html>)";

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void WiFiSetup::setup(Adafruit_SSD1306* display, bool forceAP)
{
    s_instance = this;

    if (!forceAP)
    {
        if (connectToSavedCredentials(display))
            return;
    }

    runCaptivePortal(display);
    connectToSavedCredentials(display);
}

String WiFiSetup::getLocalIP() const
{
    if (m_connected)
        return WiFi.localIP().toString();
    return "Not connected";
}

bool WiFiSetup::isConnected() const
{
    return m_connected;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

bool WiFiSetup::connectToSavedCredentials(Adafruit_SSD1306* display)
{
    String ssid, password;
    if (!ConfigManager::getInstance()->getWiFiCredentials(ssid, password))
        return false;

    showConnectingScreen(display, ssid);

    Serial.printf("WiFiSetup - Connecting to: %s\n", ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

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
        m_connected = true;
        Serial.printf("WiFiSetup - Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        showConnectedScreen(display);
        delay(1500);
        return true;
    }
    else
    {
        Serial.println("WiFiSetup - ERROR: Failed to connect to WiFi!");
        showFailedScreen(display);
        delay(2000);
        return false;
    }
}

void WiFiSetup::runCaptivePortal(Adafruit_SSD1306* display)
{
    Serial.println("WiFiSetup - Starting captive portal AP");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("CameraSlider-Setup");
    // Default AP IP: 192.168.4.1

    showAPModeScreen(display);

    DNSServer dnsServer;
    dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));

    WebServer server(80);
    bool credentialsReceived = false;

    // Serve the setup form for all captive portal detection endpoints
    auto serveForm = [&](){ server.send(200, "text/html", CAPTIVE_PORTAL_HTML); };
    server.on("/",                    HTTP_GET, serveForm);
    server.on("/generate_204",        HTTP_GET, serveForm); // Android
    server.on("/hotspot-detect.html", HTTP_GET, serveForm); // iOS / macOS
    server.on("/connecttest.txt",     HTTP_GET, serveForm); // Windows
    server.on("/ncsi.txt",            HTTP_GET, serveForm); // Windows

    server.on("/save", HTTP_POST, [&]()
    {
        String ssid     = server.arg("ssid");
        String password = server.arg("password");
        ssid.trim();

        if (ssid.length() == 0)
        {
            server.send(400, "text/html",
                "<p style='font-family:sans-serif;color:#eee;background:#111;padding:20px'>"
                "SSID cannot be empty. <a href='/' style='color:#0a84ff'>Go back</a></p>");
            return;
        }

        ConfigManager::getInstance()->setWiFiCredentials(ssid, password);
        credentialsReceived = true;
        server.send(200, "text/html", CAPTIVE_PORTAL_SUCCESS_HTML);
        Serial.printf("WiFiSetup - Credentials received for SSID: %s\n", ssid.c_str());
    });

    // Catch-all: redirect to portal
    server.onNotFound([&]()
    {
        server.sendHeader("Location", "http://192.168.4.1/", true);
        server.send(302, "text/plain", "");
    });

    server.begin();
    Serial.println("WiFiSetup - Captive portal running. Waiting for credentials...");

    unsigned long startMs = millis();
    const unsigned long TIMEOUT_MS = 5UL * 60UL * 1000UL; // 5 minutes

    while (!credentialsReceived && (millis() - startMs < TIMEOUT_MS))
    {
        dnsServer.processNextRequest();
        server.handleClient();
    }

    if (!credentialsReceived)
        Serial.println("WiFiSetup - AP mode timed out. Continuing without WiFi.");

    server.stop();
    dnsServer.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(200);
}

// ---------------------------------------------------------------------------
// OLED screens
// ---------------------------------------------------------------------------

void WiFiSetup::showAPModeScreen(Adafruit_SSD1306* display)
{
    display->clearDisplay();
    display->setTextSize(1);
    display->setTextColor(SSD1306_WHITE);
    display->setCursor(2, 4);  display->print("WiFi Setup Mode");
    display->setCursor(2, 18); display->print("Connect to:");
    display->setCursor(2, 30); display->print("CameraSlider-Setup");
    display->setCursor(2, 46); display->print("192.168.4.1");
    display->display();
}

void WiFiSetup::showConnectingScreen(Adafruit_SSD1306* display, const String& ssid)
{
    display->clearDisplay();
    display->setTextSize(1);
    display->setTextColor(SSD1306_WHITE);
    display->setCursor(2, 10); display->print("Connecting to:");
    display->setCursor(2, 24); display->print(ssid);
    display->display();
}

void WiFiSetup::showConnectedScreen(Adafruit_SSD1306* display)
{
    display->clearDisplay();
    display->setTextSize(1);
    display->setTextColor(SSD1306_WHITE);
    display->setCursor(2, 10); display->print("WiFi Connected!");
    display->setCursor(2, 24); display->print(WiFi.localIP().toString());
    display->display();
}

void WiFiSetup::showFailedScreen(Adafruit_SSD1306* display)
{
    display->clearDisplay();
    display->setTextSize(1);
    display->setTextColor(SSD1306_WHITE);
    display->setCursor(2, 10); display->print("WiFi Connect");
    display->setCursor(2, 22); display->print("Failed!");
    display->setCursor(2, 40); display->print("Hold button at boot");
    display->setCursor(2, 52); display->print("to reconfigure");
    display->display();
}
