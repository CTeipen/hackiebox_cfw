#ifndef BoxEvents_h
#define BoxEvents_h

#include "BaseHeader.h"
#include "Hackiebox.h"
#include "WrapperWiFi.h"
#include "BoxPower.h"
#include "BoxRFID.h"

class BoxEvents {
    public:
        enum class EventSource {
           BATTERY,
           EAR,
           WIFI,
           POWER,
           ACCELEROMETER,
           TAG
        };

        void
            begin(),
            loop();
        
        void handleEarEvent(BoxButtonEars::EarButton earId, BoxButtonEars::PressedType pressType, BoxButtonEars::PressedTime pressLength);
        void handleBatteryEvent(BoxBattery::BatteryEvent state);
        void handleWiFiEvent(WrapperWiFi::ConnectionState state);
        void handlePowerEvent(BoxPower::PowerEvent event);
        void handleAccelerometerOrientationEvent(BoxAccelerometer::Orientation orient);
        void handleTagEvent(BoxRFID::TAG_EVENT event);

    private:
};

extern BoxEvents Events;

#endif