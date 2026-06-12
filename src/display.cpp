/*#include "display.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>

// ── Display dimensions ────────────────────────────────────
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1   // no reset pin

// ── Screen cycling ────────────────────────────────────────
#define SCREEN_DWELL_MS   3000   // time per screen (ms)
#define NUM_SCREENS            3

// ── Global shared variables (defined here, extern'd in display.h) ──
volatile int     g_bpm        = -1;
volatile int     g_ecgRaw     = 0;
volatile bool    g_leadsOff   = true;

volatile int32_t g_spo2       = -1;
volatile int32_t g_hr         = -1;
volatile bool    g_fingerOn   = false;

volatile float   g_palmTemp   = 0.0f;
volatile float   g_coreTemp   = 0.0f;
volatile bool    g_tempReady  = false;
volatile bool    g_measuring  = false;
volatile int     g_countdown  = 60;

// ── Local ─────────────────────────────────────────────────
static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire1, OLED_RESET);

// ── Mini ECG waveform buffer (for visual sparkline) ───────
#define ECG_HISTORY  64
static int ecgHistory[ECG_HISTORY] = {0};
static int ecgHistIdx = 0;

// ─────────────────────────────────────────────────────────
//  Helper: draw a thin horizontal divider
// ─────────────────────────────────────────────────────────
static void drawDivider(int y)
{
    display.drawFastHLine(0, y, SCREEN_WIDTH, SSD1306_WHITE);
}

// ─────────────────────────────────────────────────────────
//  Screen 0 — ECG / BPM
//  Layout:
//    Row 0 (0–9):   "ECG" label + lead status
//    Row 1 (10–17): sparkline (raw waveform)
//    Divider (18)
//    Row 2 (20–63): large BPM value or status text
// ─────────────────────────────────────────────────────────
static void drawEcgScreen(void)
{
    display.clearDisplay();

    // ── Title + lead status ───────────────────────────────
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print("ECG");

    if (g_leadsOff)
    {
        display.setCursor(40, 0);
        display.print("[LEADS OFF]");
    }
    else
    {
        display.setCursor(40, 0);
        display.print("[CONNECTED]");
    }

    // ── Sparkline (raw ECG, scaled to 18px tall band) ────
    // Map ADC (0–4095) to row range 10–27 (inverted: high ADC = low pixel)
    const int sparkTop  = 10;
    const int sparkH    = 18;

    for (int i = 1; i < ECG_HISTORY; i++)
    {
        int x0 = i - 1;
        int x1 = i;
        // Use history ring: oldest at ecgHistIdx+1 wrapping
        int idx0 = (ecgHistIdx + i    ) % ECG_HISTORY;
        int idx1 = (ecgHistIdx + i + 1) % ECG_HISTORY;

        int v0 = ecgHistory[idx0];
        int v1 = ecgHistory[idx1];

        // Scale: 0–4095 → sparkTop+sparkH down to sparkTop
        int y0 = sparkTop + sparkH - (v0 * sparkH / 4095);
        int y1 = sparkTop + sparkH - (v1 * sparkH / 4095);

        // clamp
        y0 = constrain(y0, sparkTop, sparkTop + sparkH);
        y1 = constrain(y1, sparkTop, sparkTop + sparkH);

        display.drawLine(x0, y0, x1, y1, SSD1306_WHITE);
    }

    drawDivider(29);

    // ── BPM value ─────────────────────────────────────────
    display.setCursor(0, 32);

    if (g_leadsOff)
    {
        display.setTextSize(1);
        display.println("Attach leads to");
        display.println("measure ECG/BPM");
    }
    else if (g_bpm < 0)
    {
        display.setTextSize(1);
        display.setCursor(0, 35);
        display.println("Detecting...");
        display.setCursor(0, 47);
        display.setTextSize(1);
        display.println("Hold still");
    }
    else
    {
        // Big BPM number
        display.setTextSize(3);
        // Right-align: 3-digit number = 3*18 = 54px wide
        int bpmW = (g_bpm >= 100) ? 54 : (g_bpm >= 10 ? 36 : 18);
        display.setCursor(SCREEN_WIDTH - bpmW - 2, 33);
        display.print(g_bpm);

        display.setTextSize(1);
        display.setCursor(0, 45);
        display.println("BPM");
        display.setCursor(0, 55);
        display.println("Heart Rate");
    }

    display.display();
}

// ─────────────────────────────────────────────────────────
//  Screen 1 — SpO2 + Heart Rate (MAX30102)
//  Layout:
//    Row 0 (0–9):   "SpO2 / HR" label
//    Divider (10)
//    Left half:  SpO2 %
//    Right half: HR bpm
// ─────────────────────────────────────────────────────────
static void drawSpo2Screen(void)
{
    display.clearDisplay();

    // ── Title ─────────────────────────────────────────────
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(22, 0);
    display.print("SpO2  /  HR");

    drawDivider(10);

    if (!g_fingerOn)
    {
        display.setTextSize(1);
        display.setCursor(4, 20);
        display.println("Place finger on");
        display.setCursor(10, 32);
        display.println("MAX30102...");
        display.display();
        return;
    }

    if (g_spo2 < 0 || g_hr < 0)
    {
        display.setTextSize(1);
        display.setCursor(8, 22);
        display.println("Stabilising...");
        display.setCursor(4, 34);
        display.println("Hold still please");
        display.display();
        return;
    }

    // ── SpO2 (left half) ──────────────────────────────────
    display.setTextSize(2);
    // Value
    int spo2X = (g_spo2 == 100) ? 0 : 4;
    display.setCursor(spo2X, 16);
    display.print(g_spo2);

    display.setTextSize(1);
    display.setCursor(2, 38);
    display.print("SpO2 %");

    // Vertical divider
    display.drawFastVLine(64, 11, 53, SSD1306_WHITE);

    // ── HR (right half) ───────────────────────────────────
    display.setTextSize(2);
    int hrX = (g_hr >= 100) ? 67 : 73;
    display.setCursor(hrX, 16);
    display.print(g_hr);

    display.setTextSize(1);
    display.setCursor(69, 38);
    display.print("HR bpm");

    // ── SpO2 status bar ───────────────────────────────────
    drawDivider(50);
    display.setCursor(0, 54);
    display.setTextSize(1);

    if (g_spo2 >= 95)        display.print("Normal");
    else if (g_spo2 >= 90)   display.print("Low-normal");
    else                     display.print("! LOW SpO2");

    display.display();
}

// ─────────────────────────────────────────────────────────
//  Screen 2 — Body Temperature (DS18B20)
//  Layout:
//    Row 0 (0–9):   "TEMP" label
//    Divider (10)
//    Content: state-dependent
// ─────────────────────────────────────────────────────────
static void drawTempScreen(void)
{
    display.clearDisplay();

    // ── Title ─────────────────────────────────────────────
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(35, 0);
    display.print("BODY TEMP");

    drawDivider(10);

    if (!g_measuring && !g_tempReady)
    {
        // Idle — waiting for touch
        display.setTextSize(1);
        display.setCursor(4, 16);
        display.println("Touch sensor to");
        display.setCursor(4, 28);
        display.println("start measurement");

        // Draw simple hand icon hint
        display.setCursor(50, 46);
        display.setTextSize(2);
        display.print(">");  // arrow hint
        display.display();
        return;
    }

    if (g_measuring)
    {
        // Countdown progress bar
        display.setTextSize(1);
        display.setCursor(8, 14);
        display.println("Hold sensor on palm");

        // Countdown number
        display.setTextSize(3);
        int cntX = (g_countdown >= 10) ? 46 : 55;
        display.setCursor(cntX, 26);
        display.print(g_countdown);
        display.setTextSize(1);
        display.setCursor(50, 26 + 24 + 2);
        display.print("sec");

        // Progress bar (0 → 60 seconds, bar fills across screen)
        int elapsed = 60 - g_countdown;
        int barW = (elapsed * (SCREEN_WIDTH - 4)) / 60;
        barW = constrain(barW, 0, SCREEN_WIDTH - 4);
        display.drawRect(2, 55, SCREEN_WIDTH - 4, 8, SSD1306_WHITE);
        display.fillRect(2, 55, barW, 8, SSD1306_WHITE);

        display.display();
        return;
    }

    if (g_tempReady)
    {
        // Result — show palm temp and core temp estimate
        display.setTextSize(1);
        display.setCursor(0, 13);
        display.print("Palm:");

        display.setTextSize(2);
        display.setCursor(38, 11);
        display.print(g_palmTemp, 1);
        display.print((char)247);  // degree symbol
        display.print("C");

        drawDivider(32);

        display.setTextSize(1);
        display.setCursor(0, 36);
        display.print("Core:");

        display.setTextSize(2);
        display.setCursor(38, 34);
        display.print(g_coreTemp, 1);
        display.print((char)247);
        display.print("C");

        // Fever warning
        drawDivider(54);
        display.setTextSize(1);
        display.setCursor(0, 56);
        if (g_coreTemp >= 37.5f)       display.print("! FEVER");
        else if (g_coreTemp >= 36.0f)  display.print("Normal");
        else                           display.print("Below normal");

        display.display();
    }
}

// ─────────────────────────────────────────────────────────
//  Init
// ─────────────────────────────────────────────────────────
void initDisplay(void)
{
    // Use custom I2C pins
    Wire1.begin(OLED_SDA, OLED_SCL);
    Wire1.setClock(100000);

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    {
        Serial.println("OLED FAIL - HALTING");
        while (1)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    // ── Splash screen ─────────────────────────────────────
    display.setTextSize(1);
    display.setCursor(20, 10);
    display.println("Health Monitor");
    display.setCursor(32, 26);
    display.println("Initialising");
    display.setCursor(40, 40);
    display.println("sensors...");
    display.display();

    Serial.println("OLED display initialised.");
}

// ─────────────────────────────────────────────────────────
//  Display Task
//  Cycles through 3 screens every SCREEN_DWELL_MS.
//  Also continuously feeds ECG raw values into the sparkline.
// ─────────────────────────────────────────────────────────
void displayTask(void *pvParameters)
{
    int currentScreen    = 0;
    TickType_t lastSwitch = xTaskGetTickCount();

    // Give sensors time to init before showing data
    vTaskDelay(pdMS_TO_TICKS(2000));

    while (1)
    {
        // ── Feed sparkline from latest ECG raw value ──────
        ecgHistory[ecgHistIdx] = (int)g_ecgRaw;
        ecgHistIdx = (ecgHistIdx + 1) % ECG_HISTORY;

        // ── Advance screen every SCREEN_DWELL_MS ──────────
        if ((xTaskGetTickCount() - lastSwitch) >= pdMS_TO_TICKS(SCREEN_DWELL_MS))
        {
            currentScreen = (currentScreen + 1) % NUM_SCREENS;
            lastSwitch = xTaskGetTickCount();
        }

        // ── Draw current screen ───────────────────────────
        switch (currentScreen)
        {
            case 0: drawEcgScreen();  break;
            case 1: drawSpo2Screen(); break;
            case 2: drawTempScreen(); break;
        }

        vTaskDelay(pdMS_TO_TICKS(50));  // ~20 fps refresh
    }
}*/
#include "display.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>

