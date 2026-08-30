#ifndef WebPage_h
#define WebPage_h

#include <Arduino.h>
#include <math.h>
#include <WebServer.h>
#include <Update.h>
#include "NVMData.h"
#include "DynamicData.h"

class WebPage
{
private:
    WebServer server;
    String GenHeader(int redirectTime);
    String GenFooter();
    String GenTableStart();
    String GenTableNewColumn();
    String GenTableRows(String Content[], int Count);
    String GenTableEnd();
    String addKeyValuePair(String key, String value);
    void handleRoot();
    void handleChange();
    void handleChangeID();
    void handleChangeCur();
    void handleNotFound();
    void handleFirmware();
    void handleUpload();
    void handleUpload2();
    void handleJson();
    void handleWands();
    void handleSelectWand();
    void handleGesture();
    void handleRecordedGesture();
    String generateGestureSvg(size_t gestureIndex);
    String setMessage(String arg, String value, String result);

public:
    WebPage();
    ~WebPage();
    void Init();
    void loop();
    static WebPage &get();
};
WebPage::WebPage()
{
}
WebPage::~WebPage()
{
}
WebServer server(80);
WebPage &WebPage::get()
{
    static WebPage page;
    return page;
}
void WebPage::loop()
{
    server.handleClient();
}
void WebPage::Init()
{

    server.on("/", std::bind(&WebPage::handleRoot, this));
    server.on("/change", std::bind(&WebPage::handleChange, this));
    server.on("/json", std::bind(&WebPage::handleJson, this));
    server.on("/wands", std::bind(&WebPage::handleWands, this));
    server.on("/selectwand", HTTP_POST, std::bind(&WebPage::handleSelectWand, this));
    server.on("/gesture", std::bind(&WebPage::handleGesture, this));
    server.on("/recorded_gesture", std::bind(&WebPage::handleRecordedGesture, this));
    // choose bin file
    server.on("/firmware", HTTP_GET, std::bind(&WebPage::handleFirmware, this));
    /*handling uploading firmware file */
    server.on("/update", HTTP_POST, std::bind(&WebPage::handleUpload, this), std::bind(&WebPage::handleUpload2, this));
    server.onNotFound(std::bind(&WebPage::handleNotFound, this));
    server.begin();
    // Serial.println("HTTP server started");
}
String WebPage::GenHeader(int redirectTime)
{
    String message = "";
    message += "<html>";
    message += "<head>";
    message += "<meta charset=\"utf-8\">";
    if (redirectTime > 0)
    {
        String redirectTimeString = String(redirectTime);
        message += "<meta http-equiv=\"refresh\" content=\"" + redirectTimeString + ";url=http://" + DynamicData::get().ipaddress + "/\" />";
    }
    message += "</head>";
    message += "<body>\n";
    return message;
}
String WebPage::GenFooter()
{
    String message = "";
    message += "\t<p>---------------------------</p>\n";
    message += "</body>";
    message += "</html>\n";
    return message;
}
String WebPage::GenTableStart()
{
    String message = "";
    message += "\t<table border=\"4\">";
    message += "\t<tr>\n";
    return message;
}
String WebPage::GenTableNewColumn()
{
    String message = "";
    message += "\t</tr>";
    message += "\t<tr>\n";
    return message;
}
String WebPage::GenTableRows(String Content[], int Count)
{
    String message = "";
    for (int i = 0; i < Count; i++)
    {
        message += "<td>" + Content[i] + "</td>";
    }
    message += "\n";
    return message;
}
String WebPage::GenTableEnd()
{
    String message = "";
    message += "\t</tr>";
    message += "\t</table>\n";
    return message;
}
void WebPage::handleNotFound()
{
    String message = "";
    message += GenHeader(3);
    message += "File Not Found\n\n";
    message += GenFooter();
    server.send(200, "text/html", message);
}
void WebPage::handleChange()
{
    String message = "Ohh oh!\n\n";
    bool netNameSet = false;
    bool netPasswordSet = false;
    String netName = "";
    String netPassword = "";
    for (uint8_t i = 0; i < server.args(); i++)
    {
        if (server.argName(i) == "netname")
        {
            netNameSet = true;
            netName = server.arg(i);
        }
        else if (server.argName(i) == "password")
        {
            netPasswordSet = true;
            netPassword = server.arg(i);
        }
    }
    if ((netPasswordSet == true) && (netNameSet == true))
    {
        DynamicData::get().newNetworkSet = true;
        NVMData::get().SetNetData(netName, netPassword);
        message = "You got it!";
    }
    String returnMessage = "";
    returnMessage += GenHeader(3);
    returnMessage += message;
    returnMessage += "</body>";
    returnMessage += "</html>";
    server.send(200, "text/html", returnMessage);
}
void WebPage::handleFirmware()
{
    const char *serverIndex =
        "<script src='https://sciphy.de/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
        "<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
        "<input type='file' name='update'>"
        "<input type='submit' value='Update'>"
        "</form>"
        "<div id='prg'>progress: 0%</div>"
        "<script>"
        "$('form').submit(function(e){"
        "e.preventDefault();"
        "var form = $('#upload_form')[0];"
        "var data = new FormData(form);"
        " $.ajax({"
        "url: '/update',"
        "type: 'POST',"
        "data: data,"
        "contentType: false,"
        "processData:false,"
        "xhr: function() {"
        "var xhr = new window.XMLHttpRequest();"
        "xhr.upload.addEventListener('progress', function(evt) {"
        "if (evt.lengthComputable) {"
        "var per = evt.loaded / evt.total;"
        "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
        "}"
        "}, false);"
        "return xhr;"
        "},"
        "success:function(d, s) {"
        "console.log('success!')"
        "},"
        "error: function (a, b, c) {"
        "}"
        "});"
        "});"
        "</script>";
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
}
void WebPage::handleUpload()
{
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
}
void WebPage::handleUpload2()
{
    HTTPUpload &upload = server.upload();
    if (upload.status == UPLOAD_FILE_START)
    {
        // Serial.printf("Update: %s\n", upload.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN))
        { // start with max available size
            Update.printError(Serial);
        }
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        /* flashing firmware to ESP*/
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
        {
            Update.printError(Serial);
        }
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        if (Update.end(true))
        { // true to set the size to the current progress
            // Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
        }
        else
        {
            Update.printError(Serial);
        }
    }
}
void WebPage::handleRoot()
{
    String uri = DynamicData::get().ipaddress;
    String message = "";
    message += GenHeader(0);
    // message += "<meta http-equiv=\"refresh\" content=\"4\">";
    message += "<h1>Magic Caster Wand</h1>";
    if (NVMData::get().GetWandAddress().length() > 0)
    {
        message += "<p><strong>Ausgew\xC3\xA4hlt:</strong> " + NVMData::get().GetWandName();
        message += " (" + NVMData::get().GetWandAddress() + ")</p>";
    }
    else
    {
        message += "<p>Noch kein Zauberstab ausgew\xC3\xA4"
                   "hlt.</p>";
    }
    message += "<h2>Gefundene Zauberst\xC3\xA4"
               "be</h2>";
    message += "<table border=\"1\" cellpadding=\"7\"><tr><th>Name</th><th>BLE-Adresse</th><th>RSSI</th><th></th></tr>";
    if (DynamicData::get().wandCount == 0)
    {
        message += "<tr><td colspan=\"3\">Noch kein MCW-xxxx-Ger\xC3\xA4"
                   "t gefunden.</td></tr>";
    }
    for (int i = 0; i < DynamicData::get().wandCount; ++i)
    {
        message += "<tr><td>" + DynamicData::get().wandNames[i] + "</td><td>";
        message += DynamicData::get().wandAddresses[i] + "</td><td>";
        message += String(DynamicData::get().wandRssis[i]) + " dBm</td><td>";
        message += "<form action=\"/selectwand\" method=\"post\">";
        message += "<input type=\"hidden\" name=\"name\" value=\"" + DynamicData::get().wandNames[i] + "\">";
        message += "<input type=\"hidden\" name=\"address\" value=\"" + DynamicData::get().wandAddresses[i] + "\">";
        message += "<button type=\"submit\">Ausw\xC3\xA4"
                   "hlen</button></form></td></tr>";
    }
    message += "</table>";
    message += "<p>Bluetooth: " + DynamicData::get().lastBLEEvent + "</p>";
    message += "<p>Verbindung: " + String(DynamicData::get().connected ? "verbunden" : "nicht verbunden") + "</p>";
    message += "<p>IMU: GX=" + String(DynamicData::get().gx) + " GY=" + String(DynamicData::get().gy) + " GZ=" + String(DynamicData::get().gz);
    message += " | AX=" + String(DynamicData::get().ax) + " AY=" + String(DynamicData::get().ay) + " AZ=" + String(DynamicData::get().az) + "</p>";
    message += "<p>IMU-Samples: " + String(DynamicData::get().imuSampleCount) + " | Finger: 0x" + String(DynamicData::get().finger, HEX) + "</p>";
    message += "<p><a href=\"/wands\">Nur die Ger\xC3\xA4"
               "teliste (JSON)</a></p>";
    message += "<h2>Gesten</h2>";
    for (size_t i = 0; i < kGestureReferenceCount; ++i)
    {
        message += "<form action=\"/gesture\" target=\"_blank\" style=\"display:inline-block;margin:4px\">";
        message += "<input type=\"hidden\" name=\"id\" value=\"" + String(i) + "\">";
        message += "<button type=\"submit\">" + String(kGestureReferences[i].name) + "</button></form>";
    }

    if (DynamicData::get().setNewNetwork == true)
    {
        message += "\t<form action=\"change\">\n";
        message += "\t<label class=\"h2\" form=\"networkdata\">Network name</label>\n";
        message += "\t<div>\n";
        message += "\t<label for=\"netname\">netname</label>\n";
        message += "\t<input type=\"text\" name=\"netname\" maxlength=\"30\">\n";
        message += "\t</div>\n";
        message += "\t<div>\n";
        message += "\t<label for=\"password\">password</label>\n";
        message += "\t<input type=\"text\" name=\"password\" maxlength=\"40\">\n";
        message += "\t</div>\n";
        message += "\t<div>\n";
        message += "\t<button type=\"reset\">clear</button>\n";
        message += "\t<button type=\"submit\">set</button>\n";
        message += "\t</div>\n";
        message += "\t</form>\n";
    }
    // internal links
    message += "\t<p>---------------------------</p>\n";
    message += "\t<p><a href=\"/firmware\">Firmware-Update</a></p>\n";
    message += "\t<p><a href=\"/json\">JSON-Daten</a></p>\n";
    message += "\t<p><a href=\"/recorded_gesture\">Recorded Gesture</a></p>\n";
    message += "\t<p>---------------------------</p>\n";
    message += "\t<p>ipaddress:  " + DynamicData::get().ipaddress + "</p>\n";
    message += "\t<p>errorCounter: " + String(DynamicData::get().getErrorCounter()) + "</p>\n";
    for (int i = 0; i < DynamicData::get().numberOfErrorMessageHist; i++)
    {
        message += "\t<p>errorHistory: " + DynamicData::get().getErrorHist(i) + "</p>\n";
    }
    message += "\t<p>connections: " + String(DynamicData::get().connections) + "</p>\n";
    message += "\t<p>RSSIText: " + DynamicData::get().RSSIText + "</p>\n";
    message += "\t<p>---------------------------</p>\n";
    message += GenFooter();
    server.send(200, "text/html", message);
}

