#include <stdio.h>
#include "pico/stdlib.h"
#include "pico-ssd1306/ssd1306.h"
#include "hardware/i2c.h"
#include "anims/idle.h"
#include "animation_controller.h"

#pragma ide diagnostic ignored "EndlessLoop"

// Use the namespace for convenience
using namespace pico_ssd1306;

int main() {
    // initialize stdio
    stdio_init_all();

    const uint LED_PIN = 17;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    // Init i2c0 controller
    i2c_init(i2c0, 1000000);
    // Set up pins 12 and 13
    gpio_set_function(4, GPIO_FUNC_I2C);
    gpio_set_function(5, GPIO_FUNC_I2C);
    gpio_pull_up(4);
    gpio_pull_up(5);

    // If you don't do anything before initializing a display pi pico is too fast and starts sending
    // commands before the screen controller had time to set itself up, so we add an artificial delay for
    // ssd1306 to set itself up
    sleep_ms(250);

    // Create a new display object at address 0x3D and size of 128x64
    SSD1306 display = SSD1306(i2c0, 0x3C, Size::W128xH64);

    display.setOrientation(0);
    //display.invertDisplay();

    AnimationController animation = AnimationController(&display);
    animation.setAnimation(Animation::IDLE);

    uint32_t frame_counter = 0;
    // Main loop
    uint32_t last_time = to_ms_since_boot(get_absolute_time());
    // display.clear();
    // display.addBitmapImage(44, 13, 32, 8, meow_text);
    // display.addBitmapImage(78, 41, 32, 8, meow_text);
    // display.sendBuffer();
    while(1){
        uint32_t now = to_ms_since_boot(get_absolute_time());
        
        if(now - last_time > 750){
            // display.clear();
            last_time = now;
            animation.start_meow();
            // animation.meow_update();
            // display.sendBuffer();
        }
        
        display.clear();
        animation.update();
        animation.meow_update();
        display.sendBuffer();
        
        
        sleep_ms(250);
        frame_counter ++;
        //tight_loop_contents();
    }
}