#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT    64
#define OLED_RESET       -1
#define SCREEN_DWELL_MS  3000
#define NUM_SCREENS       3

// ── Globals ───────────────────────────────────────────────
volatile int     g_bpm        = -1;
volatile int     g_ecgRaw     = 0;
volatile bool    g_leadsOff   = true;

volatile int32_t g_spo2       = -1;
volatile int32_t g_hr         = -1;
volatile bool    g_fingerOn   = false;

volatile float   g_palmTemp   = 0.0f;
volatile float   g_coreTemp   = 0.0f;
volatile bool    g_tempReady  = false;
volatile bool    g_measuring  = false;
volatile int     g_countdown  = 30;

static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire1, OLED_RESET);

#define ECG_HISTORY 64
static int ecgHistory[ECG_HISTORY] = {0};
static int ecgHistIdx = 0;

static void drawDivider(int y) {
    display.drawFastHLine(0, y, SCREEN_WIDTH, SSD1306_WHITE);
}

// ─────────────────────────────────────────────────────────
//  Screen 0 — ECG / BPM
// ─────────────────────────────────────────────────────────
static void drawEcgScreen(void)
{
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    // ── Title + lead status ───────────────────────────────
    display.setCursor(0, 0);
    display.print("ECG");
    display.setCursor(40, 0);
    display.print(g_leadsOff ? "[LEADS OFF]" : "[CONNECTED]");

    // ── Sparkline ─────────────────────────────────────────
    const int sparkTop = 10, sparkH = 18;
    for (int i = 1; i < ECG_HISTORY; i++) {
        int idx0 = (ecgHistIdx + i    ) % ECG_HISTORY;
        int idx1 = (ecgHistIdx + i + 1) % ECG_HISTORY;
        int y0 = constrain(sparkTop + sparkH - (ecgHistory[idx0] * sparkH / 4095), sparkTop, sparkTop + sparkH);
        int y1 = constrain(sparkTop + sparkH - (ecgHistory[idx1] * sparkH / 4095), sparkTop, sparkTop + sparkH);
        display.drawLine(i - 1, y0, i, y1, SSD1306_WHITE);
    }

    drawDivider(29);

    // ── Bottom half ───────────────────────────────────────
    if (g_leadsOff)
    {
        display.setTextSize(1);
        display.setCursor(0, 33);
        display.println("Attach leads to");
        display.println("measure ECG/BPM");
    }
    else
    {
        // Leads are connected — show BPM if available, else waiting message
        if (g_bpm > 0)
        {
            // Large BPM number on the right
            display.setTextSize(3);
            int digits  = (g_bpm >= 100) ? 3 : 2;
            int bpmW    = digits * 18;
            display.setCursor(SCREEN_WIDTH - bpmW - 2, 33);
            display.print(g_bpm);

            // Label on the left
            display.setTextSize(1);
            display.setCursor(0, 38);
            display.print("Heart");
            display.setCursor(0, 50);
            display.print("Rate");
            display.setCursor(0, 58);  // unit under label
            display.print("BPM");
        }
        else
        {
            // Leads on but no beat detected yet
            display.setTextSize(1);
            display.setCursor(0, 35);
            display.print("Detecting...");
            display.setCursor(0, 48);
            display.print("Hold still");
        }
    }

    display.display();
}

