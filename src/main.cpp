#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <NimBLEDevice.h>

#include "DynamicData.h"
#include "generated/GestureData.h"
#include "NVMData.h"
#include "WebPage.h"

WiFiUDP spellUdp;

constexpr uint16_t SPELL_BROADCAST_PORT = 8888;

namespace {
constexpr char WAND_NAME_PREFIX[] = "MCW-";
constexpr uint32_t SCAN_DURATION_MS = 10 * 1000;
constexpr uint32_t WAND_RESCAN_INTERVAL_MS = 10000;
constexpr uint32_t IMU_INIT_TIMEOUT_MS = 1000;

volatile uint32_t lastImuDataAt = 0;
volatile bool fingerPressed = false;
volatile bool oldFingerPressed = false;
volatile bool spellFound = false;

uint32_t lastWandScan = 0;
constexpr char SERVICE_UUID[] = "57420001-587E-48A0-974C-544D6163C577";
constexpr char SERVICE_NOTIFY[] = "57420003-587E-48A0-974C-544D6163C577";
constexpr char SERVICE_WRITE[] = "57420002-587E-48A0-974C-544D6163C577";

NimBLEClient *wandClient = nullptr;
NimBLERemoteCharacteristic *notifyCharacteristic = nullptr;
NimBLERemoteCharacteristic *writeCharacteristic = nullptr;
NimBLEAddress selectedAddress;
volatile bool connectPending = false;
volatile bool scanRequested = false;

constexpr int ANGLE_STEP = 1;
constexpr int DIR_COUNT = 360;
constexpr int MAX_DIR_ERROR = 20;
constexpr int MAX_LEN_ERROR = 200;

void broadcastSpell(const String &spellName)
{
    String message = "spell:";
    message += spellName;
    message += ":";
    message += NVMData::get().GetHouse();
    message += ":";
    message += NVMData::get().GetPatronus();

    spellUdp.beginPacket(IPAddress(255, 255, 255, 255), SPELL_BROADCAST_PORT);
    spellUdp.print(message);
    spellUdp.endPacket();

    Serial.printf("Spell broadcast: %s\n", message.c_str());
}

void setupWiFi()
{
    constexpr char hostname[] = "MagicCasterWand";
    if (!NVMData::get().NetDataValid()) {
        DynamicData::get().setNewNetwork = true;
        WiFi.mode(WIFI_AP);
        WiFi.softAP(hostname, "alohomora");
        Serial.println("No valid network data found. Starting AP mode.");
        DynamicData::get().ipaddress = WiFi.softAPIP().toString();
        return;
    }

    Serial.printf("Connecting to WiFi network: %s\n", NVMData::get().GetNetName().c_str());
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(hostname);
    WiFi.begin(NVMData::get().GetNetName().c_str(), NVMData::get().GetNetPassword().c_str());
    const uint32_t startedAt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startedAt < 15 * 1000) delay(100);

    if (WiFi.status() == WL_CONNECTED) {
        DynamicData::get().ipaddress = WiFi.localIP().toString();
    } else {
        DynamicData::get().setNewNetwork = true;
        WiFi.mode(WIFI_AP);
        WiFi.softAP(hostname, "alohomora");
        DynamicData::get().ipaddress = WiFi.softAPIP().toString();
    }
}

class WandScanCallbacks : public NimBLEScanCallbacks
{
    void onResult(const NimBLEAdvertisedDevice *device) override
    {
        const String name(device->getName().c_str());
        DynamicData::get().bleAdvertisementsSeen++;
        if (name.startsWith(WAND_NAME_PREFIX)) {
            DynamicData::get().addWand(name, String(device->getAddress().toString().c_str()), device->getRSSI());
        }
        if (DynamicData::get().connected || NVMData::get().GetWandAddress().length() == 0) return;
        if (String(device->getAddress().toString().c_str()) == NVMData::get().GetWandAddress()) {
            Serial.printf("Selected wand found: %s. Stopping scan and requesting connection.\n", device->getAddress().toString().c_str());
            selectedAddress = device->getAddress();
            connectPending = true;
            scanRequested = false;
            NimBLEDevice::getScan()->stop();
        }
    }
};

void startScan(bool resetWandList)
{
    DynamicData::get().lastBLEEvent = "Scanning for MCW devices";
    NimBLEDevice::getScan()->start(SCAN_DURATION_MS, false, resetWandList);
}

