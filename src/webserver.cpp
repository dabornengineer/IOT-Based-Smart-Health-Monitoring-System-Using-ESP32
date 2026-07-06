/*#include "webserver.h"
#include "display.h"      // shared globals: g_bpm, g_spo2, g_palmTemp, etc.
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <LittleFS.h>  
#include <Arduino.h>

// ── AP credentials ────────────────────────────────────────────────────────────
#define AP_SSID     "ESP32-HealthMonitor"
#define AP_PASSWORD "12345678"            // min 8 chars

// ── ECG circular buffer (written by ad8232 task via pushEcgSample) ────────────
int                g_ecgBuf[ECG_BUF_SIZE] = {0};
volatile int       g_ecgHead              = 0;
SemaphoreHandle_t  g_webMutex             = nullptr;

static AsyncWebServer server(80);
void initWebserver(void);

// ─────────────────────────────────────────────────────────────────────────────
// Call from readAd8232Values() task instead of writing g_ecgRaw directly.
// This keeps the circular buffer filled without any extra task overhead.
// ─────────────────────────────────────────────────────────────────────────────
void pushEcgSample(int value)
{
    // No mutex needed — single writer, reader takes snapshot under mutex
    g_ecgBuf[g_ecgHead] = value;
    g_ecgHead = (g_ecgHead + 1) % ECG_BUF_SIZE;
}

// ─────────────────────────────────────────────────────────────────────────────
// /data  — vitals JSON, polled every 1 s
// ─────────────────────────────────────────────────────────────────────────────
static void handleData(AsyncWebServerRequest *req)
{
    int   bpm      = -1;
    int   spo2     = -1;
    float palmTemp = 0.0f;
    float coreTemp = 0.0f;
    bool  leadsOff = true;
    bool  fingerOn = false;
    bool  measuring = false;
    bool  tempReady = false;
    int   countdown = 0;

    if (xSemaphoreTake(g_webMutex, pdMS_TO_TICKS(30)) == pdTRUE)
    {
        bpm      = g_bpm;
        spo2     = g_spo2;
        palmTemp = g_palmTemp;
        coreTemp = g_coreTemp;
        leadsOff = g_leadsOff;
        fingerOn = g_fingerOn;
        measuring = g_measuring;
        tempReady = g_tempReady;
        countdown = g_countdown;
        xSemaphoreGive(g_webMutex);
    }

    char json[256];
    snprintf(json, sizeof(json),
        "{"
            "\"bpm\":%d,"
            "\"spo2\":%d,"
            "\"palmTemp\":%.1f,"
            "\"coreTemp\":%.1f,"
            "\"leadsOff\":%s,"
            "\"fingerOn\":%s,"
            "\"measuring\":%s,"
            "\"tempReady\":%s,"
            "\"countdown\":%d"
        "}",
        bpm, spo2,
        palmTemp, coreTemp,
        leadsOff  ? "true" : "false",
        fingerOn  ? "true" : "false",
        measuring ? "true" : "false",
        tempReady ? "true" : "false",
        countdown
    );

    req->send(200, "application/json", json);
}

// ─────────────────────────────────────────────────────────────────────────────
// /ecg  — 200-sample waveform array, polled every 100 ms
// Returns samples in chronological order (oldest → newest)
// ─────────────────────────────────────────────────────────────────────────────
static void handleEcg(AsyncWebServerRequest *req)
{
    int snapshot[ECG_BUF_SIZE];
    int head = 0;

    if (xSemaphoreTake(g_webMutex, pdMS_TO_TICKS(30)) == pdTRUE)
    {
        head = g_ecgHead;
        memcpy(snapshot, g_ecgBuf, sizeof(g_ecgBuf));
        xSemaphoreGive(g_webMutex);
    }

    // Build compact JSON array, oldest sample first
    String json;
    json.reserve(ECG_BUF_SIZE * 5 + 4);
    json = "[";
    for (int i = 0; i < ECG_BUF_SIZE; i++)
    {
        int idx = (head + i) % ECG_BUF_SIZE;
        json += snapshot[idx];
        if (i < ECG_BUF_SIZE - 1) json += ',';
    }
    json += "]";

    req->send(200, "application/json", json);
}


// ─────────────────────────────────────────────────────────────────────────────
// initWebserver — called by webTask; do not call directly from app_main
// ─────────────────────────────────────────────────────────────────────────────
void initWebserver(void)
{
    // Mutex protects shared globals read by web callbacks
    g_webMutex = xSemaphoreCreateMutex();

    // ── Mount LittleFS ────────────────────────────────────────────────────────
    if (!LittleFS.begin(true))   // true = format on first boot if blank
    {
        Serial.println("[WEB] LittleFS mount failed");
        return;
    }
    Serial.println("[WEB] LittleFS mounted");

    // ── Start AP ──────────────────────────────────────────────────────────────
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    Serial.printf("[WEB] AP started — SSID: %s  IP: %s\n",
                  AP_SSID, WiFi.softAPIP().toString().c_str());

    // ── Routes ────────────────────────────────────────────────────────────────
    server.on("/data", HTTP_GET, handleData);
    server.on("/ecg",  HTTP_GET, handleEcg);

    // Serve all static files from LittleFS; index.html is the default
    server.serveStatic("/", LittleFS, "/")
          .setDefaultFile("index.html")
          .setCacheControl("max-age=3600");   // CSS/JS cached 1 h

    server.onNotFound([](AsyncWebServerRequest *req) {
        req->send(404, "text/plain", "not found");
    });

    server.begin();
    Serial.println("[WEB] Server listening on port 80");
}

// ─────────────────────────────────────────────────────────────────────────────
// webTask — spawn from app_main after all sensors are inited.
// Runs at priority 5. Stack 8192 is sufficient for LittleFS + AsyncTCP.
// After init it self-deletes — AsyncTCP manages its own internal tasks.
// ─────────────────────────────────────────────────────────────────────────────
void webTask(void *pvParameters)
{
    initWebserver();
    vTaskDelete(NULL);
}*/