// ─────────────────────────────────────────────────────────
//  Screen 1 — SpO2 + HR (MAX30102)
// ─────────────────────────────────────────────────────────
static void drawSpo2Screen(void)
{
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    // ── Title ─────────────────────────────────────────────
    display.setCursor(38, 0);
    display.print("BLOOD SpO2");
    drawDivider(10);

    if (!g_fingerOn)
    {
        display.setCursor(4, 22);
        display.println("Place finger on");
        display.setCursor(10, 34);
        display.println("MAX30102...");
        display.display();
        return;
    }

    if (g_spo2 < 0)
    {
        display.setCursor(8, 22);
        display.println("Stabilising...");
        display.setCursor(4, 34);
        display.println("Hold still please");
        display.display();
        return;
    }

    // ── Large SpO2 value, centered ────────────────────────
    display.setTextSize(3);
    // 100 = 3 digits (54px), <100 = 2 digits (36px) — center on 128px
    int valW = (g_spo2 == 100) ? 54 : 36;
    display.setCursor((SCREEN_WIDTH - valW) / 2, 16);
    display.print(g_spo2);

    // % symbol next to value
    display.setTextSize(2);
    display.setCursor((SCREEN_WIDTH + valW) / 2 + 2, 22);
    display.print("%");

    // ── Status bar ────────────────────────────────────────
    drawDivider(46);
    display.setTextSize(1);
    display.setCursor(0, 50);
    display.print("Oxygen Saturation");
    drawDivider(58);
    display.setCursor(0, 60);  // won't render past 63 but safe
    display.setCursor(35, 50); // overwrite with status right-aligned
    display.setTextSize(1);

    // Clear and reprint bottom area properly
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(38, 0);
    display.print("BLOOD SpO2");
    drawDivider(10);

    display.setTextSize(3);
    display.setCursor((SCREEN_WIDTH - valW) / 2, 14);
    display.print(g_spo2);
    display.setTextSize(2);
    display.setCursor((SCREEN_WIDTH + valW) / 2 + 2, 20);
    display.print("%");

    drawDivider(44);

    display.setTextSize(1);
    display.setCursor(0, 48);
    display.print("Oxygen Saturation");

    drawDivider(57);
    display.setCursor(0, 59);
    if      (g_spo2 >= 95) display.print("Status: Normal");
    else if (g_spo2 >= 90) display.print("Status: Low-normal");
    else                   display.print("Status: ! LOW");

    display.display();
}