struct segmentation_t {
    int direction; // 0-360 degrees
    int start;     // start index in the points array
    int length;    // number of points in the segment
};

struct CandidateSegment {
    uint16_t direction;
    float length;
};

struct point_t {
    float x;
    float y;
};

void create_segments(point_t *points, int pointCount, CandidateSegment *segments, int *segmentCount)
{
    *segmentCount = 0;

    for (int i = 1; i < pointCount; ++i) {

        float dx = points[i].x - points[i - 1].x;
        float dy = points[i].y - points[i - 1].y;
        float length = hypotf(dx, dy);

        if (length < 1e-6f) {
            continue;
        }

        // Exakt wie Python:
        // degrees(atan2(dx, dy))
        float angle = atan2f(dx, dy) * 180.0f / PI;

        int direction =
            static_cast<int>(roundf(angle / ANGLE_STEP))
            % DIR_COUNT;

        if (direction < 0) {
            direction += DIR_COUNT;
        }

        segments[*segmentCount].direction = static_cast<uint16_t>(direction);
        segments[*segmentCount].length = length;
        (*segmentCount)++;
    }
}

void normalize_segments(CandidateSegment *segments, int segmentCount) 
{
    float totalLength = 0.0f;

    for (int i = 0; i < segmentCount; ++i) {
        totalLength += segments[i].length;
    }

    if (totalLength <= 0.0f) {
        return;
    }

    for (int i = 0; i < segmentCount; ++i) {
        segments[i].length = roundf(segments[i].length / totalLength * 1000.0f);
    }
}

int angle_diff(int a1, int a2)
{
    int d = abs(a1 - a2);
    return (d < 360 - d) ? d : 360 - d;
}

struct test_point_t {
    float x;
    float y;
};

