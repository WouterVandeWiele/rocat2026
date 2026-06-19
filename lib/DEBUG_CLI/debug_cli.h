#pragma once

#include "../PowerManagerDriver/power_manager_driver.h"
#include "../RTC_DRIVER/rtc_driver.h"
#include "../ACCELEROMETER_DRIVER/accelerometer_driver.h"
#include "../BATTERY_DRIVER/battery_driver.h"
#include "../WIFI_DRIVER/wifi_driver.h"
#include <SimpleCLI.h>

class DebugCLI {
public:
    DebugCLI(PowerManagerDriver& power, RtcDriver& rtc,
             AccelerometerDriver& accel, BatteryDriver& battery,
             WifiDriver& wifi);

    void begin();

private:
    PowerManagerDriver&  _power;
    RtcDriver&           _rtc;
    AccelerometerDriver& _accel;
    BatteryDriver&       _battery;
    WifiDriver&          _wifi;
    SimpleCLI            _cli;

    static DebugCLI* _instance;
    static void _on_shutdown(cmd* c);
    static void _on_get_time(cmd* c);
    static void _on_set_time(cmd* c);
    static void _on_get_alarm(cmd* c);
    static void _on_set_alarm(cmd* c);
    static void _on_disable_alarm(cmd* c);
    static void _on_clear_alarm(cmd* c);
    static void _on_acc_en(cmd* c);
    static void _on_acc_dis(cmd* c);
    static void _on_get_accel(cmd* c);
    static void _on_get_battery(cmd* c);
    static void _on_gpio(cmd* c);
    static void _on_wifi_reset(cmd* c);
    static void _on_error(cmd_error* e);

    static void _task(void* arg);
    void        _run();
};