// ─────────────────────────────────────────────────────────
//  Screen 2 — Body Temperature (DS18B20)
// ─────────────────────────────────────────────────────────
static void drawTempScreen(void)
{
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(35, 0);
    display.print("BODY TEMP");
    drawDivider(10);

    if (!g_measuring && !g_tempReady)
    {
        // Idle — no result yet
        display.setCursor(4, 16);
        display.println("Touch sensor to");
        display.setCursor(4, 28);
        display.println("start measurement");
        display.display();
        return;
    }

    if (g_measuring)
    {
        // Show last result (if any) in small text at top, countdown below
        if (g_tempReady)
        {
            // Previous result — small, top area
            display.setTextSize(1);
            display.setCursor(0, 13);
            display.print("Last: ");
            display.print(g_coreTemp, 1);
            display.print((char)247);
            display.print("C core");
        }
        else
        {
            display.setTextSize(1);
            display.setCursor(0, 13);
            display.print("Hold sensor on palm");
        }

        // Countdown number — big, center
        display.setTextSize(3);
        display.setCursor((g_countdown >= 10) ? 46 : 55, 26);
        display.print(g_countdown);
        display.setTextSize(1);
        display.setCursor(50, 52);
        display.print("sec");

        // Progress bar
        int barW = constrain((30 - g_countdown) * (SCREEN_WIDTH - 4) / 30, 0, SCREEN_WIDTH - 4);
        display.drawRect(2, 55, SCREEN_WIDTH - 4, 8, SSD1306_WHITE);
        display.fillRect(2, 55, barW, 8, SSD1306_WHITE);

        display.display();
        return;
    }

    // ── Result screen ─────────────────────────────────────
    if (g_tempReady)
    {
        display.setCursor(0, 13);
        display.print("Palm:");
        display.setTextSize(2);
        display.setCursor(38, 11);
        display.print(g_palmTemp, 1);
        display.print((char)247);
        display.print("C");

        drawDivider(32);

        display.setTextSize(1);
        display.setCursor(0, 36);
        display.print("Core:");
        display.setTextSize(2);
        display.setCursor(38, 34);
        display.print(g_coreTemp, 1);
        display.print((char)247);
        display.print("C");

        drawDivider(54);
        display.setTextSize(1);
        display.setCursor(0, 56);
        if      (g_coreTemp >= 37.5f) display.print("! FEVER");
        else if (g_coreTemp >= 36.0f) display.print("Normal");
        else                          display.print("Below normal");

        display.display();
    }
}

// ─────────────────────────────────────────────────────────
void initDisplay(void)
{
    Wire1.begin(OLED_SDA, OLED_SCL);

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    {
        Serial.println("ERROR: SSD1306 not found on Wire1 (SDA=33, SCL=32)");
        return;
    }

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(20, 10);
    display.println("Health Monitor");
    display.setCursor(32, 26);
    display.println("Initialising");
    display.setCursor(40, 40);
    display.println("sensors...");
    display.display();

    Serial.println("OLED initialised on Wire1 (SDA=33, SCL=32).");
}