#ifdef DEBUG_GESTURE_POINTS
static const test_point_t testGesture[] = {
    {0.002f,  0.001f},
    {0.004f,  0.001f},
    {0.005f,  0.001f},
    {0.007f,  0.002f},
    {0.008f,  0.002f},
    {0.009f,  0.002f},
    {0.010f,  0.002f},
    {0.011f,  0.001f},
    {0.011f,  0.001f},
    {0.011f,  0.000f},
    {0.011f, -0.000f},
    {0.011f, -0.001f},
    {0.010f, -0.001f},
    {0.010f, -0.002f},
    {0.009f, -0.002f},
    {0.010f, -0.002f},
    {0.010f, -0.002f},
    {0.011f, -0.002f},
    {0.012f, -0.002f},
    {0.014f, -0.001f},
    {0.017f, -0.001f},
    {0.020f,  0.000f},
    {0.023f,  0.001f},
    {0.027f,  0.002f},
    {0.031f,  0.003f},
    {0.035f,  0.003f},
    {0.040f,  0.004f},
    {0.044f,  0.004f},
    {0.048f,  0.003f},
    {0.052f,  0.003f},
    {0.055f,  0.003f},
    {0.057f,  0.002f},
    {0.059f,  0.001f},
    {0.059f, -0.000f},
    {0.059f, -0.001f},
    {0.059f, -0.002f},
    {0.058f, -0.003f},
    {0.056f, -0.004f},
    {0.055f, -0.005f},
    {0.053f, -0.006f},
    {0.052f, -0.007f},
    {0.051f, -0.008f},
    {0.049f, -0.009f},
    {0.049f, -0.010f},
    {0.048f, -0.011f},
    {0.048f, -0.012f},
    {0.048f, -0.013f},
    {0.049f, -0.013f},
    {0.050f, -0.014f},
    {0.051f, -0.014f},
    {0.053f, -0.015f},
    {0.055f, -0.016f},
    {0.057f, -0.016f},
    {0.059f, -0.017f},
    {0.061f, -0.018f},
    {0.063f, -0.019f},
    {0.065f, -0.020f},
    {0.067f, -0.021f},
    {0.069f, -0.023f},
    {0.070f, -0.025f},
    {0.071f, -0.028f},
    {0.072f, -0.030f},
    {0.073f, -0.033f},
    {0.073f, -0.035f},
    {0.073f, -0.037f},
    {0.074f, -0.038f},
    {0.074f, -0.039f},
    {0.075f, -0.038f},
    {0.076f, -0.036f},
    {0.077f, -0.032f},
    {0.079f, -0.027f},
    {0.082f, -0.020f},
    {0.086f, -0.011f},
    {0.090f,  0.000f},
    {0.096f,  0.013f},
    {0.102f,  0.028f},
    {0.110f,  0.044f},
    {0.118f,  0.061f},
    {0.126f,  0.080f},
    {0.135f,  0.101f},
    {0.144f,  0.122f},
    {0.154f,  0.145f},
    {0.164f,  0.169f},
    {0.174f,  0.193f},
    {0.185f,  0.218f},
    {0.197f,  0.244f},
    {0.210f,  0.271f},
    {0.223f,  0.299f},
    {0.237f,  0.328f},
    {0.252f,  0.357f},
    {0.268f,  0.387f},
    {0.285f,  0.417f},
    {0.302f,  0.447f},
    {0.320f,  0.477f},
    {0.338f,  0.507f},
    {0.356f,  0.535f},
    {0.374f,  0.562f},
    {0.391f,  0.588f},
    {0.409f,  0.612f},
    {0.426f,  0.635f},
    {0.443f,  0.655f},
    {0.459f,  0.672f},
    {0.475f,  0.688f},
    {0.491f,  0.702f},
    {0.506f,  0.713f},
    {0.520f,  0.723f},
    {0.534f,  0.731f},
    {0.547f,  0.738f},
    {0.560f,  0.743f},
    {0.572f,  0.748f},
    {0.583f,  0.752f},
    {0.593f,  0.754f},
    {0.602f,  0.755f},
    {0.611f,  0.755f},
    {0.618f,  0.753f},
    {0.624f,  0.749f},
    {0.629f,  0.741f},
    {0.634f,  0.731f},
    {0.638f,  0.716f},
    {0.641f,  0.698f},
    {0.644f,  0.677f},
    {0.648f,  0.652f},
    {0.651f,  0.625f},
    {0.654f,  0.596f},
    {0.657f,  0.566f},
    {0.659f,  0.534f},
    {0.662f,  0.501f},
    {0.664f,  0.467f},
    {0.666f,  0.433f},
    {0.668f,  0.398f},
    {0.670f,  0.362f},
    {0.672f,  0.326f},
    {0.674f,  0.290f},
    {0.676f,  0.254f},
    {0.679f,  0.219f},
    {0.682f,  0.184f},
    {0.685f,  0.151f},
    {0.688f,  0.118f},
    {0.691f,  0.087f},
    {0.694f,  0.056f},
    {0.695f,  0.025f},
    {0.696f, -0.006f},
    {0.695f, -0.037f},
    {0.693f, -0.068f},
    {0.689f, -0.099f},
    {0.684f, -0.128f},
    {0.678f, -0.155f},
    {0.670f, -0.179f},
    {0.662f, -0.201f},
    {0.654f, -0.220f},
    {0.646f, -0.236f},
    {0.640f, -0.249f}
};

constexpr int TEST_GESTURE_COUNT =
    sizeof(testGesture) / sizeof(testGesture[0]);
#endif
bool keep[1000];

float perpendicular_distance(float px, float py, float ax, float ay, float bx, float by)
{
    if (ax == bx && ay == by) {
        float dx = px - ax;
        float dy = py - ay;
        return sqrtf(dx * dx + dy * dy);
    }

    float num = fabsf(
          (by - ay) * px
        - (bx - ax) * py
        + bx * ay
        - by * ax
    );

    float den = sqrtf( (bx - ax) * (bx - ax) + (by - ay) * (by - ay) );

    return num / den;
}

void douglas_peucker(point_t *points, int start, int end, float epsilon, bool *keep)
{
    if (end - start < 2) {
        return;
    }

    float max_distance = 0.0f;
    int index = -1;

    const float startX = points[start].x;
    const float startY = points[start].y;

    const float endX = points[end].x;
    const float endY = points[end].y;

    for (int i = start + 1; i < end; ++i) {

        float d = perpendicular_distance(points[i].x, points[i].y, startX, startY, endX, endY);

        if (d > max_distance) {
            max_distance = d;
            index = i;
        }
    }

    if (max_distance > epsilon) {
        keep[index] = true;
        douglas_peucker(points, start, index, epsilon, keep);
        douglas_peucker(points, index, end, epsilon, keep);
    }
}

int simplify_points(point_t *points, int pointCount, float epsilon)
{
    if (pointCount < 3) {
        return pointCount;
    }
    memset(keep, 0, sizeof(keep));
    keep[0] = true;
    keep[pointCount - 1] = true;

    douglas_peucker(points, 0, pointCount - 1, epsilon, keep);

    int newCount = 0;

    for (int i = 0; i < pointCount; ++i) {
        if (keep[i]) {
            points[newCount++] = points[i];
        }
    }

    return newCount;
}

