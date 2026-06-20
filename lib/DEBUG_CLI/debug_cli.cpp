#include "debug_cli.h"
#include <Arduino.h>

DebugCLI* DebugCLI::_instance = nullptr;

DebugCLI::DebugCLI(PowerManagerDriver& power, RtcDriver& rtc,
                   AccelerometerDriver& accel, BatteryDriver& battery,
                   WifiDriver& wifi, AudioWebDriver& webAudio)
    : _power(power), _rtc(rtc), _accel(accel), _battery(battery), _wifi(wifi), _webAudio(webAudio) {}

void DebugCLI::begin() {
    _instance = this;

    _cli.addCommand("shutdown", _on_shutdown);

    Command cmdSetTime = _cli.addCommand("set_time", _on_set_time);
    cmdSetTime.addPositionalArgument("year");
    cmdSetTime.addPositionalArgument("month");
    cmdSetTime.addPositionalArgument("day");
    cmdSetTime.addPositionalArgument("hour");
    cmdSetTime.addPositionalArgument("minute");
    cmdSetTime.addPositionalArgument("second");

    _cli.addCommand("get_time", _on_get_time);

    Command cmdSetAlarm = _cli.addCommand("set_alarm", _on_set_alarm);
    cmdSetAlarm.addPositionalArgument("day");
    cmdSetAlarm.addPositionalArgument("hour");
    cmdSetAlarm.addPositionalArgument("minute");

    _cli.addCommand("get_alarm", _on_get_alarm);
    _cli.addCommand("disable_alarm", _on_disable_alarm);
    _cli.addCommand("clear_alarm", _on_clear_alarm);
    _cli.addCommand("acc_en", _on_acc_en);
    _cli.addCommand("acc_dis", _on_acc_dis);


    _cli.addCommand("get_accel", _on_get_accel);
    _cli.addCommand("get_battery", _on_get_battery);

    Command cmdGpio = _cli.addCommand("gpio", _on_gpio);
    cmdGpio.addPositionalArgument("pin");
    cmdGpio.addPositionalArgument("value");

    _cli.addCommand("wifi_reset", _on_wifi_reset);

    Command cmdRadioPlay = _cli.addCommand("radio_play", _on_radio_play);
    cmdRadioPlay.addPositionalArgument("station", "0");
    _cli.addCommand("radio_stop", _on_radio_stop);
    _cli.addCommand("radio_next", _on_radio_next);
    _cli.addCommand("radio_prev", _on_radio_prev);

    _cli.setOnError(_on_error);

    xTaskCreate(_task, "debug_cli", 4096, this, 1, nullptr);
}

void DebugCLI::_on_shutdown(cmd* c) {
    Serial.println("[CLI] shutting down");
    _instance->_power.shut_down();
}

void DebugCLI::_on_get_time(cmd* c) {
    DateTime now = _instance->_rtc.get_time();
    char buf[24];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
             now.year(), now.month(), now.day(),
             now.hour(), now.minute(), now.second());
    Serial.println(buf);
}

void DebugCLI::_on_set_time(cmd* c) {
    Command cmd(c);
    int year   = cmd.getArgument("year").getValue().toInt();
    int month  = cmd.getArgument("month").getValue().toInt();
    int day    = cmd.getArgument("day").getValue().toInt();
    int hour   = cmd.getArgument("hour").getValue().toInt();
    int minute = cmd.getArgument("minute").getValue().toInt();
    int second = cmd.getArgument("second").getValue().toInt();
    _instance->_rtc.set_time(DateTime(year, month, day, hour, minute, second));
    Serial.println("[CLI] time set");
    Serial.printf("%ld-%ld-%ld %ld:%ld:%ld\n", year, month, day, hour, minute, second);
}

void DebugCLI::_on_get_alarm(cmd* c) {
    AlarmSettings a = _instance->_rtc.get_alarm();
    char buf[20];
    snprintf(buf, sizeof(buf), "day=%02d hour=%02d min=%02d", a.day, a.hour, a.minute);
    Serial.println(buf);
}

void DebugCLI::_on_clear_alarm(cmd* c) {
    _instance->_rtc.clear_alarm_flag();
    Serial.println("[CLI] alarm cleared");
}

void DebugCLI::_on_disable_alarm(cmd* c) {
    _instance->_rtc.disable_alarm();
    Serial.println("[CLI] alarm disabled");
}

void DebugCLI::_on_acc_en(cmd* c) {
    _instance->_accel.setup_motion_interrupt(100, 20);
    Serial.println("[CLI] acc en");
}

void DebugCLI::_on_acc_dis(cmd* c) {
    _instance->_accel.clear_interrupt();
    Serial.println("[CLI] acc dis");
}



void DebugCLI::_on_set_alarm(cmd* c) {
    Command cmd(c);
    uint8_t day    = cmd.getArgument("day").getValue().toInt();
    uint8_t hour   = cmd.getArgument("hour").getValue().toInt();
    uint8_t minute = cmd.getArgument("minute").getValue().toInt();
    _instance->_rtc.set_alarm(day, hour, minute);
    Serial.println("[CLI] alarm set");
}

void DebugCLI::_on_get_accel(cmd* c) {
    float x = _instance->_accel.get_accel_x();
    float y = _instance->_accel.get_accel_y();
    float z = _instance->_accel.get_accel_z();
    char buf[40];
    snprintf(buf, sizeof(buf), "x=%.3f y=%.3f z=%.3f g", x, y, z);
    Serial.println(buf);
}

void DebugCLI::_on_get_battery(cmd* c) {
    uint16_t raw = _instance->_battery.get_raw_adc();
    float  pct = _instance->_battery.get_voltage();
    char buf[24];
    snprintf(buf, sizeof(buf), "raw=%4u  pct=%.3f", raw, pct);
    Serial.println(buf);
}

void DebugCLI::_on_gpio(cmd* c) {
    Command cmd(c);
    int pin   = cmd.getArgument("pin").getValue().toInt();
    int value = cmd.getArgument("value").getValue().toInt();
    pinMode(pin, OUTPUT);
    digitalWrite(pin, value ? HIGH : LOW);
    Serial.printf("[CLI] gpio %d = %d\n", pin, value ? 1 : 0);
}

void DebugCLI::_on_wifi_reset(cmd* c) {
    Serial.println("[CLI] resetting WiFi credentials");
    _instance->_wifi.reset();
}

void DebugCLI::_on_radio_play(cmd* c) {
    Command cmd(c);
    int idx = cmd.getArgument("station").getValue().toInt();
    _instance->_webAudio.play(idx);
}

void DebugCLI::_on_radio_stop(cmd* c) {
    _instance->_webAudio.stop();
}

void DebugCLI::_on_radio_next(cmd* c) {
    _instance->_webAudio.next();
}

void DebugCLI::_on_radio_prev(cmd* c) {
    _instance->_webAudio.previous();
}

void DebugCLI::_on_error(cmd_error* e) {
    CommandError err(e);
    Serial.print("[CLI] error: ");
    Serial.println(err.toString());
}

void DebugCLI::_task(void* arg) {
    static_cast<DebugCLI*>(arg)->_run();
}

void DebugCLI::_run() {
    String input;
    for (;;) {
        while (Serial.available()) {
            char c = Serial.read();
            if (c == '\n' || c == '\r') {
                if (input.length() > 0) {
                    Serial.print("> ");
                    Serial.println(input);
                    _cli.parse(input);
                    input.clear();
                }
            } else {
                input += c;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
