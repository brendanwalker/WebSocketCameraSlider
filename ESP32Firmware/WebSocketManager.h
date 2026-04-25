#ifndef WEBSOCKET_MANAGER_H__
#define WEBSOCKET_MANAGER_H__

#include <Arduino.h>
#include <vector>
#include <string>
#include <sstream>
#include "SliderManager.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Drop-in replacement for BLECommandResponse
class WSCommandResponse
{
public:
    WSCommandResponse();

    bool isEmpty() const;
    void addStringParam(const std::string& value);
    void addIntParam(const int value);
    void addFloatParam(const float value);
    std::string toString();

private:
    void addSeparator();

    std::stringstream m_responseStream;
    bool m_bIsEmpty;
};

// Drop-in replacement for BLECommandHandler
class WSCommandHandler
{
public:
    virtual bool onCommand(const std::vector<std::string>& args, WSCommandResponse& results) { return false; }
};

struct WSPendingMessage
{
    uint32_t clientId;
    String json;
};

class WebSocketManager : public SliderStateEventListener
{
public:
    WebSocketManager();
    static WebSocketManager* getInstance() { return s_instance; }

    void setControlEnabled(bool bEnabled) { m_controlEnabled = bEnabled; }
    void setCommandHandler(WSCommandHandler* handler);
    void clearCommandHandler(WSCommandHandler* handler);
    void setStatus(const std::string& status);

    void setup();
    void loop();

private:
    static WebSocketManager* s_instance;

    void onWebSocketEvent(void* server, void* client, int type, void* arg, uint8_t* data, size_t len);
    void processMessage(uint32_t clientId, const String& json);

    // SliderStateEventListener
    virtual void onSliderMinSet(int32_t pos) override;
    virtual void onSliderMaxSet(int32_t pos) override;
    virtual void onSliderTargetSet(int32_t pos) override;
    virtual void onPanTargetSet(int32_t pos) override;
    virtual void onTiltTargetSet(int32_t pos) override;
    virtual void onMoveToTargetStart() override;
    virtual void onMoveToTargetComplete() override;

    void broadcastPositions();

    WSCommandHandler* m_commandHandler = nullptr;
    bool m_controlEnabled = true;

    std::vector<WSPendingMessage> m_pendingMessages;
    SemaphoreHandle_t m_messageMutex = nullptr;
};

#endif // WEBSOCKET_MANAGER_H__