int match_spell(const CandidateSegment *candidate, int candidateCount, const GestureReference &spell)
{
    if (candidateCount != spell.segmentCount) {
        return -1;
    }

    int score = 0;

    for (int i = 0; i < candidateCount; ++i) {

        int dirError = angle_diff(
            candidate[i].direction,
            spell.segments[i].direction
        );

        int lenError = abs(
            static_cast<int>(candidate[i].length) -
            static_cast<int>(spell.segments[i].length)
        );

        Serial.printf(" Spell name: %s\n", spell.name);
        Serial.printf(
            "    Seg %d: "
            "dir %d/%d err=%d°  "
            "len %d/%d err=%d\n",
            i,
            candidate[i].direction,
            spell.segments[i].direction,
            dirError,
            static_cast<int>(candidate[i].length),
            spell.segments[i].length,
            lenError
        );

        if (dirError > MAX_DIR_ERROR) {
            return -1;
        }

        if (lenError > MAX_LEN_ERROR) {
            return -1;
        }

        score += (MAX_DIR_ERROR - dirError) * 100;

        score += (MAX_LEN_ERROR - lenError);
    }

    return score;
}


const GestureReference* recognize_spell(
    const CandidateSegment *candidate,
    int candidateCount,
    int *bestScore)
{
    const GestureReference *bestSpell = nullptr;
    *bestScore = INT_MIN;

    for (size_t i = 0; i < kGestureReferenceCount; ++i) {

        const GestureReference &spell =
            kGestureReferences[i];

        int score = match_spell(
            candidate,
            candidateCount,
            spell
        );

        if (score < 0) {
            continue;
        }

        Serial.printf(
            "  %s score=%d\n",
            spell.name,
            score
        );

        if (score > *bestScore) {
            *bestScore = score;
            bestSpell = &spell;
        }
    }

    return bestSpell;
}

point_t points[1000];
#ifdef DEBUG_GESTURE_POINTS
void test_gesture_recognition(int pointCount)
{
    Serial.println();
    Serial.println("=== Gesture Recognition Test ===");

    CandidateSegment segments[10];
    int segmentCount = 0;

    create_segments(
        points,
        pointCount,
        segments,
        &segmentCount
    );

    normalize_segments(
        segments,
        segmentCount
    );

    Serial.printf(
        "Created %d segments from %d points\n",
        segmentCount,
        pointCount
    );

    Serial.println("Candidate:");

    for (int i = 0; i < segmentCount; ++i) {
        Serial.printf(
            "  Seg %d: dir=%d len=%d\n",
            i,
            segments[i].direction,
            static_cast<int>(segments[i].length)
        );
    }

    Serial.println("Matching:");

    int bestScore = 0;

    const GestureReference *bestSpell =
        recognize_spell(
            segments,
            segmentCount,
            &bestScore
        );

    if (bestSpell) {
        Serial.printf(
            "RESULT: %s (score=%d)\n",
            bestSpell->name,
            bestScore
        );
    } else {
        Serial.println("RESULT: no matching spell");
    }
}
void test_douglas_peucker()
{
    Serial.printf(
        "\n=== Douglas-Peucker Test ===\n"
        "Input points: %d\n"
        "Epsilon: 0.1\n",
        TEST_GESTURE_COUNT
    );

    for (int i = 0; i < TEST_GESTURE_COUNT; ++i) {
        points[i].x = testGesture[i].x;
        points[i].y = testGesture[i].y;
    }

    int simple_count = simplify_points(
        points,
        TEST_GESTURE_COUNT,
        0.1f
    );

    Serial.printf(
        "Result: %d -> %d points\n",
        TEST_GESTURE_COUNT,
        simple_count
    );

    for (int i = 0; i < simple_count; ++i) {
        Serial.printf(
            "Result %d: (%.3f, %.3f)\n",
            i,
            points[i].x,
            points[i].y
        );
    }
    test_gesture_recognition(simple_count);
}
#endif

