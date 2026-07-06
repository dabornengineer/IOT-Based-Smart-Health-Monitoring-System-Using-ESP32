#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

// ── Config ────────────────────────────────────────────────────────────────────
#define LOG_DIR          "/sessions"
#define MAX_SESSIONS     20
#define LOG_INTERVAL_MS  1000          // log every 1 second when active

// ── Session states ────────────────────────────────────────────────────────────
typedef enum {
    SESSION_IDLE    = 0,   // no session started
    SESSION_RUNNING = 1,   // actively logging
    SESSION_PAUSED  = 2,   // paused, file still open
} SessionState_t;

// ── Session metadata ──────────────────────────────────────────────────────────
typedef struct {
    char     filename[32];    // e.g. "session_001.csv"
    char     startTime[16];   // uptime HH:MM:SS when session started
    uint32_t rowCount;
    uint32_t sizeBytes;
} SessionInfo_t;

// ── Lifecycle ─────────────────────────────────────────────────────────────────
void initLogger(void);              // call after LittleFS mounted

// ── Session control ───────────────────────────────────────────────────────────
bool        startSession(void);     // create new session file, begin logging
void        pauseSession(void);     // pause — file stays, logging stops
void        resumeSession(void);    // resume into same session
bool        restartSession(void);   // close current, open new session file
SessionState_t getSessionState(void);

// ── Logging ───────────────────────────────────────────────────────────────────
void logTick(void);                 // call from a FreeRTOS task every 1 s

// ── Session list ──────────────────────────────────────────────────────────────
int  listSessions(SessionInfo_t *out, int maxCount);  // returns count
bool deleteSession(const char *filename);
void deleteAllSessions(void);

// ── Storage info ──────────────────────────────────────────────────────────────
size_t getUsedBytes(void);
size_t getTotalBytes(void);

// ── FreeRTOS task — spawn from app_main at priority 1 ────────────────────────
void logTask(void *pvParameters);

// ── Active session name ───────────────────────────────────────────────────────
const char* getActiveSession(void);

#endif