#include "webserver.h"
#include "display.h"
#include "logger.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <Arduino.h>

#define AP_SSID     "ESP32-HealthMonitor"
#define AP_PASSWORD "12345678"

int                g_ecgBuf[ECG_BUF_SIZE] = {0};
volatile int       g_ecgHead              = 0;
SemaphoreHandle_t  g_webMutex             = nullptr;

static AsyncWebServer server(80);

void pushEcgSample(int value)
{
    g_ecgBuf[g_ecgHead] = value;
    g_ecgHead = (g_ecgHead + 1) % ECG_BUF_SIZE;
}

// ── /data ─────────────────────────────────────────────────────────────────────
static void handleData(AsyncWebServerRequest *req)
{
    int   bpm = -1, spo2 = -1, countdown = 0;
    float palmTemp = 0, coreTemp = 0;
    bool  leadsOff = true, fingerOn = false, measuring = false, tempReady = false;

    if (xSemaphoreTake(g_webMutex, pdMS_TO_TICKS(30)) == pdTRUE)
    {
        bpm       = g_bpm;      spo2      = g_spo2;
        palmTemp  = g_palmTemp; coreTemp  = g_coreTemp;
        leadsOff  = g_leadsOff; fingerOn  = g_fingerOn;
        measuring = g_measuring; tempReady = g_tempReady;
        countdown = g_countdown;
        xSemaphoreGive(g_webMutex);
    }

    // Session state
    SessionState_t ss = getSessionState();
    const char* sessionState = (ss == SESSION_RUNNING) ? "running"
                             : (ss == SESSION_PAUSED)  ? "paused"
                             :                           "idle";

    size_t used  = getUsedBytes();
    size_t total = getTotalBytes();

    char json[512];
    snprintf(json, sizeof(json),
        "{"
          "\"bpm\":%d,\"spo2\":%d,"
          "\"palmTemp\":%.1f,\"coreTemp\":%.1f,"
          "\"leadsOff\":%s,\"fingerOn\":%s,"
          "\"measuring\":%s,\"tempReady\":%s,"
          "\"countdown\":%d,"
          "\"session\":\"%s\","
          "\"storageUsed\":%u,\"storageTotal\":%u"
        "}",
        bpm, spo2, palmTemp, coreTemp,
        leadsOff  ? "true":"false",
        fingerOn  ? "true":"false",
        measuring ? "true":"false",
        tempReady ? "true":"false",
        countdown,
        sessionState,
        used, total
    );
    req->send(200, "application/json", json);
}

// ── /ecg ──────────────────────────────────────────────────────────────────────
static void handleEcg(AsyncWebServerRequest *req)
{
    int snapshot[ECG_BUF_SIZE];
    int head = 0;

    if (xSemaphoreTake(g_webMutex, pdMS_TO_TICKS(30)) == pdTRUE)
    {
        head = g_ecgHead;
        memcpy(snapshot, g_ecgBuf, sizeof(g_ecgBuf));
        xSemaphoreGive(g_webMutex);
    }

    String json;
    json.reserve(ECG_BUF_SIZE * 5 + 4);
    json = "[";
    for (int i = 0; i < ECG_BUF_SIZE; i++)
    {
        json += snapshot[(head + i) % ECG_BUF_SIZE];
        if (i < ECG_BUF_SIZE - 1) json += ',';
    }
    json += "]";
    req->send(200, "application/json", json);
}