void WebPage::handleWands()
{
    String message = "[";
    for (int i = 0; i < DynamicData::get().wandCount; ++i)
    {
        if (i > 0)
            message += ",";
        message += "{\"name\":\"" + DynamicData::get().wandNames[i] + "\",\"address\":\"";
        message += DynamicData::get().wandAddresses[i] + "\",\"rssi\":";
        message += String(DynamicData::get().wandRssis[i]) + "}";
    }
    message += "]";
    server.send(200, "application/json", message);
}

void WebPage::handleSelectWand()
{
    const String name = server.arg("name");
    const String address = server.arg("address");
    bool found = false;

    for (int i = 0; i < DynamicData::get().wandCount; ++i)
    {
        if (DynamicData::get().wandNames[i] == name && DynamicData::get().wandAddresses[i] == address)
        {
            found = true;
            break;
        }
    }

    if (found)
    {
        NVMData::get().SetWand(name, address);
        DynamicData::get().wandSelectionChanged = true;
        server.sendHeader("Location", "/");
        server.send(303);
        return;
    }

    server.send(400, "text/plain", "Wand was not found in the current scan results.");
}

void WebPage::handleGesture()
{
    const String requestedIndex = server.arg("id");
    const int gestureIndex = requestedIndex.toInt();
    if (requestedIndex != String(gestureIndex) || gestureIndex < 0 || static_cast<size_t>(gestureIndex) >= kGestureReferenceCount)
    {
        server.send(404, "text/plain", "Gesture not found.");
        return;
    }
    server.send(200, "image/svg+xml; charset=utf-8", generateGestureSvg(static_cast<size_t>(gestureIndex)));
}

