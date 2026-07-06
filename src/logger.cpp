#include "logger.h"
#include "display.h"
#include <LittleFS.h>
#include <Arduino.h>

// ── Internal state ────────────────────────────────────────────────────────────
static SessionState_t g_state         = SESSION_IDLE;
static char           g_activeFile[48] = {0};
static char           g_startTime[16]  = {0};
static uint32_t       g_rowCount       = 0;
static unsigned long  g_lastLog        = 0;

// ── Helpers ───────────────────────────────────────────────────────────────────
static String uptime(void)
{
    unsigned long s = millis() / 1000;
    unsigned long m = s / 60, h = m / 60;
    s %= 60; m %= 60;
    char buf[12];
    snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", h, m, s);
    return String(buf);
}

// Find the next available session number
static int nextSessionNumber(void)
{
    int highest = 0;
    File dir = LittleFS.open(LOG_DIR);
    if (!dir || !dir.isDirectory()) return 1;
    File f = dir.openNextFile();
    while (f)
    {
        String name = String(f.name());
        // name is just the filename inside the dir e.g. "session_003.csv"
        if (name.startsWith("session_") && name.endsWith(".csv"))
        {
            int n = name.substring(8, 11).toInt();
            if (n > highest) highest = n;
        }
        f = dir.openNextFile();
    }
    return highest + 1;
}

static void writeHeader(File &f, const char *startTime)
{
    f.println("sep=,");  // Excel hint
    f.println("index,uptime,elapsed_s,bpm,spo2_pct,core_temp_c,notes");
}

// ── initLogger ────────────────────────────────────────────────────────────────
void initLogger(void)
{
    if (!LittleFS.exists(LOG_DIR))
        LittleFS.mkdir(LOG_DIR);

    Serial.printf("[LOG] Ready. Sessions dir: %s  Used: %u / %u bytes\n",
        LOG_DIR, getUsedBytes(), getTotalBytes());
}

// ── startSession ─────────────────────────────────────────────────────────────
bool startSession(void)
{
    if (g_state == SESSION_RUNNING) return false;

    int num = nextSessionNumber();
    snprintf(g_activeFile, sizeof(g_activeFile),
             "%s/session_%03d.csv", LOG_DIR, num);

    File f = LittleFS.open(g_activeFile, "w");
    if (!f)
    {
        Serial.printf("[LOG] ERROR: cannot create %s\n", g_activeFile);
        return false;
    }

    String t = uptime();
    strncpy(g_startTime, t.c_str(), sizeof(g_startTime));
    writeHeader(f, g_startTime);
    f.close();

    g_rowCount = 0;
    g_lastLog  = 0;
    g_state    = SESSION_RUNNING;

    Serial.printf("[LOG] Session started: %s\n", g_activeFile);
    return true;
}

// ── pauseSession ──────────────────────────────────────────────────────────────
void pauseSession(void)
{
    if (g_state != SESSION_RUNNING) return;
    g_state = SESSION_PAUSED;
    Serial.printf("[LOG] Session paused: %s (%lu rows)\n", g_activeFile, g_rowCount);
}

// ── resumeSession ─────────────────────────────────────────────────────────────
void resumeSession(void)
{
    if (g_state != SESSION_PAUSED) return;
    g_state = SESSION_RUNNING;
    Serial.printf("[LOG] Session resumed: %s\n", g_activeFile);
}

// ── restartSession ────────────────────────────────────────────────────────────
bool restartSession(void)
{
    // Close current (keep it), open a new one
    g_state = SESSION_IDLE;
    return startSession();
}

// ── getSessionState ───────────────────────────────────────────────────────────
SessionState_t getSessionState(void) { return g_state; }

// ── getActiveSession ──────────────────────────────────────────────────────────
const char* getActiveSession(void) { return g_activeFile; }

