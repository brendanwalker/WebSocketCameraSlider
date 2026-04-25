#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <sstream>
#include "WebSocketManager.h"
#include "SliderManager.h"

static AsyncWebServer s_server(80);
static AsyncWebSocket s_ws("/ws");

WebSocketManager* WebSocketManager::s_instance = nullptr;

// ---- WSCommandResponse ----

WSCommandResponse::WSCommandResponse()
    : m_bIsEmpty(true)
{
}

bool WSCommandResponse::isEmpty() const
{
    return m_bIsEmpty;
}

void WSCommandResponse::addStringParam(const std::string& value)
{
    addSeparator();
    m_responseStream << value;
}

void WSCommandResponse::addIntParam(const int value)
{
    addSeparator();
    m_responseStream << value;
}

void WSCommandResponse::addFloatParam(const float value)
{
    addSeparator();
    m_responseStream << value;
}

std::string WSCommandResponse::toString()
{
    return m_responseStream.str();
}

void WSCommandResponse::addSeparator()
{
    if (!m_bIsEmpty)
    {
        m_responseStream << " ";
    }
    m_bIsEmpty = false;
}

// ---- WebSocketManager ----

WebSocketManager::WebSocketManager()
{
    s_instance = this;
}

void WebSocketManager::setup()
{
    m_messageMutex = xSemaphoreCreateMutex();

    if (!LittleFS.begin(true))
    {
        Serial.println("WebSocketManager - LittleFS mount failed!");
    }
    else
    {
        Serial.println("WebSocketManager - LittleFS mounted");
    }

    s_ws.onEvent([](AsyncWebSocket* server, AsyncWebSocketClient* client,
                    AwsEventType type, void* arg, uint8_t* data, size_t len)
    {
        if (!WebSocketManager::s_instance) return;
        WebSocketManager* self = WebSocketManager::s_instance;

        switch (type)
        {
            case WS_EVT_CONNECT:
                Serial.printf("WebSocket client #%u connected from %s\n",
                    client->id(), client->remoteIP().toString().c_str());
                break;

            case WS_EVT_DISCONNECT:
                Serial.printf("WebSocket client #%u disconnected\n", client->id());
                break;

            case WS_EVT_DATA:
            {
                AwsFrameInfo* info = (AwsFrameInfo*)arg;
                if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
                {
                    String msg = String((char*)data, len);
                    if (self->m_messageMutex &&
                        xSemaphoreTake(self->m_messageMutex, portMAX_DELAY) == pdTRUE)
                    {
                        self->m_pendingMessages.push_back({client->id(), msg});
                        xSemaphoreGive(self->m_messageMutex);
                    }
                }
                break;
            }

            case WS_EVT_ERROR:
                Serial.printf("WebSocket error from client #%u\n", client->id());
                break;

            default:
                break;
        }
    });

    s_server.addHandler(&s_ws);

    s_server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    s_server.onNotFound([](AsyncWebServerRequest* req)
    {
        req->send(404, "text/plain", "Not found");
    });

    s_server.begin();
    Serial.println("WebSocketManager - HTTP server started on port 80");

    SliderState::getInstance()->setListener(this);
}

void WebSocketManager::loop()
{
    s_ws.cleanupClients();

    std::vector<WSPendingMessage> toProcess;
    if (m_messageMutex && xSemaphoreTake(m_messageMutex, 0) == pdTRUE)
    {
        toProcess.swap(m_pendingMessages);
        xSemaphoreGive(m_messageMutex);
    }

    for (auto& msg : toProcess)
    {
        processMessage(msg.clientId, msg.json);
    }
}

void WebSocketManager::setCommandHandler(WSCommandHandler* handler)
{
    m_commandHandler = handler;
}

void WebSocketManager::clearCommandHandler(WSCommandHandler* handler)
{
    if (m_commandHandler == handler)
    {
        m_commandHandler = nullptr;
    }
}

