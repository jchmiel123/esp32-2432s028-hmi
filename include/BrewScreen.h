/**
 * BrewScreen.h - Main brew display screen for BrewForge HMI
 *
 * Shows temperature, brew state, buttons, flow info, relay status.
 * Uses ForgeUI widgets for clean rendering with dirty-flag updates.
 *
 * Layout (240x320 portrait):
 *   Y=0   Title bar (25px) - "BrewForge" + connection dot
 *   Y=25  Temperature (70px) - Large temp + target + bar
 *   Y=95  State (30px) - [step]STATE + timer + relay dots
 *   Y=125 Buttons (55px) - BREW / STOP
 *   Y=180 Flow (45px) - Flow rate + volume
 *   Y=225 Adjust (45px) - -5 / +5 / CAL
 *   Y=270 Nav (50px) - Reserved
 */

#pragma once

#include <ForgeUI.h>
#include <functional>

// Forward-declare the BrewStatus struct (defined in main.cpp)
struct BrewStatus;

class BrewScreen : public Screen {
private:
    // References to shared state
    BrewStatus &brew;

    // Command callbacks
    std::function<void()> onBrew;
    std::function<void()> onStop;
    std::function<void()> onTempDown;
    std::function<void()> onTempUp;
    std::function<void()> onCalibrate;

    // --- Widget pointers (owned by Screen::elements via addElement) ---
    // Title
    Label*      lblTitle    = nullptr;
    StatusDot*  dotConn     = nullptr;

    // Temperature
    Label*      lblTemp     = nullptr;
    Label*      lblTarget   = nullptr;
    Label*      lblRate     = nullptr;
    ProgressBar* barTemp    = nullptr;

    // State
    Label*      lblState    = nullptr;
    Label*      lblTimer    = nullptr;
    StatusDot*  dotPump     = nullptr;
    StatusDot*  dotBoiler   = nullptr;
    StatusDot*  dotSolenoid = nullptr;
    StatusDot*  dotWarmer   = nullptr;

    // Buttons
    Button*     btnBrew     = nullptr;
    Button*     btnStop     = nullptr;
    Button*     btnTempDown = nullptr;
    Button*     btnTempUp   = nullptr;
    Button*     btnCal      = nullptr;

    // Flow
    Label*      lblFlow     = nullptr;
    Label*      lblVolume   = nullptr;

    // Layout constants
    static constexpr int16_t TITLE_Y   = 0;
    static constexpr int16_t TEMP_Y    = 25;
    static constexpr int16_t STATE_Y   = 95;
    static constexpr int16_t BUTTONS_Y = 125;
    static constexpr int16_t FLOW_Y    = 180;
    static constexpr int16_t TEMPADJ_Y = 225;

public:
    BrewScreen(GfxDriver &gfx, const ForgeTheme &theme, BrewStatus &status)
        : Screen(gfx, theme, "BrewForge"), brew(status) {}

    // Set command callbacks
    void setCallbacks(
        std::function<void()> brew_cb,
        std::function<void()> stop_cb,
        std::function<void()> tempDown_cb,
        std::function<void()> tempUp_cb,
        std::function<void()> cal_cb
    ) {
        onBrew      = brew_cb;
        onStop      = stop_cb;
        onTempDown  = tempDown_cb;
        onTempUp    = tempUp_cb;
        onCalibrate = cal_cb;
    }

    void setup() override {
        int16_t W = theme.screenW;

        // ========== TITLE BAR ==========
        lblTitle = new Label(5, 5, "BrewForge",
                             theme.accentCyan, theme.bgHeader, 2);
        addElement(lblTitle);

        dotConn = new StatusDot(W - 15, 12, 5,
                                theme.accentGreen, theme.accentRed);
        addElement(dotConn);

        // ========== TEMPERATURE ==========
        lblTemp = new Label(W / 2, TEMP_Y + 5, "0.0C",
                            theme.accentPrimary, theme.bgPrimary,
                            4, GfxDriver::DATUM_TC, W);
        addElement(lblTemp);

        lblTarget = new Label(W / 2, TEMP_Y + 40, "Target: 93C",
                              theme.accentCyan, theme.bgPrimary,
                              1, GfxDriver::DATUM_TC, W);
        addElement(lblTarget);

        lblRate = new Label(W / 2, TEMP_Y + 52, "",
                            theme.textDim, theme.bgPrimary,
                            1, GfxDriver::DATUM_TC, W);
        addElement(lblRate);

        barTemp = new ProgressBar(20, TEMP_Y + 62, W - 40, 6,
                                  theme.accentGreen, theme.bgPrimary,
                                  theme.textDim, true);
        addElement(barTemp);

        // ========== STATE ==========
        lblState = new Label(5, STATE_Y + 2, "[0]IDLE",
                             theme.accentGreen, theme.bgPrimary,
                             2, GfxDriver::DATUM_TL, 150);
        addElement(lblState);

        lblTimer = new Label(W - 5, STATE_Y + 2, "",
                             theme.textPrimary, theme.bgPrimary,
                             2, GfxDriver::DATUM_TR, 100);
        addElement(lblTimer);

        // Relay dots (P B S W)
        int16_t dotY = STATE_Y + 22;
        int16_t dotX = W - 70;
        int16_t dotSpace = 14;

        dotPump     = new StatusDot(dotX,              dotY, 4, theme.accentGreen,  theme.btnDefault, 'P');
        dotBoiler   = new StatusDot(dotX + dotSpace,   dotY, 4, theme.accentRed,    theme.btnDefault, 'B');
        dotSolenoid = new StatusDot(dotX + dotSpace*2, dotY, 4, theme.accentBlue,   theme.btnDefault, 'S');
        dotWarmer   = new StatusDot(dotX + dotSpace*3, dotY, 4, theme.accentYellow, theme.btnDefault, 'W');
        addElement(dotPump);
        addElement(dotBoiler);
        addElement(dotSolenoid);
        addElement(dotWarmer);

        // ========== BUTTONS ==========
        btnBrew = new Button(5, BUTTONS_Y, 112, 50, "BREW",
                             theme.accentGreen, theme.bgPrimary, 3);
        btnBrew->onClick = [this]() { if (onBrew) onBrew(); };
        addElement(btnBrew);

        btnStop = new Button(123, BUTTONS_Y, 112, 50, "STOP",
                             theme.accentRed, theme.textPrimary, 3);
        btnStop->onClick = [this]() { if (onStop) onStop(); };
        addElement(btnStop);

        // ========== TEMP ADJUST ==========
        btnTempDown = new Button(5, TEMPADJ_Y, 55, 40, "-5",
                                 theme.btnDefault, theme.textPrimary, 2);
        btnTempDown->onClick = [this]() { if (onTempDown) onTempDown(); };
        addElement(btnTempDown);

        btnTempUp = new Button(65, TEMPADJ_Y, 55, 40, "+5",
                               theme.btnDefault, theme.textPrimary, 2);
        btnTempUp->onClick = [this]() { if (onTempUp) onTempUp(); };
        addElement(btnTempUp);

        btnCal = new Button(130, TEMPADJ_Y, 105, 40, "CAL",
                            theme.btnDefault, theme.accentCyan, 2);
        btnCal->onClick = [this]() { if (onCalibrate) onCalibrate(); };
        addElement(btnCal);

        // ========== FLOW ==========
        lblFlow = new Label(5, FLOW_Y + 2, "Flow 0.0 mL/s",
                            theme.accentCyan, theme.bgPrimary,
                            2, GfxDriver::DATUM_TL, W);
        addElement(lblFlow);

        lblVolume = new Label(5, FLOW_Y + 22, "Vol  0.0 mL",
                              theme.accentCyan, theme.bgPrimary,
                              2, GfxDriver::DATUM_TL, W);
        addElement(lblVolume);
    }

