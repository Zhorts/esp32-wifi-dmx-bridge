#ifndef _STATUSLED_H_
#define _STATUSLED_H_
#include <Arduino.h>
class StatusLed {
    public:
        enum class status_t{off, on, flash};
        void tick();
        void on(uint8_t pin);
        void off(uint8_t pin);
        void off();
        void flash(uint8_t pin, int time_on, int time_off);
        void set(uint8_t pin, status_t s);
        void set(uint8_t pin, status_t s, int time_on, int time_off);
        StatusLed();
        StatusLed(uint8_t p[]);
        StatusLed(uint8_t p0, uint8_t p1, uint8_t p2);
        void output();

    protected:
        status_t LED_Status[3];
        bool outputs[3];
        uint8_t pins[3];
        int on_times[3];
        int off_times[3];
        unsigned long next_times[3];
        unsigned long min_time;
        int lookup(uint8_t pin);
        void set_internal(uint8_t pin, status_t s, int time_on, int time_off);
        

};
#endif