void displayTask(void *pvParameters)
{
    int currentScreen     = 0;
    TickType_t lastSwitch = xTaskGetTickCount();

    vTaskDelay(pdMS_TO_TICKS(2000));

    while (1)
    {
        // Feed ECG sparkline
        ecgHistory[ecgHistIdx] = (int)g_ecgRaw;
        ecgHistIdx = (ecgHistIdx + 1) % ECG_HISTORY;

        // Auto-cycle screens
        if ((xTaskGetTickCount() - lastSwitch) >= pdMS_TO_TICKS(SCREEN_DWELL_MS)) {
            currentScreen = (currentScreen + 1) % NUM_SCREENS;
            lastSwitch = xTaskGetTickCount();
        }

        switch (currentScreen) {
            case 0: drawEcgScreen();  break;
            case 1: drawSpo2Screen(); break;
            case 2: drawTempScreen(); break;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/*#include "display.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>

#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT    64
#define OLED_RESET       -1
#define SCREEN_DWELL_MS  3000
#define NUM_SCREENS       3

// ── Globals ───────────────────────────────────────────────
volatile int     g_bpm        = -1;
volatile int     g_ecgRaw     = 0;
volatile bool    g_leadsOff   = true;

volatile int32_t g_spo2       = -1;
volatile int32_t g_hr         = -1;
volatile bool    g_fingerOn   = false;

volatile float   g_palmTemp   = 0.0f;
volatile float   g_coreTemp   = 0.0f;
volatile bool    g_tempReady  = false;
volatile bool    g_measuring  = false;
volatile int     g_countdown  = 30;

static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire1, OLED_RESET);

#define ECG_HISTORY 64
static int ecgHistory[ECG_HISTORY] = {0};
static int ecgHistIdx = 0;

static void drawDivider(int y) {
    display.drawFastHLine(0, y, SCREEN_WIDTH, SSD1306_WHITE);
}

// ─────────────────────────────────────────────────────────
//  Screen 0 — ECG / BPM
// ─────────────────────────────────────────────────────────
static void drawEcgScreen(void)
{
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    // ── Title + lead status ───────────────────────────────
    display.setCursor(0, 0);
    display.print("ECG");
    display.setCursor(40, 0);
    display.print(g_leadsOff ? "[LEADS OFF]" : "[CONNECTED]");

    // ── Sparkline ─────────────────────────────────────────
    const int sparkTop = 10, sparkH = 18;
    for (int i = 1; i < ECG_HISTORY; i++) {
        int idx0 = (ecgHistIdx + i    ) % ECG_HISTORY;
        int idx1 = (ecgHistIdx + i + 1) % ECG_HISTORY;
        int y0 = constrain(sparkTop + sparkH - (ecgHistory[idx0] * sparkH / 4095), sparkTop, sparkTop + sparkH);
        int y1 = constrain(sparkTop + sparkH - (ecgHistory[idx1] * sparkH / 4095), sparkTop, sparkTop + sparkH);
        display.drawLine(i - 1, y0, i, y1, SSD1306_WHITE);
    }

    drawDivider(29);

    // ── Bottom half ───────────────────────────────────────
    if (g_leadsOff)
    {
        display.setTextSize(1);
        display.setCursor(0, 33);
        display.println("Attach leads to");
        display.println("measure ECG/BPM");
    }
    else
    {
        // Leads are connected — show BPM if available, else waiting message
        if (g_bpm > 0)
        {
            // Large BPM number on the right
            display.setTextSize(3);
            int digits  = (g_bpm >= 100) ? 3 : 2;
            int bpmW    = digits * 18;
            display.setCursor(SCREEN_WIDTH - bpmW - 2, 33);
            display.print(g_bpm);

            // Label on the left
            display.setTextSize(1);
            display.setCursor(0, 38);
            display.print("Heart");
            display.setCursor(0, 50);
            display.print("Rate");
            display.setCursor(0, 58);  // unit under label
            display.print("BPM");
        }
        else
        {
            // Leads on but no beat detected yet
            display.setTextSize(1);
            display.setCursor(0, 35);
            display.print("Detecting...");
            display.setCursor(0, 48);
            display.print("Hold still");
        }
    }

    display.display();
}

// ─────────────────────────────────────────────────────────
//  Screen 1 — SpO2 + HR (MAX30102)
// ─────────────────────────────────────────────────────────
static void drawSpo2Screen(void)
{
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    // ── Title ─────────────────────────────────────────────
    display.setCursor(38, 0);
    display.print("BLOOD SpO2");
    drawDivider(10);

    if (!g_fingerOn)
    {
        display.setCursor(4, 22);
        display.println("Place finger on");
        display.setCursor(10, 34);
        display.println("MAX30102...");
        display.display();
        return;
    }

    if (g_spo2 < 0)
    {
        display.setCursor(8, 22);
        display.println("Stabilising...");
        display.setCursor(4, 34);
        display.println("Hold still please");
        display.display();
        return;
    }

    // ── Large SpO2 value, centered ────────────────────────
    display.setTextSize(3);
    // 100 = 3 digits (54px), <100 = 2 digits (36px) — center on 128px
    int valW = (g_spo2 == 100) ? 54 : 36;
    display.setCursor((SCREEN_WIDTH - valW) / 2, 16);
    display.print(g_spo2);

    // % symbol next to value
    display.setTextSize(2);
    display.setCursor((SCREEN_WIDTH + valW) / 2 + 2, 22);
    display.print("%");

    // ── Status bar ────────────────────────────────────────
    drawDivider(46);
    display.setTextSize(1);
    display.setCursor(0, 50);
    display.print("Oxygen Saturation");
    drawDivider(58);
    display.setCursor(0, 60);  // won't render past 63 but safe
    display.setCursor(35, 50); // overwrite with status right-aligned
    display.setTextSize(1);

    // Clear and reprint bottom area properly
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(38, 0);
    display.print("BLOOD SpO2");
    drawDivider(10);

    display.setTextSize(3);
    display.setCursor((SCREEN_WIDTH - valW) / 2, 14);
    display.print(g_spo2);
    display.setTextSize(2);
    display.setCursor((SCREEN_WIDTH + valW) / 2 + 2, 20);
    display.print("%");

    drawDivider(44);

    display.setTextSize(1);
    display.setCursor(0, 48);
    display.print("Oxygen Saturation");

    drawDivider(57);
    display.setCursor(0, 59);
    if      (g_spo2 >= 95) display.print("Status: Normal");
    else if (g_spo2 >= 90) display.print("Status: Low-normal");
    else                   display.print("Status: ! LOW");

    display.display();
}

// ─────────────────────────────────────────────────────────
//  Screen 2 — Body Temperature (DS18B20)
// ─────────────────────────────────────────────────────────
static void drawTempScreen(void)
{
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(35, 0);
    display.print("BODY TEMP");
    drawDivider(10);

    if (!g_measuring && !g_tempReady)
    {
        // Idle — no result yet
        display.setCursor(4, 16);
        display.println("Touch sensor to");
        display.setCursor(4, 28);
        display.println("start measurement");
        display.display();
        return;
    }

    if (g_measuring)
    {
        // Show last result (if any) in small text at top, countdown below
        if (g_tempReady)
        {
            display.setTextSize(1);
            display.setCursor(0, 13);
            display.print("Last: ");
            display.print(g_coreTemp, 1);
            display.print((char)247);
            display.print("C body");
        }
        else
        {
            display.setTextSize(1);
            display.setCursor(0, 13);
            display.print("Hold sensor on palm");
        }

        // Countdown number — big, center
        display.setTextSize(3);
        display.setCursor((g_countdown >= 10) ? 46 : 55, 26);
        display.print(g_countdown);
        display.setTextSize(1);
        display.setCursor(50, 52);
        display.print("sec");

        // Progress bar
        int barW = constrain((30 - g_countdown) * (SCREEN_WIDTH - 4) / 30, 0, SCREEN_WIDTH - 4);
        display.drawRect(2, 55, SCREEN_WIDTH - 4, 8, SSD1306_WHITE);
        display.fillRect(2, 55, barW, 8, SSD1306_WHITE);

        display.display();
        return;
    }

    // ── Result screen ─────────────────────────────────────
    if (g_tempReady)
    {
        // ── Large core temp, centered ─────────────────────
        display.setTextSize(3);
        display.setCursor(10, 14);
        display.print(g_coreTemp, 1);
        display.print((char)247);
        display.print("C");

        drawDivider(44);

        display.setTextSize(1);
        display.setCursor(0, 48);
        display.print("Body Temperature");

        drawDivider(57);
        display.setCursor(0, 59);
        if      (g_coreTemp >= 37.5f) display.print("Status: ! FEVER");
        else if (g_coreTemp >= 36.0f) display.print("Status: Normal");
        else                          display.print("Status: Below normal");

        display.display();
    }
}

// ─────────────────────────────────────────────────────────
void initDisplay(void)
{
    Wire1.begin(OLED_SDA, OLED_SCL);

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    {
        Serial.println("ERROR: SSD1306 not found on Wire1 (SDA=33, SCL=32)");
        return;
    }

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(20, 10);
    display.println("Health Monitor");
    display.setCursor(32, 26);
    display.println("Initialising");
    display.setCursor(40, 40);
    display.println("sensors...");
    display.display();

    Serial.println("OLED initialised on Wire1 (SDA=33, SCL=32).");
}

void displayTask(void *pvParameters)
{
    int currentScreen     = 0;
    TickType_t lastSwitch = xTaskGetTickCount();

    vTaskDelay(pdMS_TO_TICKS(2000));

    while (1)
    {
        // Feed ECG sparkline
        ecgHistory[ecgHistIdx] = (int)g_ecgRaw;
        ecgHistIdx = (ecgHistIdx + 1) % ECG_HISTORY;

        // Auto-cycle screens
        if ((xTaskGetTickCount() - lastSwitch) >= pdMS_TO_TICKS(SCREEN_DWELL_MS)) {
            currentScreen = (currentScreen + 1) % NUM_SCREENS;
            lastSwitch = xTaskGetTickCount();
        }

        switch (currentScreen) {
            case 0: drawEcgScreen();  break;
            case 1: drawSpo2Screen(); break;
            case 2: drawTempScreen(); break;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}*/

/*#include "display.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>

#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT    64
#define OLED_RESET       -1
#define SCREEN_DWELL_MS  3000
#define NUM_SCREENS       3

// ── Globals ───────────────────────────────────────────────
volatile int     g_bpm        = -1;
volatile int     g_ecgRaw     = 0;
volatile bool    g_leadsOff   = true;

volatile int32_t g_spo2       = -1;
volatile int32_t g_hr         = -1;
volatile bool    g_fingerOn   = false;

volatile float   g_palmTemp   = 0.0f;
volatile float   g_coreTemp   = 0.0f;
volatile bool    g_tempReady  = false;
volatile bool    g_measuring  = false;
volatile int     g_countdown  = 30;
volatile bool    g_touched    = false;

volatile int     g_beatCount  = 0;  // increments each detected beat

static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire1, OLED_RESET);

#define ECG_HISTORY 64
static int ecgHistory[ECG_HISTORY] = {0};
static int ecgHistIdx = 0;

static void drawDivider(int y) {
    display.drawFastHLine(0, y, SCREEN_WIDTH, SSD1306_WHITE);
}

// ─────────────────────────────────────────────────────────
//  Screen 0 — ECG / BPM
// ─────────────────────────────────────────────────────────
static void drawEcgScreen(void)
{
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    // ── Title + lead status ───────────────────────────────
    display.setCursor(0, 0);
    display.print("ECG");
    display.setCursor(40, 0);
    display.print(g_leadsOff ? "[LEADS OFF]" : "[CONNECTED]");

    // ── Sparkline ─────────────────────────────────────────
    const int sparkTop = 10, sparkH = 18;
    for (int i = 1; i < ECG_HISTORY; i++) {
        int idx0 = (ecgHistIdx + i    ) % ECG_HISTORY;
        int idx1 = (ecgHistIdx + i + 1) % ECG_HISTORY;
        int y0 = constrain(sparkTop + sparkH - (ecgHistory[idx0] * sparkH / 4095), sparkTop, sparkTop + sparkH);
        int y1 = constrain(sparkTop + sparkH - (ecgHistory[idx1] * sparkH / 4095), sparkTop, sparkTop + sparkH);
        display.drawLine(i - 1, y0, i, y1, SSD1306_WHITE);
    }

    drawDivider(29);

    // ── Bottom half ───────────────────────────────────────
    if (g_leadsOff)
    {
        display.setTextSize(1);
        display.setCursor(0, 33);
        display.println("Attach leads to");
        display.println("measure ECG/BPM");
    }
    else
    {
        // Leads on — show BPM or detecting state
        if (g_bpm > 0)
        {
            display.setTextSize(3);
            int digits = (g_bpm >= 100) ? 3 : 2;
            int bpmW   = digits * 18;
            display.setCursor(SCREEN_WIDTH - bpmW - 2, 33);
            display.print(g_bpm);
            display.setTextSize(1);
            display.setCursor(0, 38);
            display.print("Heart");
            display.setCursor(0, 50);
            display.print("Rate");
            display.setCursor(0, 58);
            display.print("BPM");
        }
        else
        {
            display.setTextSize(1);
            display.setCursor(0, 33);
            display.print("Detecting...");
            display.setCursor(0, 45);
            display.print("Beats found: ");
            display.print(g_beatCount);
            display.setCursor(0, 57);
            display.print("Hold still");
        }
    }

    display.display();
}

// ─────────────────────────────────────────────────────────
//  Screen 1 — SpO2 + HR (MAX30102)
// ─────────────────────────────────────────────────────────
static void drawSpo2Screen(void)
{
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    // ── Title ─────────────────────────────────────────────
    display.setCursor(38, 0);
    display.print("BLOOD SpO2");
    drawDivider(10);

    if (!g_fingerOn)
    {
        display.setCursor(4, 22);
        display.println("Place finger on");
        display.setCursor(10, 34);
        display.println("MAX30102...");
        display.display();
        return;
    }

    if (g_spo2 < 0)
    {
        display.setCursor(8, 22);
        display.println("Stabilising...");
        display.setCursor(4, 34);
        display.println("Hold still please");
        display.display();
        return;
    }

    // ── Large SpO2 value, centered ────────────────────────
    display.setTextSize(3);
    // 100 = 3 digits (54px), <100 = 2 digits (36px) — center on 128px
    int valW = (g_spo2 == 100) ? 54 : 36;
    display.setCursor((SCREEN_WIDTH - valW) / 2, 16);
    display.print(g_spo2);

    // % symbol next to value
    display.setTextSize(2);
    display.setCursor((SCREEN_WIDTH + valW) / 2 + 2, 22);
    display.print("%");

    // ── Status bar ────────────────────────────────────────
    drawDivider(46);
    display.setTextSize(1);
    display.setCursor(0, 50);
    display.print("Oxygen Saturation");
    drawDivider(58);
    display.setCursor(0, 60);  // won't render past 63 but safe
    display.setCursor(35, 50); // overwrite with status right-aligned
    display.setTextSize(1);

    // Clear and reprint bottom area properly
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(38, 0);
    display.print("BLOOD SpO2");
    drawDivider(10);

    display.setTextSize(3);
    display.setCursor((SCREEN_WIDTH - valW) / 2, 14);
    display.print(g_spo2);
    display.setTextSize(2);
    display.setCursor((SCREEN_WIDTH + valW) / 2 + 2, 20);
    display.print("%");

    drawDivider(44);

    display.setTextSize(1);
    display.setCursor(0, 48);
    display.print("Oxygen Saturation");

    drawDivider(57);
    display.setCursor(0, 59);
    if      (g_spo2 >= 95) display.print("Status: Normal");
    else if (g_spo2 >= 90) display.print("Status: Low-normal");
    else                   display.print("Status: ! LOW");

    display.display();
}

// ─────────────────────────────────────────────────────────
//  Screen 2 — Body Temperature (DS18B20)
// ─────────────────────────────────────────────────────────
static void drawTempScreen(void)
{
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(35, 0);
    display.print("BODY TEMP");
    drawDivider(10);

    if (!g_touched && !g_measuring)
    {
        // Finger not on sensor — show idle prompt
        // Still show last result below if available
        display.setCursor(4, 16);
        display.println("Touch sensor to");
        display.setCursor(4, 28);
        display.println("start measurement");

        if (g_tempReady)
        {
            drawDivider(40);
            display.setTextSize(1);
            display.setCursor(0, 44);
            display.print("Last: ");
            display.print(g_coreTemp, 1);
            display.print((char)247);
            display.print("C");
            display.setCursor(0, 56);
            if      (g_coreTemp >= 37.5f) display.print("! FEVER");
            else if (g_coreTemp >= 36.0f) display.print("Normal");
            else                          display.print("Below normal");
        }

        display.display();
        return;
    }

    if (g_measuring)
    {
        // Show last result (if any) in small text at top, countdown below
        if (g_tempReady)
        {
            display.setTextSize(1);
            display.setCursor(0, 13);
            display.print("Last: ");
            display.print(g_coreTemp, 1);
            display.print((char)247);
            display.print("C body");
        }
        else
        {
            display.setTextSize(1);
            display.setCursor(0, 13);
            display.print("Hold sensor on palm");
        }

        // Countdown number — big, center
        display.setTextSize(3);
        display.setCursor((g_countdown >= 10) ? 46 : 55, 26);
        display.print(g_countdown);
        display.setTextSize(1);
        display.setCursor(50, 52);
        display.print("sec");

        // Progress bar
        int barW = constrain((30 - g_countdown) * (SCREEN_WIDTH - 4) / 30, 0, SCREEN_WIDTH - 4);
        display.drawRect(2, 55, SCREEN_WIDTH - 4, 8, SSD1306_WHITE);
        display.fillRect(2, 55, barW, 8, SSD1306_WHITE);

        display.display();
        return;
    }

    // ── Result screen ─────────────────────────────────────
    if (g_tempReady)
    {
        // ── Large core temp, centered ─────────────────────
        display.setTextSize(3);
        display.setCursor(10, 14);
        display.print(g_coreTemp, 1);
        display.print((char)247);
        display.print("C");

        drawDivider(44);

        display.setTextSize(1);
        display.setCursor(0, 48);
        display.print("Body Temperature");

        drawDivider(57);
        display.setCursor(0, 59);
        if      (g_coreTemp >= 37.5f) display.print("Status: ! FEVER");
        else if (g_coreTemp >= 36.0f) display.print("Status: Normal");
        else                          display.print("Status: Below normal");

        display.display();
    }
}

// ─────────────────────────────────────────────────────────
void initDisplay(void)
{
    Wire1.begin(OLED_SDA, OLED_SCL);

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    {
        Serial.println("ERROR: SSD1306 not found on Wire1 (SDA=33, SCL=32)");
        return;
    }

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(20, 10);
    display.println("Health Monitor");
    display.setCursor(32, 26);
    display.println("Initialising");
    display.setCursor(40, 40);
    display.println("sensors...");
    display.display();

    Serial.println("OLED initialised on Wire1 (SDA=33, SCL=32).");
}

void displayTask(void *pvParameters)
{
    int currentScreen     = 0;
    TickType_t lastSwitch = xTaskGetTickCount();

    vTaskDelay(pdMS_TO_TICKS(2000));

    while (1)
    {
        // Feed ECG sparkline
        ecgHistory[ecgHistIdx] = (int)g_ecgRaw;
        ecgHistIdx = (ecgHistIdx + 1) % ECG_HISTORY;

        // Auto-cycle screens
        if ((xTaskGetTickCount() - lastSwitch) >= pdMS_TO_TICKS(SCREEN_DWELL_MS)) {
            currentScreen = (currentScreen + 1) % NUM_SCREENS;
            lastSwitch = xTaskGetTickCount();
        }

        switch (currentScreen) {
            case 0: drawEcgScreen();  break;
            case 1: drawSpo2Screen(); break;
            case 2: drawTempScreen(); break;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}*/