int recognize_gesture()
{
    
    int count = DynamicData::get().sampleCounter;

    if (count < 2) {
        return 0;
    }

    for (int i = 0; i < count; ++i) {
        points[i].x = DynamicData::get().posX[i];
        points[i].y = DynamicData::get().posY[i];
    }

    // print points for debugging
    #ifdef DEBUG_GESTURE_POINTS
    Serial.println("Gesture points:");
    for (int i = 0; i < count; ++i) {
        Serial.printf("Point %d: (%.3f, %.3f)\n", i, points[i].x, points[i].y);
    }
    #endif
    int simple_count = simplify_points(points, count, 0.1f);
    
    Serial.printf("Gesture simplified from %d to %d points\n", count, simple_count);
    for (int i = 0; i < simple_count; ++i) {
        Serial.printf(
            "Simplified Point %d: (%.3f, %.3f)\n",
            i,
            points[i].x,
            points[i].y
        );
        if (i < 20) {
            DynamicData::get().posXreduced[i] = points[i].x;
            DynamicData::get().posYreduced[i] = points[i].y;
            DynamicData::get().reducedSampleCounter = i + 1;
        }
    }

    CandidateSegment segments[10];
    int segmentCount = 0;

    create_segments(
        points,
        simple_count,
        segments,
        &segmentCount
    );

    normalize_segments(
        segments,
        segmentCount
    );

    Serial.printf(
        "Created %d segments\n",
        segmentCount
    );
    
    for (int i = 0; i < segmentCount; ++i) {
        Serial.printf(
            "Segment %d: dir=%d len=%d\n",
            i,
            segments[i].direction,
            static_cast<int>(segments[i].length)
        );
    }

    int bestScore = 0;

    const GestureReference *spell =
        recognize_spell(
            segments,
            segmentCount,
            &bestScore
        );

    if (!spell) {
        Serial.println("No matching spell");
        return 0;
    }

    broadcastSpell(spell->name);

    int spellIndex =
        spell - kGestureReferences;

    Serial.printf(
        "Recognized spell: %s (index=%d, score=%d)\n",
        spell->name,
        spellIndex,
        bestScore
    );
    return spellIndex;
}

void gestureComplete()
{
    if (!DynamicData::get().dataValid) {
        Serial.println("No valid gesture data to recognize.");
        return;
    }
    const int sampleCount = DynamicData::get().sampleCounter;
    if (sampleCount < 10) {
        Serial.printf("Not enough samples for gesture recognition: %d\n", sampleCount);
        return;
    }
    const int spellIndex = recognize_gesture();
    if (spellIndex > 0) {
        Serial.printf("Gesture Complete - Recognized spell index: %d\n", spellIndex);
        DynamicData::get().lastBLEEvent = "Gesture Complete - Recognized spell index: " + String(spellIndex);
        spellFound = true;
    } else {
        Serial.println("No matching spell recognized.");
        DynamicData::get().lastBLEEvent = "No matching spell recognized.";
        spellFound = false;
    }
}

void add_projection_point(float x, float y)
{
    int arrayIndex = DynamicData::get().sampleCounter;
    if (arrayIndex >= 1000) {
        Serial.println("Projection point array full; discarding point.");
        DynamicData::get().dataValid = false;
        return;
    }
    DynamicData::get().sampleCounter++;
    DynamicData::get().posX[arrayIndex] = x;
    DynamicData::get().posY[arrayIndex] = y;
    DynamicData::get().dataValid = true;
}

float gestureAXFilter = 0;
float gestureAZFilter = 0;
float gestureGxSum = 0;
float gestureGzSum = 0;

bool initializeHapticHardware()
{
    if (writeCharacteristic == nullptr)
        return false;

    static const uint8_t cmds[][3] = {
        {0x00},
        {0x08},
        {0x09},

        {0x0e, 0x02},
        {0x0e, 0x04},
        {0x0e, 0x08},
        {0x0e, 0x09},
        {0x0e, 0x01},

        {0xdd, 0x00},
        {0xdd, 0x04},
        {0xdd, 0x01},
        {0xdd, 0x05},
        {0xdd, 0x02},
        {0xdd, 0x06},
        {0xdd, 0x03},
        {0xdd, 0x07},

        {0xdc, 0x00, 0x07},
        {0xdc, 0x04, 0x0a},
        {0xdc, 0x01, 0x07},
        {0xdc, 0x05, 0x0a},
        {0xdc, 0x02, 0x07},
        {0xdc, 0x06, 0x0a},
        {0xdc, 0x03, 0x07},
        {0xdc, 0x07, 0x0a},

        {0xdd, 0x00},
        {0xdd, 0x04},
        {0xdd, 0x01},
        {0xdd, 0x05},
        {0xdd, 0x02},
        {0xdd, 0x06},
        {0xdd, 0x03},
        {0xdd, 0x07}
    };

    static const uint8_t lengths[] = {
        1, 1, 1,

        2, 2, 2, 2, 2,

        2, 2, 2, 2,
        2, 2, 2, 2,

        3, 3, 3, 3,
        3, 3, 3, 3,

        2, 2, 2, 2,
        2, 2, 2, 2
    };

    constexpr size_t commandCount =
        sizeof(lengths) / sizeof(lengths[0]);

    for (size_t i = 0; i < commandCount; ++i)
    {
        if (!writeCharacteristic->writeValue(
                cmds[i],
                lengths[i],
                true))
        {
            Serial.printf(
                "Haptic init failed at command %u\n",
                static_cast<unsigned>(i)
            );
            return false;
        }
    }

    delay(100);

    Serial.println("Haptic hardware initialized.");
    return true;
}

