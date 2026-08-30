#ifndef NVMData_h
#define NVMData_h

#include <Arduino.h>
#include <Preferences.h>

class NVMData
{
private:
    const char* prefDefaultValue = "none";

    const char* prefKeySSID = "ssid";
    const char* prefKeyPSK = "psk";
    const char* prefKeyWandName = "wand_name";
    const char* prefKeyWandAddress = "wand_addr";
    const char* prefKeyNamespace = "battery";
    const char* prefKeyOperatingHourCounter = "OHC";
    const int prefKeyPowerSourceSerialDefault = 0;
    const uint32_t prefKeyOffsetDefault = 0;

    String NetName = "";
    bool NetNameChanged = false;
    bool NetNameValid = false;
    String NetPassword = "";
    bool NetPasswordChanged = false;
    bool NetPasswordValid = false;
    String WandName = "";
    String WandAddress = "";
    int PV_U_ID = 0;
    int PV_I_ID = 0;
    int PV_P_ID = 0;
    int CurrentOffset = 0;
    NVMData(/* args */);
public:
    static NVMData& get()
    {
        static NVMData nonVolatileData;
        return nonVolatileData;
    }
    void Init();
    void StoreNetData();
    void DeleteNetData();
    void SetNetData(String newNetName, String newNetPassword);
    void SetWand(const String &name, const String &address);
    void SetDisplayIP(String newDisplayIP);
    void SetCCUIDs(int U_ID, int I_ID, int P_ID);
    void SetCurrentOffset(int offset);
    String GetNetName();
    String GetNetPassword();
    String GetWandName();
    String GetWandAddress();
    
    bool NetDataValid();
};

void NVMData::Init() 
{
    Preferences preferences;
    preferences.begin(prefKeyNamespace, false);
    NetName = preferences.getString(prefKeySSID, prefDefaultValue);
    NetPassword = preferences.getString(prefKeyPSK, prefDefaultValue);
    WandName = preferences.getString(prefKeyWandName, "");
    WandAddress = preferences.getString(prefKeyWandAddress, "");
    preferences.end();
    if (NetName != prefDefaultValue)
    {
        NetNameValid = true;
    }
    if (NetPassword != prefDefaultValue)
    {
        NetPasswordValid = true;
    }
}
bool NVMData::NetDataValid()
{
    bool retVal = true;
    if (NetNameValid == false)
    {
        retVal = false;
    }
    if (NetPasswordValid == false)
    {
        retVal = false;
    }
    return retVal;
}
void NVMData::SetNetData(String newNetName, String newNetPassword)
{
    if (NetName != newNetName)
    {
      NetName = newNetName;
      NetNameChanged = true;
    }
    if (NetPassword != newNetPassword)
    {
      NetPassword = newNetPassword;
      NetPasswordChanged = true;
    }
}
void NVMData::SetWand(const String &name, const String &address)
{
    WandName = name;
    WandAddress = address;

    Preferences preferences;
    preferences.begin(prefKeyNamespace, false);
    preferences.putString(prefKeyWandName, WandName);
    preferences.putString(prefKeyWandAddress, WandAddress);
    preferences.end();
}
String NVMData::GetNetName()
{
    return NetName;
}
String NVMData::GetNetPassword()
{
    return NetPassword;
}
String NVMData::GetWandName()
{
    return WandName;
}
String NVMData::GetWandAddress()
{
    return WandAddress;
}
void NVMData::StoreNetData() {
    Preferences preferences;
    preferences.begin(prefKeyNamespace, false);
    if (NetNameChanged == true)
    {
        preferences.putString(prefKeySSID, NetName);
        NetNameChanged = false;
    }
    if (NetPasswordChanged == true)
    {
        preferences.putString(prefKeyPSK, NetPassword);
        NetPasswordChanged = false;
    }
    preferences.end();
}
void NVMData::DeleteNetData() {
    Preferences preferences;
    preferences.begin(prefKeyNamespace, false);
    preferences.remove(prefKeyPSK);
    preferences.remove(prefKeySSID);
    preferences.end();
    NetName = "";
    NetPassword ="";
}

NVMData::NVMData(/* args */)
{
}

#endif
