
#pragma once

#include "ssd1306.h"
#include "textRenderer/TextRenderer.h"

using namespace pico_ssd1306;

#define TIME_OFFSET 16

class ClockController {
    SSD1306 * display;
    //Current time in milliseconds
    uint32_t current_time;
    uint32_t last_update;
    bool editing = false;
    //1 is hours, 0 is minutes
    int pos = 0;
    
    //Converts the current time to a string
    
public:
    ClockController(SSD1306 * display);

    void render_time(int mode);

    void render_cursor();

    // Updates the time
    void set_time(uint32_t time);
    void update_time(uint32_t current);
    uint32_t time_to_millis(const char * time_str);


    //Called when the user presses the button
    void increment();
    void change_cursor();
    void edit_mode(bool mode);
    bool is_editing(){
        return editing;
    }

};