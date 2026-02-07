/**
 * CalibrationScreen.h - Touch calibration screen for BrewForge HMI
 *
 * 4-corner touch calibration procedure. Draws crosshairs at each corner,
 * captures raw touch coordinates, and computes mapping ranges.
 *
 * This screen runs a blocking calibration sequence when entered,
 * then triggers a callback with the results and navigates back.
 *
 * Note: This is somewhat special — it uses blocking waits during
 * calibration (touch.touched() loops). That's fine for a calibration
 * screen that takes full control.
 */

#pragma once

#include <ForgeUI.h>
#include <XPT2046_Touchscreen.h>
#include <functional>

struct TouchCal {
    int16_t xMin = 300;
    int16_t xMax = 3800;
    int16_t yMin = 300;
    int16_t yMax = 3800;
    bool valid = false;
};

class CalibrationScreen : public Screen {
private:
    XPT2046_Touchscreen &touch;
    TouchCal &cal;
    std::function<void()> onComplete;  // Called when calibration finishes

public:
    CalibrationScreen(GfxDriver &gfx, const ForgeTheme &theme,
                      XPT2046_Touchscreen &ts, TouchCal &calData)
        : Screen(gfx, theme, "Calibration"), touch(ts), cal(calData) {}

    void setOnComplete(std::function<void()> cb) {
        onComplete = cb;
    }

    void setup() override {
        // No persistent widgets — this screen draws procedurally
    }

    void onEnter() override {
        Screen::onEnter();
        runCalibration();
    }

    void update() override {}

    void draw() override {
        // Drawing handled in runCalibration()
    }

private:
    void drawCrosshair(int16_t sx, int16_t sy) {
        gfx.drawCircle(sx, sy, 10, theme.accentRed);
        gfx.drawCircle(sx, sy, 3, theme.accentRed);
        // Horizontal line
        gfx.drawLine(sx - 15, sy, sx + 15, sy, theme.accentRed);
        // Vertical line
        gfx.drawLine(sx, sy - 15, sx, sy + 15, theme.accentRed);
    }

    void runCalibration() {
        struct CalPoint { int16_t sx, sy, tx, ty; };
        CalPoint pts[4] = {
            {20,  20,  0, 0},    // Top-left
            {220, 20,  0, 0},    // Top-right
            {220, 300, 0, 0},    // Bottom-right
            {20,  300, 0, 0}     // Bottom-left
        };

        for (int i = 0; i < 4; i++) {
            gfx.fillScreen(theme.bgPrimary);
            gfx.setTextSize(2);
            gfx.setTextColor(theme.textPrimary, theme.bgPrimary);
            gfx.setTextDatum(GfxDriver::DATUM_TL);

            char msg[24];
            snprintf(msg, sizeof(msg), "Touch point %d/4", i + 1);
            gfx.drawString(msg, 30, 140);

            drawCrosshair(pts[i].sx, pts[i].sy);

            // Wait for touch
            while (!touch.touched()) delay(10);
            delay(50);  // Settle
            TS_Point p = touch.getPoint();
            pts[i].tx = p.x;
            pts[i].ty = p.y;

            Serial.printf("Cal[%d] screen(%d,%d) raw(%d,%d) z=%d\n",
                          i, pts[i].sx, pts[i].sy, p.x, p.y, p.z);

            // Wait for release
            while (touch.touched()) delay(10);
            delay(300);
        }

        // Compute calibration
        cal.xMin = (pts[0].tx + pts[3].tx) / 2;
        cal.xMax = (pts[1].tx + pts[2].tx) / 2;
        cal.yMin = (pts[0].ty + pts[1].ty) / 2;
        cal.yMax = (pts[2].ty + pts[3].ty) / 2;
        cal.valid = true;

        Serial.printf("Calibration: X(%d->%d) Y(%d->%d)\n",
                      cal.xMin, cal.xMax, cal.yMin, cal.yMax);

        // Show result
        gfx.fillScreen(theme.bgPrimary);
        gfx.setTextSize(2);
        gfx.setTextColor(theme.accentGreen, theme.bgPrimary);
        gfx.setTextDatum(GfxDriver::DATUM_TL);
        gfx.drawString("Calibration done!", 20, 100);

        gfx.setTextSize(1);
        gfx.setTextColor(theme.textDim, theme.bgPrimary);

        char info[32];
        snprintf(info, sizeof(info), "X: %d -> %d", cal.xMin, cal.xMax);
        gfx.drawString(info, 20, 130);
        snprintf(info, sizeof(info), "Y: %d -> %d", cal.yMin, cal.yMax);
        gfx.drawString(info, 20, 145);
        gfx.drawString("Touch to continue...", 20, 170);

        // Wait for touch then release
        while (!touch.touched()) delay(10);
        while (touch.touched()) delay(10);

        // Notify completion (main will switch back to brew screen)
        if (onComplete) onComplete();
    }
};
