#include "clock_controller.h"
#include <stdio.h>

ClockController::ClockController(SSD1306 *display) {
    this->display = display;
    this->current_time = 0;
    this->last_update = 0;
}


void ClockController::increment(){
    if(editing){
        if(pos == 0){
            current_time += 1000*60;
        }else if(pos == 1){
            current_time += 1000*60*60;
        }
    }
}

void ClockController::change_cursor(){
    pos += 1;
    if(pos == 2){
        pos = 0;
        edit_mode(false);
    }
}

void ClockController::edit_mode(bool mode){
    editing = mode;
}

void ClockController::set_time(uint32_t time){
    current_time = time;
}

void ClockController::update_time(uint32_t current) {
    uint32_t time_increment = current - last_update;
    current_time += time_increment;
    last_update = current;

    //Flip over to 0 if we reach 24 hours
    if(current_time > 1000*60*60*24){
        current_time = 0;
    }
}


uint32_t ClockController::time_to_millis(const char * time_str){
    uint32_t hours = (time_str[0] - '0')*10 + (time_str[1] - '0');
    uint32_t minutes = (time_str[3] - '0')*10 + (time_str[4] - '0');
    return (hours*60 + minutes)*60*1000;

}

void ClockController::render_cursor(){
    if(pos == 0){
        render_time(1);
    }
    else if(pos == 1){
        render_time(2
        
        
        );
    }
}

void ClockController::render_time(int mode) {
    display->clear();
    char hours_str[3];
    char minutes_str[3];
    uint32_t seconds = (current_time / 1000)%60;
    uint32_t minutes = (current_time / (1000*60))%60;
    uint32_t hours = (current_time / (1000*60*60))%24;
    sprintf(hours_str, "%02d", hours);
    sprintf(minutes_str, "%02d", minutes);
    int initial_pos = 8;
    if(mode == 0){
        drawTextDouble(display, font_16x32, hours_str, initial_pos, 0);
        drawTextDouble(display, font_16x32, ":", initial_pos + TIME_OFFSET*2, 0);
        drawTextDouble(display, font_16x32, minutes_str, initial_pos + TIME_OFFSET*3, 0);
    }
    else if(mode == 1){
        drawTextDouble(display, font_16x32, hours_str, initial_pos, 0);
        drawTextDouble(display, font_16x32, ":", initial_pos + TIME_OFFSET*2, 0);
    }
    else if(mode == 2){
        drawTextDouble(display, font_16x32, ":", initial_pos + TIME_OFFSET*2, 0);
        drawTextDouble(display, font_16x32, minutes_str, initial_pos + TIME_OFFSET*3, 0);
    }
    
}