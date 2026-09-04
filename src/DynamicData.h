#ifndef DynamicData_h
#define DynamicData_h

#include <Arduino.h>
#include "NVMData.h"

class DynamicData
{
public:
    static const int numberOfErrorMessageHist = 4;
    static const int maxWands = 20;
private:
    DynamicData(/* args */);
    unsigned int errorCounter = 0;
    String errorMessageHist[numberOfErrorMessageHist];
    int errorMessagePointer = 0;
public:
    static DynamicData& get()
    {
        static DynamicData globalDynamicData;
        return globalDynamicData;
    }
    void Init();
    // static definitions
    const int MAX_NO_UPDATE = 600;

    String ipaddress = "";
    
    int connections = 0;
    String RSSIText = "";
    bool setNewNetwork = false;
    bool newNetworkSet = false;
    bool wandSelectionChanged = false;
    boolean connected = false;
    unsigned int bleAdvertisementsSeen = 0;
    unsigned int bleConnectAttempts = 0;
    String lastBLEName = "";
    String lastBLEAddress = "";
    String lastBLEEvent = "Waiting for scan result";
    String wandNames[maxWands];
    String wandAddresses[maxWands];
    int wandRssis[maxWands];
    int wandCount = 0;
    int CommandSend = 0;
    int CommandReceived = 0;
    int restartCounter = 0;
    int kickCounter = 0;
    uint32_t lastNotifyTime;
    uint32_t lastWriteTime;
    uint32_t notifyCounter;
    uint32_t writeCounter;
    uint32_t disconnectCounter;
    uint32_t writeErrors;
    boolean clientConnected = false;
    uint32_t bleBMSMatches = 0;
    int16_t gx = 0;
    int16_t gy = 0;
    int16_t gz = 0;
    int16_t ax = 0;
    int16_t ay = 0;
    int16_t az = 0;
    uint8_t finger = 0;
    uint32_t imuSampleCount = 0;
    // array of 1000 samples of interpreted IMU data, each sample is 12 bytes (gx, gy, gz, ax, ay, az)
    // x and y as position
    uint8_t lastFinger = 0;
    int sampleCounter = 0;
    float posX[1000];
    float posY[1000];
    bool dataValid = false;
    float posXreduced[20];
    float posYreduced[20];
    int reducedSampleCounter = 0;

    bool gestureRecording = false;
    bool gestureValid = false;

    unsigned int getErrorCounter();
    void incErrorCounter(String message);
    String getErrorHist(int pos);
    void addWand(const String &name, const String &address, int rssi);
};

void DynamicData::Init() {
    for (int i = 0; i < numberOfErrorMessageHist; i++)
    {
        errorMessageHist[i] = "";
    }
}
unsigned int DynamicData::getErrorCounter()
{
    return errorCounter;
    this->errorCounter;
}
void DynamicData::incErrorCounter(String message)
{
    errorMessageHist[errorMessagePointer] = message;
    errorMessagePointer++;
    if (errorMessagePointer >= numberOfErrorMessageHist)
    {
        errorMessagePointer = 0;
    }
    errorCounter++;
}
String DynamicData::getErrorHist(int pos)
{
    int arrayPos = pos + errorMessagePointer;
    if (arrayPos >= numberOfErrorMessageHist)
    {
        arrayPos -= numberOfErrorMessageHist;
    }
    if (arrayPos >= numberOfErrorMessageHist)
    {
        return "end";
    }
    return errorMessageHist[arrayPos];
}
void DynamicData::addWand(const String &name, const String &address, int rssi)
{
    for (int i = 0; i < wandCount; ++i) {
        if (wandAddresses[i] == address) {
            wandNames[i] = name;
            wandRssis[i] = rssi;
            return;
        }
    }
    if (wandCount >= maxWands) return;
    wandNames[wandCount] = name;
    wandAddresses[wandCount] = address;
    wandRssis[wandCount] = rssi;
    ++wandCount;
}

DynamicData::DynamicData(/* args */)
{
}
#endif