void WebPage::handleRecordedGesture()
{
    if (!DynamicData::get().dataValid)
    {
        server.send(400, "text/plain", "No valid recorded gesture data available.");
        return;
    }

    String points = "";
    for (int i = 0; i < DynamicData::get().sampleCounter; ++i)
    {
        points += String(400 * DynamicData::get().posX[i] + 400) + ",";
        points += String(800 - (400 * DynamicData::get().posY[i] + 400)) + " ";
    }

    String svg = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
    svg += "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 800 800\">";
    svg += "<rect width=\"800\" height=\"800\" fill=\"#f8fafc\"/>";
    svg += "<polyline points=\"" + points + "\" fill=\"none\" stroke=\"#2563eb\" stroke-width=\"12\" stroke-linecap=\"round\" stroke-linejoin=\"round\"/>";
    svg += "</svg>";

    server.send(200, "image/svg+xml; charset=utf-8", svg);
}

String WebPage::generateGestureSvg(size_t gestureIndex)
{
    constexpr float pi = 3.14159f;
    constexpr float canvasSize = 800.0f;
    constexpr float margin = 70.0f;
    const GestureReference &gesture = kGestureReferences[gestureIndex];
    float x[8] = {0};
    float y[8] = {0};
    float minX = 0;
    float maxX = 0;
    float minY = 0;
    float maxY = 0;

    for (uint8_t i = 0; i < gesture.segmentCount; ++i)
    {
        const float angle = gesture.segments[i].direction * pi / 180.0f;
        // In test.py, direction is atan2(dx, dy): x=sin(angle), y=cos(angle).
        x[i + 1] = x[i] + sinf(angle) * gesture.segments[i].length;
        y[i + 1] = y[i] + cosf(angle) * gesture.segments[i].length;
        minX = min(minX, x[i + 1]);
        maxX = max(maxX, x[i + 1]);
        minY = min(minY, y[i + 1]);
        maxY = max(maxY, y[i + 1]);
    }

    const float spanX = max(1.0f, maxX - minX);
    const float spanY = max(1.0f, maxY - minY);
    const float scale = min((canvasSize - 2 * margin) / spanX, (canvasSize - 2 * margin) / spanY);
    const float offsetX = (canvasSize - (maxX - minX) * scale) / 2.0f - minX * scale;
    const float offsetY = (canvasSize - (maxY - minY) * scale) / 2.0f - minY * scale;

    String points = "";
    for (uint8_t i = 0; i <= gesture.segmentCount; ++i)
    {
        points += String(x[i] * scale + offsetX, 1) + "," + String(800 - (y[i] * scale + offsetY), 1) + " ";
    }

    const float endX = x[gesture.segmentCount] * scale + offsetX;
    const float endY = 800 - (y[gesture.segmentCount] * scale + offsetY);
    String svg = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
    svg += "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 800 800\">";
    svg += "<defs><marker id=\"arrow\" markerWidth=\"6\" markerHeight=\"6\" refX=\"2\" refY=\"2\" orient=\"auto\"><path d=\"M0,0 L0,4 L5,2 z\" fill=\"#2563eb\"/></marker></defs>";
    svg += "<rect width=\"800\" height=\"800\" fill=\"#f8fafc\"/>";
    svg += "<text x=\"400\" y=\"55\" text-anchor=\"middle\" font-family=\"sans-serif\" font-size=\"30\" font-weight=\"bold\" fill=\"#0f172a\">" + String(gesture.name) + "</text>";
    svg += "<polyline points=\"" + points + "\" fill=\"none\" stroke=\"#2563eb\" stroke-width=\"12\" stroke-linecap=\"round\" stroke-linejoin=\"round\" marker-end=\"url(#arrow)\"/>";
    svg += "<circle cx=\"" + String(offsetX, 1) + "\" cy=\"" + String(800 - offsetY, 1) + "\" r=\"15\" fill=\"#2563eb\"/>";
    svg += "</svg>";
    return svg;
}

String WebPage::addKeyValuePair(String key, String value)
{
    String retVal = "";
    retVal += "\"";
    retVal += key;
    retVal += "\" :\"";
    retVal += value;
    retVal += "\"";
    return retVal;
}
void WebPage::handleJson()
{
    String message = "";
    String key = "";
    message += "{ ";
    message += addKeyValuePair("Data1", "data1 content");
    message += ",";
    message += addKeyValuePair("Data2", "data2 content");
    message += ",";
    message += addKeyValuePair("Data3", "data3 content");
    message += "}";
    server.send(200, "text/plain", message);
}
#endif