bool sendWandCommand(size_t commandIndex)
{
    if (writeCharacteristic == nullptr)
        return false;

    const uint8_t commands[][8] = {
        {0x30, 0x00, 0x80}, // Command 0
        {0x10, 0x01}, // Command 1
        {0x60}, // Command 2
        {0x40}, // Command 3
        {0xc8}, // Command 4
        {0x68, 0x50, 0x64, 0x00}, // Command 5
        {0x68, 0x50, 0xc8, 0x00}, // Command 6
        {0x68, 0x22, 0x00, 0x00, 0x00, 0xff, 0x10, 0x00}, // Command 7
        {0x68, 0x22, 0x00, 0x00, 0xff, 0x00, 0x10, 0x00}, // Command 8
        {0x68, 0x22, 0x00, 0xff, 0x00, 0x00, 0x10, 0x00}, // Command 9
        {0x68, 0x22, 0x01, 0x00, 0xff, 0x00, 0x20, 0x03}, // Command 10
        {0x68, 0x22, 0x02, 0x00, 0x00, 0xff, 0x20, 0x03}, // Command 11
        {0x68, 0x22, 0x03, 0xff, 0xff, 0xff, 0x00, 0x01}, // Command 12
        {0x68, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f}, // Command 13
        {0x68, 0x22, 0x01, 0x00, 0x00, 0x00, 0x00, 0x04}, // Command 14
        {0x68, 0x22, 0x02, 0x00, 0x00, 0x00, 0x00, 0x04}, // Command 15
        {0x68, 0x22, 0x03, 0x00, 0x00, 0x00, 0x00, 0x04} // Command 16
    };

    const size_t commandLengths[] = {3, 2, 1, 1, 1, 4, 4, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8};

    
        Serial.printf("Sending command %u.\n", commandIndex);

        if (!writeCharacteristic->writeValue(commands[commandIndex], commandLengths[commandIndex], true)) {
            Serial.printf("Command %u failed.\n", commandIndex);
            return false;
        }
       
    return true;
}

