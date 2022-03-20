//statusled.cpp
// Usage:
//      - Create object of class with defined LED pins. Assumes exactly 3 pins. Will probably work with less, if you duplicate the same pin (i.e. call constructor with (2,2,2)).
//      - Class expects regular calls to tick() every now and then to deal with updates for flashes
//      - LEDs are updated one on set, so no need to regularly call tick() if only using static LEDs
//      - Set various LEDs on, off or flashing using dedicated methods (on(), off() or flash()), or using composite methods (set())
// To improve:
//      - Dynamically add LEDs to handle.
//      - Flip data orientation so that a data object is used per LED instead of an array per attribute, also makes ^ easier to do :)

#include "statusled.h"

void StatusLed::tick() {
    if (millis() > min_time) { // To save procesing power, nothing needs to be done unless the time has come for an update to be run - which will be indicated in min_time (which should be the minimum of all next_times[] elements)
        //Serial.println("Time for another LED tick");
        output();
        // Improvement idea: Send millis() value to the output method. Might handle some odd corner case where stuff desync's as well (millis() updates between call to tick() and running of output())
    }
}

void StatusLed::on(uint8_t pin) { // Turn selected LED on
    StatusLed::set(pin, status_t::on);
}
void StatusLed::off(uint8_t pin) { // Turn selected LED off
    StatusLed::set(pin, status_t::off);
}
void StatusLed::off() { // Turn all LEDs off
    for (auto p : pins) {
        set_internal(p, status_t::off, 0, 0);
    }
    output();
}
void StatusLed::flash(uint8_t pin, int time_on, int time_off) { // Make selected LED flash. Times are in ms
    StatusLed::set(pin, status_t::flash, time_on, time_off);
}
void StatusLed::set(uint8_t pin, StatusLed::status_t s) { // Shorthand to set status directly, assumes 0 ms times
    StatusLed::set(pin, s, 0, 0);
}
void StatusLed::set(uint8_t pin, StatusLed::status_t s, int time_on, int time_off) { // Proper full initialization method. Sets selected LED into selected status mode, with on and off times in ms.
    set_internal(pin, s, time_on, time_off);
    // Force an update, to make statics "stick"
    output();
}
void StatusLed::set_internal(uint8_t pin, StatusLed::status_t s, int time_on, int time_off) { // Most of the work happens here, so that we can update multiple without running output() for each.
    int p = lookup(pin); // Convert from pin number to position in array
    //pins[p] = pin; // Should it be possible to dynamically change pins... that sounds nuts. Move to constructor. It is _theoretically_ possible to do it that way, I guess...
    LED_Status[p] = s; // Set status
    on_times[p] = time_on; // Set on-time
    off_times[p] = time_off; // Set off-time
    if (s == status_t::off) { // If requested to turn off, obviously start with LED off
        outputs[p] = false;
    } else { // Set LED to on - default to start LED on when flashing
        outputs[p] = true;
    }
    if (s == status_t::flash) { // Deal with flashing specials
        next_times[p] = 0; // This will guarantee an update the next time output is called
        min_time = millis(); // This will guarantee that the update is run (to save processing power in tick() )
    } else {
        next_times[p] = (unsigned long) -1; // Set time to "forever", so that the update checker doesn't have to do anything
    }
}

StatusLed::StatusLed() { // Default stupid constructor using LED 2... 3 times. No clue how that would work in reality
    StatusLed(2,2,2); 
}

StatusLed::StatusLed(uint8_t p[]) { // Constructor using array, expects EXACTLY 3 elements (more will be ignored, less will segfault)
    StatusLed(p[0], p[1], p[2]);
}
StatusLed::StatusLed(uint8_t p0, uint8_t p1, uint8_t p2) { // Constructor using direct separate pin numbers
    // Initialize all variables
    for(auto s : LED_Status)    { s = status_t::off; }
    for(auto o : outputs)       { o = false; }
    pins[0] = p0;
    pins[1] = p1;
    pins[2] = p2;
    for(auto t : on_times)      { t = 0; }
    for(auto t : off_times)     { t = 0; }
    for(auto n : next_times)    { n = (unsigned long) -1; } // Default to "never" check times (this number be big)
    min_time = (unsigned long) -1;    
}

void StatusLed::output() { // Check time, run flasher updates, check for changes in permanents, and do stuff!
    int i = 0;
    unsigned long current_time = millis();
    // Are we here early, from a new Set?
    if (current_time < min_time) {
        // Do nothing to min_time - this keeps our old min_time which will be our next regular checkup. This was an additional one, which might mean we won't re-set min_time later on, and if we do, we want to compare to the "correct" next time anyway
    } else {
        min_time = (unsigned long) -1; // We're handling a "timer" right now, so we move the next time we get here naturally forward. Default is "never", but any flash will lower this.
    }
    for (auto p: pins) {
        switch(LED_Status[i]) {
            case status_t::on: // Check for new permanent on
                outputs[i] = true;
                break;
            case status_t::off: // Check for new permanent off
                outputs[i] = false;
                break;
            case status_t::flash: // Handle flashers
                //Serial.println("Checking a flash! " + String(next_times[i], DEC));
                if(current_time > next_times[i]) { // If it is time to change states (1->0 or 0->1)
                    unsigned long t = 0;
                    if (outputs[i]) { // This is a transition 1->0
                        t = current_time + off_times[i]; // So we're then waiting off_times time before we check again
                    } else { // 0->1
                        t = current_time + on_times[i]; // So we wait on_times time
                    }
                    next_times[i] = t; // Update when we're next supposed to check
                    if (t < min_time) // See if that is before the currently stored "master" value (keeping track of the min of all elements of next_times)
                        min_time = t;
                    outputs[i] = !outputs[i]; // Flip the output to actually do the flash
                }
                break;
        }
        digitalWrite(p, outputs[i]); // Implicit type conversion from bool to uint_8t
        i++;
    }
    //digitalWrite(pins[i], HIGH);
    //digitalWrite(pins[i], LOW);
    //String s = "We've now handled LEDS at " + String(current_time,DEC);
    //s += ". ";
    //s += "Next time to handle is at ";
    //s += String(min_time,DEC);
    //Serial.println(s);
}

int StatusLed::lookup(uint8_t pin) { // Converts a pin number into an index in the internal storage arrays
    int i = 0;
    for(int p : pins) {
        if(p == pin)
            return i;
        i++;
    }
    return 0;
}