    void onEnter() override {
        Screen::onEnter();
    }

    void update() override {
        char buf[32];

        // --- Title bar ---
        dotConn->setActive(brew.connected);

        // --- Temperature ---
        snprintf(buf, sizeof(buf), "%.1fC", brew.temp);
        lblTemp->setText(buf);

        snprintf(buf, sizeof(buf), "Target: %.0fC", brew.target);
        lblTarget->setText(buf);

        // Rate of change
        if (brew.step >= 1 && brew.step <= 7 && brew.tempRate != 0) {
            snprintf(buf, sizeof(buf), "%+.1f/s", brew.tempRate);
            lblRate->setText(buf);
            lblRate->setVisible(true);
        } else {
            lblRate->setText("");
            lblRate->setVisible(brew.step >= 1 && brew.step <= 7);
        }

        // Temperature bar
        float ratio = (brew.target > 0) ? (brew.temp / brew.target) : 0;
        if (ratio > 1.0f) ratio = 1.0f;
        if (ratio < 0) ratio = 0;
        barTemp->setProgress(ratio);

        // Bar color based on proximity to target
        if (brew.temp < brew.target - 5)
            barTemp->fillColor = theme.accentRed;
        else if (brew.temp < brew.target - 2)
            barTemp->fillColor = theme.accentYellow;
        else
            barTemp->fillColor = theme.accentGreen;

        // --- State ---
        snprintf(buf, sizeof(buf), "[%d]%s", brew.step, brew.state);
        lblState->setText(buf);

        // State color
        if (strcmp(brew.state, "IDLE") == 0)         lblState->textColor = theme.accentGreen;
        else if (strcmp(brew.state, "BREW") == 0)     lblState->textColor = theme.accentPrimary;
        else if (strcmp(brew.state, "PREHEAT") == 0)  lblState->textColor = theme.accentYellow;
        else if (strcmp(brew.state, "DONE") == 0)     lblState->textColor = theme.accentGreen;
        else                                          lblState->textColor = theme.accentCyan;

        // Timer
        if (brew.stepTime > 0) {
            snprintf(buf, sizeof(buf), "%d/%ds", brew.stepElapsed, brew.stepTime);
            lblTimer->setText(buf);
        } else {
            lblTimer->setText("");
        }

        // Relay dots
        dotPump->setActive(brew.pump);
        dotBoiler->setActive(brew.boiler);
        dotSolenoid->setActive(brew.solenoid);
        dotWarmer->setActive(brew.warmer);

        // --- Flow ---
        snprintf(buf, sizeof(buf), "Flow %.1f mL/s", brew.flow);
        lblFlow->setText(buf);
        snprintf(buf, sizeof(buf), "Vol  %.1f mL", brew.volume);
        lblVolume->setText(buf);

        // --- Button press states ---
        btnBrew->updatePressState();
        btnStop->updatePressState();
        btnTempDown->updatePressState();
        btnTempUp->updatePressState();
        btnCal->updatePressState();

        setNeedsRedraw();
    }

    void draw() override {
        if (!needsRedraw) return;

        if (firstDraw) {
            gfx.fillScreen(theme.bgPrimary);

            // Draw title bar background
            gfx.fillRect(0, TITLE_Y, theme.screenW, 25, theme.bgHeader);

            firstDraw = false;
        }

        // Draw all elements
        for (auto elem : elements) {
            if (elem && elem->visible) {
                elem->draw(gfx);
            }
        }

        needsRedraw = false;
    }
};