void notificationHandler(NimBLERemoteCharacteristic *, uint8_t *data, size_t length, bool)
{
    if (length == 0) return;
    DynamicData::get().notifyCounter++;
    DynamicData::get().lastNotifyTime = millis();
    if (data[0] == 0x10 && length >= 2) {
        DynamicData::get().finger = data[1];
        if (DynamicData::get().finger != DynamicData::get().lastFinger) {
            
            if (DynamicData::get().finger == 0x00) {
                DynamicData::get().lastBLEEvent = "Finger released";
                DynamicData::get().lastFinger = DynamicData::get().finger;
                DynamicData::get().gestureRecording = false;
                Serial.printf("Gesture recording stopped; %d samples recorded.\n", DynamicData::get().sampleCounter);
                gestureComplete();
                fingerPressed = false;
            } else if (DynamicData::get().finger == 0x0f) {
                DynamicData::get().lastBLEEvent = "Finger pressed";
                DynamicData::get().lastFinger = DynamicData::get().finger;
                DynamicData::get().sampleCounter = 0;
                DynamicData::get().gestureRecording = true;
                Serial.println("Gesture recording started.");
                fingerPressed = true;
            }
        }
        return;
    }
    if (data[0] != 0x2c || length < 4) return;
    const uint8_t sampleCount = data[3];
    if (length < 4 + static_cast<size_t>(sampleCount) * 12) return;
    lastImuDataAt = millis();
    for (uint8_t i = 0; i < sampleCount; ++i) {
        const uint8_t *sample = data + 4 + i * 12;
        auto readInt16 = [](const uint8_t *value) {
            return static_cast<int16_t>(value[0] | (static_cast<uint16_t>(value[1]) << 8));
        };
        // Same offsets as the proven capture program, test.py.
        DynamicData::get().gx = readInt16(sample + 0) + 63;
        DynamicData::get().gy = readInt16(sample + 2);
        DynamicData::get().gz = readInt16(sample + 4) - 10;
        DynamicData::get().ax = readInt16(sample + 6);
        DynamicData::get().ay = readInt16(sample + 8);
        DynamicData::get().az = readInt16(sample + 10);
        DynamicData::get().imuSampleCount++;

        if (DynamicData::get().gestureRecording) {
            float ax_raw = DynamicData::get().ax;
            float az_raw = DynamicData::get().az;
            gestureAXFilter = (gestureAXFilter * 99 + ax_raw) / 100;
            gestureAZFilter = (gestureAZFilter * 99 + az_raw) / 100;
            gestureGxSum += DynamicData::get().gx;
            gestureGzSum += DynamicData::get().gz;
        
            float ax_f = gestureAXFilter;
            float az_f = gestureAZFilter;
            if (ax_f == 0 && az_f == 0) {
                gestureGxSum = 0;
                gestureGzSum = 0;
                continue;
            }
            float projection_angle = atan2f(static_cast<float>(az_f), static_cast<float>(-ax_f));
            float gx_sum = static_cast<float>(gestureGxSum);
            float gz_sum = static_cast<float>(gestureGzSum);
            float gx_sum_rot = gx_sum * cosf(projection_angle) - gz_sum * sinf(projection_angle);
            float gz_sum_rot = gx_sum * sinf(projection_angle) + gz_sum * cosf(projection_angle);
            float px = (gx_sum_rot) / 100000.0f;
            float py = (gz_sum_rot) / 100000.0f;
            add_projection_point(px, py);
        } else {
            gestureGxSum = 0;
            gestureGzSum = 0;
        }
    }
}

class WandClientCallbacks : public NimBLEClientCallbacks
{
    void onDisconnect(NimBLEClient *, int) override {
        Serial.println("Wand disconnected; scheduling another scan.");
        DynamicData::get().connected = false;
        DynamicData::get().clientConnected = false;
        DynamicData::get().disconnectCounter++;
        DynamicData::get().lastBLEEvent = "Wand disconnected; scanning again";
        scanRequested = true;
    }
    void onConnectFail(NimBLEClient *, int) override {
        Serial.println("Wand connection attempt failed; scheduling another scan.");
        DynamicData::get().connected = false;
        DynamicData::get().lastBLEEvent = "Wand connection failed; scanning again";
        scanRequested = true;
    }
};

bool initializeWand()
{
    for (size_t i = 0; i < 3; ++i)
    {
        Serial.printf("Sending initialization command %u.\n", i + 1);

        if (!sendWandCommand(i)) {
            Serial.printf("Initialization command %u failed.\n", i + 1);
            return false;
        }
        delay(20);
    }
    Serial.println("Wand initialized successfully.");
    DynamicData::get().lastBLEEvent = "Wand initialized successfully";
    return true;
}