void WebSocketManager::setStatus(const std::string& status)
{
    String json = String("{\"type\":\"status\",\"value\":\"") + status.c_str() + "\"}";
    s_ws.textAll(json);
}

void WebSocketManager::processMessage(uint32_t clientId, const String& json)
{
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);
    if (error)
    {
        Serial.printf("WebSocketManager - JSON parse error: %s\n", error.c_str());
        return;
    }

    const char* cmdStr = doc["cmd"];
    const char* idStr  = doc["id"] | "";

    if (!cmdStr)
    {
        Serial.println("WebSocketManager - Missing 'cmd' field");
        return;
    }

    String commandId(idStr);

    // Parse space-separated command args
    std::vector<std::string> args;
    std::istringstream stream(cmdStr);
    for (std::string arg; std::getline(stream, arg, ' '); )
    {
        if (!arg.empty()) args.push_back(arg);
    }

    if (args.empty()) return;

    WSCommandResponse response;

    if (args[0] == "ping")
    {
        Serial.println("WebSocketManager - Received ping");
        response.addStringParam("pong");
    }
    else if (args[0] == "set_speed" && args.size() >= 2)
    {
        if (m_controlEnabled)
        {
            float value = std::stof(args[1]);
            SliderState::getInstance()->setSpeedFraction(value);
            Serial.printf("WebSocketManager - Set speed fraction: %f\n", value);
        }
    }
    else if (args[0] == "set_accel" && args.size() >= 2)
    {
        if (m_controlEnabled)
        {
            float value = std::stof(args[1]);
            SliderState::getInstance()->setAccelFraction(value);
            Serial.printf("WebSocketManager - Set accel fraction: %f\n", value);
        }
    }
    else if (m_commandHandler != nullptr)
    {
        if (!m_commandHandler->onCommand(args, response))
        {
            Serial.printf("WebSocketManager - Unhandled command: %s\n", cmdStr);
        }
    }
    else
    {
        Serial.printf("WebSocketManager - No handler for command: %s\n", cmdStr);
    }

    // Send response if we have content or a command ID to acknowledge
    if (!response.isEmpty() || !commandId.isEmpty())
    {
        String data = String(response.toString().c_str());
        String responseJson = String("{\"type\":\"response\",\"id\":\"") + commandId +
                              "\",\"data\":\"" + data + "\"}";

        AsyncWebSocketClient* client = s_ws.client(clientId);
        if (client)
        {
            client->text(responseJson);
        }
    }
}

void WebSocketManager::broadcastPositions()
{
    SliderState* slider = SliderState::getInstance();
    float sliderFrac = slider->getSliderPosFraction();
    float panFrac    = slider->getPanPosFraction();
    float tiltFrac   = slider->getTiltPosFraction();
    float speed      = slider->getSpeedFraction();
    float accel      = slider->getAccelFraction();

    char buf[128];
    snprintf(buf, sizeof(buf),
        "{\"type\":\"position\",\"slider\":%.3f,\"pan\":%.3f,\"tilt\":%.3f,\"speed\":%.3f,\"accel\":%.3f}",
        sliderFrac, panFrac, tiltFrac, speed, accel);
    s_ws.textAll(buf);
}

void WebSocketManager::onSliderMinSet(int32_t pos)
{
    Serial.printf("WebSocketManager - slider_min_set: %d\n", pos);
}

void WebSocketManager::onSliderMaxSet(int32_t pos)
{
    Serial.printf("WebSocketManager - slider_max_set: %d\n", pos);
}

void WebSocketManager::onSliderTargetSet(int32_t pos)
{
    broadcastPositions();
}

void WebSocketManager::onPanTargetSet(int32_t pos)
{
    broadcastPositions();
}

void WebSocketManager::onTiltTargetSet(int32_t pos)
{
    broadcastPositions();
}

void WebSocketManager::onMoveToTargetStart()
{
    setStatus("move_start");
}

void WebSocketManager::onMoveToTargetComplete()
{
    setStatus("move_complete");
}