// ── logTick — call every 1 s from a task ─────────────────────────────────────
void logTick(void)
{
    if (g_state != SESSION_RUNNING) return;

    unsigned long now = millis();
    if (now - g_lastLog < LOG_INTERVAL_MS) return;
    g_lastLog = now;

    // Snapshot globals (no mutex needed for single-word reads on ESP32)
    int   bpm      = g_bpm;
    int   spo2     = g_spo2;
    float coreTemp = g_coreTemp;
    bool  tempRdy  = g_tempReady;
    bool  leadsOff = g_leadsOff;
    bool  fingerOn = g_fingerOn;

    // Skip row if everything is invalid
    if (leadsOff && !fingerOn && !tempRdy) return;

    // Build notes
    String notes = "";
    if (leadsOff)                          notes += "leads_off ";
    if (!fingerOn)                         notes += "no_finger ";
    if (bpm > 100)                         notes += "high_hr ";
    if (bpm > 0 && bpm < 50)              notes += "low_hr ";
    if (spo2 > 0 && spo2 < 95)           notes += "low_spo2 ";
    if (tempRdy && coreTemp >= 38.0f)     notes += "fever ";
    if (notes.length() == 0)              notes  = "ok";
    notes.trim();

    // Elapsed seconds since session start
    unsigned long elapsed = now / 1000;

    File f = LittleFS.open(g_activeFile, "a");
    if (!f) return;

    char row[128];
    snprintf(row, sizeof(row), "%lu,%s,%lu,%s,%s,%s,%s",
        g_rowCount++,
        uptime().c_str(),
        elapsed,
        (bpm > 0 && !leadsOff)  ? String(bpm).c_str()         : "",
        (spo2 > 0 && fingerOn)  ? String(spo2).c_str()        : "",
        (tempRdy)               ? String(coreTemp,1).c_str()  : "",
        notes.c_str()
    );
    f.println(row);
    f.close();
}

// ── listSessions ──────────────────────────────────────────────────────────────
int listSessions(SessionInfo_t *out, int maxCount)
{
    int count = 0;
    File dir = LittleFS.open(LOG_DIR);
    if (!dir || !dir.isDirectory()) return 0;

    File f = dir.openNextFile();
    while (f && count < maxCount)
    {
        String name = String(f.name());
        if (name.startsWith("session_") && name.endsWith(".csv"))
        {
            SessionInfo_t &s = out[count++];
            strncpy(s.filename, name.c_str(), sizeof(s.filename));
            s.sizeBytes = f.size();
            s.rowCount  = s.sizeBytes / 50;  // rough estimate
            strncpy(s.startTime, "—", sizeof(s.startTime));
        }
        f = dir.openNextFile();
    }
    return count;
}

// ── deleteSession ─────────────────────────────────────────────────────────────
bool deleteSession(const char *filename)
{
    char path[48];
    snprintf(path, sizeof(path), "%s/%s", LOG_DIR, filename);
    bool ok = LittleFS.remove(path);
    Serial.printf("[LOG] Delete %s: %s\n", path, ok ? "ok" : "failed");
    return ok;
}

// ── deleteAllSessions ─────────────────────────────────────────────────────────
void deleteAllSessions(void)
{
    File dir = LittleFS.open(LOG_DIR);
    if (!dir || !dir.isDirectory()) return;
    File f = dir.openNextFile();
    while (f)
    {
        String name = String(f.name());
        if (name.endsWith(".csv"))
        {
            char path[48];
            snprintf(path, sizeof(path), "%s/%s", LOG_DIR, name.c_str());
            LittleFS.remove(path);
        }
        f = dir.openNextFile();
    }
    g_state = SESSION_IDLE;
    Serial.println("[LOG] All sessions deleted");
}

// ── Storage info ──────────────────────────────────────────────────────────────
size_t getUsedBytes(void)  { return LittleFS.usedBytes(); }
size_t getTotalBytes(void) { return LittleFS.totalBytes(); }

// ── logTask — spawn from app_main at priority 1 ───────────────────────────────
void logTask(void *pvParameters)
{
    while (1)
    {
        logTick();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}