// ── /sessions — list all session files as JSON ────────────────────────────────
static void handleSessions(AsyncWebServerRequest *req)
{
    SessionInfo_t sessions[MAX_SESSIONS];
    int count = listSessions(sessions, MAX_SESSIONS);

    String json = "[";
    for (int i = 0; i < count; i++)
    {
        if (i > 0) json += ",";
        json += "{\"file\":\"" + String(sessions[i].filename) + "\","
              + "\"size\":"   + sessions[i].sizeBytes + ","
              + "\"rows\":"   + sessions[i].rowCount  + "}";
    }
    json += "]";
    req->send(200, "application/json", json);
}

// ── /download?file=session_001.csv ────────────────────────────────────────────
static void handleDownload(AsyncWebServerRequest *req)
{
    if (!req->hasParam("file"))
    {
        req->send(400, "text/plain", "missing file param");
        return;
    }
    String filename = req->getParam("file")->value();

    // Sanitise — only allow session_XXX.csv
    if (!filename.startsWith("session_") || !filename.endsWith(".csv"))
    {
        req->send(400, "text/plain", "invalid filename");
        return;
    }

    String path = String(LOG_DIR) + "/" + filename;
    if (!LittleFS.exists(path))
    {
        req->send(404, "text/plain", "file not found");
        return;
    }

    req->send(LittleFS, path, "text/csv",
              true,   // download (Content-Disposition: attachment)
              nullptr);
}

// ── /delete?file=session_001.csv ──────────────────────────────────────────────
static void handleDelete(AsyncWebServerRequest *req)
{
    if (!req->hasParam("file"))
    {
        req->send(400, "text/plain", "missing file param");
        return;
    }
    String filename = req->getParam("file")->value();
    if (!filename.startsWith("session_") || !filename.endsWith(".csv"))
    {
        req->send(400, "text/plain", "invalid filename");
        return;
    }
    bool ok = deleteSession(filename.c_str());
    req->send(200, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
}

// ── /session/start|pause|resume|restart ───────────────────────────────────────
static void handleSessionControl(AsyncWebServerRequest *req)
{
    String action = req->url();   // e.g. "/session/start"
    String result = "ok";

    if      (action == "/session/start")   { if (!startSession())   result = "already running"; }
    else if (action == "/session/pause")   { pauseSession(); }
    else if (action == "/session/resume")  { resumeSession(); }
    else if (action == "/session/restart") { if (!restartSession()) result = "failed"; }
    else                                   { req->send(400, "text/plain", "unknown action"); return; }

    req->send(200, "application/json",
              "{\"ok\":true,\"result\":\"" + result + "\","
              "\"session\":\"" + String(getActiveSession()) + "\"}");
}

// ── initWebserver ─────────────────────────────────────────────────────────────
void initWebserver(void)
{
    g_webMutex = xSemaphoreCreateMutex();

    if (!LittleFS.begin(true))
    {
        Serial.println("[WEB] LittleFS mount failed");
        return;
    }
    Serial.println("[WEB] LittleFS mounted");

    initLogger();

    WiFi.softAP(AP_SSID, AP_PASSWORD);
    Serial.printf("[WEB] AP: %s  IP: %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());

    // Vitals + ECG
    server.on("/data",  HTTP_GET, handleData);
    server.on("/ecg",   HTTP_GET, handleEcg);

    // Session list + download + delete
    server.on("/sessions",        HTTP_GET, handleSessions);
    server.on("/download",        HTTP_GET, handleDownload);
    server.on("/delete",          HTTP_GET, handleDelete);

    // Session control
    server.on("/session/start",   HTTP_POST, handleSessionControl);
    server.on("/session/pause",   HTTP_POST, handleSessionControl);
    server.on("/session/resume",  HTTP_POST, handleSessionControl);
    server.on("/session/restart", HTTP_POST, handleSessionControl);

    // Static files
    server.serveStatic("/", LittleFS, "/")
          .setDefaultFile("index.html")
          .setCacheControl("max-age=3600");

    server.onNotFound([](AsyncWebServerRequest *req){
        req->send(404, "text/plain", "not found");
    });

    server.begin();
    Serial.println("[WEB] Server ready on port 80");
}

void webTask(void *pvParameters)
{
    initWebserver();
    vTaskDelete(NULL);
}