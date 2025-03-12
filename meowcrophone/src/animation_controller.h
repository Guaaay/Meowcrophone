
#pragma once

#include "idle.h"
#include "meow_text.h"
#include "meow_eye.h"
#include "ssd1306.h"

using namespace pico_ssd1306;

//This class controls the animations. It can control animation speed and select which animation to display

//The number of meows that can be displayed at once
#define MAX_NUM_MEOWS 3
#define NUM_MEOW_FRAMES 5

enum class Animation {
    IDLE
};

struct meow_pos {
    //Default meow start position
    uint8_t frame = 0;
    bool visible = false;
    uint8_t x = 44;
    uint8_t y = 13;
};

class AnimationController {
    SSD1306 * display;
    Animation current_anim;
    int frame = 0;
    uint8_t num_meows = 0;
    meow_pos meows[MAX_NUM_MEOWS]; //Array of meow positions
    
public:
    AnimationController(SSD1306 * display);

    // Goes to next frame of animation and sends the buffer to the display
    // The animation will loop by default
    void update();
    void update_half();

    void start_meow(); //Generate a new meow
    void meow_update(); //Move all meows
    void update_meow_position(meow_pos *meow);
    void reset_meow_position(meow_pos *meow);
    void happy_eyes();

    // Sets the current animation
    void setAnimation(Animation anim);
};