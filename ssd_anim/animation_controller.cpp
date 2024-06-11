#include "animation_controller.h"
#include <stdio.h>
using namespace pico_ssd1306;

AnimationController::AnimationController(SSD1306 *display) {
    this->display = display;
    this->current_anim = Animation::IDLE;
}

void AnimationController::update() {
    switch (current_anim) {
        case Animation::IDLE:
            display->addBitmapImage(0, 0, 128, 64, idle_anim_all[frame]);
            //display->sendBuffer();
            frame = (frame + 1) % IDLE_FRAMES;
            break;
    }
}

void AnimationController::setAnimation(Animation anim) {
    current_anim = anim;
}

void AnimationController::start_meow() {
    for (uint8_t i = 0; i < MAX_NUM_MEOWS; i++) {
        if(!meows[i].visible){
            meows[i].visible = true;
            break;
        }
    }
}


void AnimationController::reset_meow_position(meow_pos *meow){
    printf("resetting meow position\n");
    meow->x = 44;
    meow->y = 13;
    meow->visible = false;
}

void AnimationController::update_meow_position(meow_pos *meow){
    
    switch(meow->frame){
        case 0:
            meow->x = 61;
            meow->y = 28;
            break;
        case 1:
            meow->x = 81;
            meow->y = 40;
            break;
        case 2:
            meow->x = 98;
            meow->y = 28;
            break;
        case 3:
            meow->x = 126;
            meow->y = 20;
            break;
        case 4:
            reset_meow_position(meow);
    }
    printf("frame %d x: %d y: %d\n", meow->frame, meow->x, meow->y);
    meow->frame = (meow->frame + 1) % NUM_MEOW_FRAMES;
}

void AnimationController::meow_update() {
    for (uint8_t i = 0; i < MAX_NUM_MEOWS; i++) {
        if (meows[i].visible) {
            printf("meow %d visible\n", i);
            display->addBitmapImage(meows[i].x, meows[i].y, 32, 8, meow_text);
            //display->sendBuffer();
            update_meow_position(&meows[i]);
        }
        else{
            //printf("meow %d not visible\n", i);
        }
    }
}