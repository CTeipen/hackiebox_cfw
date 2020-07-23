#include "BoxBattery.h"
#include "BoxEvents.h"

void BoxBattery::begin() {
    reloadConfig();

    pinMode(8, INPUT); //Charger pin

    _wasLow = false;
    _wasCritical = false;
    _batteryAdcRaw = analogRead(60);
    _batteryAdcLowRaw = 9999;

    loop();
    logBatteryStatus();

    setInterval(500);
}
void BoxBattery::loop() {
    _batteryAdcRaw = analogRead(60);
    _charger.read();
    
    if (_batteryAdcRaw < _batteryAdcLowRaw || isChargerConnected())
        _batteryAdcLowRaw = _batteryAdcRaw;

    if (_charger.wasPressed()) {
        Events.handleBatteryEvent(BatteryEvent::CHR_CONNECT);
    } else if (_charger.wasReleased()) {
        Events.handleBatteryEvent(BatteryEvent::CHR_DISCONNECT);
    }

    if (!isChargerConnected()) {
        if (!_wasCritical && isBatteryCritical()) {
            _wasCritical = true;
            Events.handleBatteryEvent(BatteryEvent::BAT_CRITICAL);
        } else if (!_wasLow && isBatteryLow()) {
            _wasLow = true;
            Events.handleBatteryEvent(BatteryEvent::BAT_LOW);
        }
    } else {
        _wasLow = false;
        _wasCritical = false;
    }

    _batteryTestThread.runIfNeeded();
}

bool BoxBattery::isChargerConnected() {
    if (_charger.isPressed())
        return true;
    return false;
}
uint16_t BoxBattery::getBatteryAdcRaw() {
    return _batteryAdcRaw;
}
uint16_t BoxBattery::getBatteryVoltage() {
    if (isChargerConnected()) {
        return 10000 * getBatteryAdcRaw() / _batteryVoltageChargerFactor;
    }
    return 10000 * getBatteryAdcRaw() / _batteryVoltageFactor;
}
bool BoxBattery::isBatteryLow() {
    if (getBatteryAdcRaw() < _batteryLowAdc)
        return true;
    return false;
}
bool BoxBattery::isBatteryCritical() {
    if (getBatteryAdcRaw() < _batteryCriticalAdc)
        return true;
    return false;
}

void BoxBattery::logBatteryStatus() {
    int voltageDec = getBatteryVoltage();
    int voltageNum = voltageDec / 100;
    voltageDec = voltageDec - voltageNum * 100;

    Log.info("Battery Status:");
    Log.info(" Charging: %T", isChargerConnected());
    Log.info(" ADC Raw: %c", getBatteryAdcRaw());
    Log.info(" Estimated Voltage: %d.%s%dV", voltageNum, (voltageDec<10) ? "0": "", voltageDec);
    Log.info(" Battery Low: %T", isBatteryLow());
    Log.info(" Battery Critical: %T", isBatteryCritical());
}

void BoxBattery::reloadConfig() { 
    ConfigStruct* config = Config.get();

    _batteryVoltageFactor = config->battery.voltageFactor;
    _batteryVoltageChargerFactor = config->battery.voltageChargerFactor;
    _batteryLowAdc = config->battery.lowAdc;
    _batteryCriticalAdc = config->battery.criticalAdc;
}

void BoxBattery::_doBatteryTestStep() {
    Log.info("Write battery test data...");

    FileFs file;
    if (file.open(_batteryTestFilename, FA_OPEN_APPEND | FA_WRITE)) {
        int voltageDec = getBatteryVoltage();
        int voltageNum = voltageDec / 100;
        voltageDec = voltageDec - voltageNum * 100;
        
        char* output;
        asprintf(&output, "%i;%i;%i;%i.%s%i;%i;%i;",
            (millis()-_batteryTestStartMillis) / (1000*60),
            isChargerConnected(),
            getBatteryAdcRaw(),
            voltageNum, (voltageDec<10) ? "0": "", voltageDec,
            isBatteryLow(),
            isBatteryCritical()
        );
        file.writeString(output);
        free(output);

        file.writeString("\r\n");
        file.close();
    } else {
        Log.error("Could not write battery logfile %", _batteryTestFilename);
    }
}
void BoxBattery::startBatteryTest() {
    Log.info("Starting battery test...");

    _batteryTestThread.enabled = true;
    _batteryTestStartMillis = millis();
    FileFs file;
    if (file.open(_batteryTestFilename, FA_CREATE_ALWAYS | FA_WRITE)) {
        char* output;
        
        file.writeString("Timestamp;");
        file.writeString("Charging;");
        file.writeString("ADC;");
        file.writeString("Estimated Voltage;");
        file.writeString("Low;");
        file.writeString("Critical;");
        file.writeString("Comments");
        file.writeString("\r\n");
        file.writeString("0;;;;;;");
        asprintf(&output, "vFactor=%u, vChargerFactor=%u;", _batteryVoltageFactor, _batteryVoltageChargerFactor);
        file.writeString(output);
        free(output);
        file.writeString("\r\n");
        file.close();

       _batteryTestThread.run();
    } else {
        Log.error("Could not initialize battery logfile %s", _batteryTestFilename);
        _batteryTestThread.enabled = false;
    }
}
void BoxBattery::stopBatteryTest() {
    if (!_batteryTestThread.enabled)
        return;
    Log.info("Stopping battery test...");
    _batteryTestThread.enabled = false;
    _doBatteryTestStep();
    FileFs file;
    if (file.open(_batteryTestFilename, FA_OPEN_APPEND | FA_WRITE)) {
        char* output;
        asprintf(&output, "%i;;;;;;stopped", (millis()-_batteryTestStartMillis) / (1000*60));
        file.writeString(output);
        free(output);
        file.writeString("\r\n");
        file.close();
    } else {
        Log.error("Could not write battery logfile %s", _batteryTestFilename);
        _batteryTestThread.enabled = false;
    }
}
bool BoxBattery::batteryTestActive() {
    return _batteryTestThread.enabled;
}

BoxBattery::BatteryStats BoxBattery::getBatteryStats() {
    BoxBattery::BatteryStats stats;
    
    stats.charging = isChargerConnected();
    stats.low = isBatteryLow();
    stats.critical = isBatteryCritical();
    stats.adcRaw = _batteryAdcRaw;
    stats.voltage = getBatteryVoltage();
    stats.testActive = batteryTestActive();
    stats.testActiveMinutes = (millis()-_batteryTestStartMillis) / (1000*60);

    return stats;
}