bool connectSelectedWand()
{
    connectPending = false;
    DynamicData::get().bleConnectAttempts++;
    DynamicData::get().lastBLEEvent = "Connecting to selected wand";
    Serial.printf("Connecting to selected wand %s (attempt %u).\n", selectedAddress.toString().c_str(), DynamicData::get().bleConnectAttempts);
    if (wandClient == nullptr) {
        wandClient = NimBLEDevice::createClient();
        wandClient->setClientCallbacks(new WandClientCallbacks(), true);
        wandClient->setConnectTimeout(30 * 1000);
    }
    const uint32_t connectStartedAt = millis();
    if (!wandClient->connect(selectedAddress)) {
        Serial.printf("Connection failed after %lu ms, NimBLE error %d (timeout).\n",
                      millis() - connectStartedAt, wandClient->getLastError());
        DynamicData::get().lastBLEEvent = "Could not connect to selected wand";
        scanRequested = true;
        return false;
    }
    NimBLERemoteService *service = wandClient->getService(SERVICE_UUID);
    if (service == nullptr) {
        Serial.println("Connected, but service 57420001-587E-48A0-974C-544D6163C577 was not found.");
        DynamicData::get().lastBLEEvent = "Wand notify service not found";
        wandClient->disconnect();
        return false;
    }
    notifyCharacteristic = service->getCharacteristic(SERVICE_NOTIFY);
    writeCharacteristic = service->getCharacteristic(SERVICE_WRITE);
    if (notifyCharacteristic == nullptr || writeCharacteristic == nullptr || !notifyCharacteristic->canNotify() || !writeCharacteristic->canWrite()) {
        Serial.printf("Characteristics: notify=%p write=%p notify-capable=%d write-capable=%d\n", notifyCharacteristic, writeCharacteristic,
                      notifyCharacteristic != nullptr && notifyCharacteristic->canNotify(),
                      writeCharacteristic != nullptr && writeCharacteristic->canWrite());
        DynamicData::get().lastBLEEvent = "Wand characteristics not available";
        wandClient->disconnect();
        return false;
    }
    if (!notifyCharacteristic->subscribe(true, notificationHandler, true)) {
        Serial.println("Notification subscription failed.");
        DynamicData::get().lastBLEEvent = "Wand notification subscription failed";
        wandClient->disconnect();
        return false;
    }
    if (!initializeWand()) {
        Serial.println("Wand initialization failed.");
        DynamicData::get().lastBLEEvent = "Wand initialization failed";
        wandClient->disconnect();
        return false;
    }
    //initializeHapticHardware();
    lastImuDataAt = millis();
    DynamicData::get().connected = true;
    DynamicData::get().clientConnected = true;
    DynamicData::get().connections++;
    DynamicData::get().lastBLEEvent = "Connected to selected wand; waiting for IMU data";
    Serial.println("Wand connected, subscribed, and initialized. Waiting for IMU data.");
    return true;
}
} // namespace

void setup()
{
    Serial.begin(115200);
    Serial.println("Starting wand detection...");
    NVMData::get().Init();
    DynamicData::get().Init();
    Serial.println("NetName: " + NVMData::get().GetNetName());
    setupWiFi();
    WebPage::get().Init();

    NimBLEDevice::init("");
    NimBLEScan *scan = NimBLEDevice::getScan();
    scan->setScanCallbacks(new WandScanCallbacks());
    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(80);
    scanRequested = true;
    #ifdef DEBUG_GESTURE_POINTS
    test_douglas_peucker();
    #endif
}

void checkNetworkSet()
{
    if (DynamicData::get().setNewNetwork == true)
    {
        if (DynamicData::get().newNetworkSet == true)
        {
            DynamicData::get().setNewNetwork = false;
            DynamicData::get().newNetworkSet = false;
            NVMData::get().StoreNetData();
        }
    }
}
void loop()
{
    WebPage::get().loop();
    checkNetworkSet();
    if (DynamicData::get().wandSelectionChanged) {
        DynamicData::get().wandSelectionChanged = false;
        if (wandClient != nullptr && wandClient->isConnected()) wandClient->disconnect();
        scanRequested = true;
    }
    if (connectPending) connectSelectedWand();
    if (scanRequested && !NimBLEDevice::getScan()->isScanning()) {
        scanRequested = false;
        startScan(true);
    }
    if (DynamicData::get().connected &&
        !NimBLEDevice::getScan()->isScanning() &&
        millis() - lastWandScan >= WAND_RESCAN_INTERVAL_MS)
    {
        lastWandScan = millis();
        startScan(false);
    }
    if (DynamicData::get().connected && millis() - lastImuDataAt >= IMU_INIT_TIMEOUT_MS) {
        Serial.println("No IMU data received after initialization. Retrying initialization.");

        lastImuDataAt = millis();

        if (!initializeWand())
        {
            Serial.println("Retry initialization failed.");
            DynamicData::get().lastBLEEvent = "Wand initialization failed after timeout";
        }
    }
    if (fingerPressed != oldFingerPressed) {
        oldFingerPressed = fingerPressed;
        if (fingerPressed) {
            if (NVMData::get().GetVibration()) {
                sendWandCommand(5);
            }
            sendWandCommand(7);
        } else {
            if (NVMData::get().GetVibration()) {
                sendWandCommand(6);
            }
            if (spellFound) {
                sendWandCommand(8);
            } else {
                sendWandCommand(9);
            }
            sendWandCommand(13);
        }
    }   
    delay(10);
}
