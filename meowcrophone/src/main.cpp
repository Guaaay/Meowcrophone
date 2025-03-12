/*
 * Copyright (c) 2021 Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * 
 */

#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"

extern "C" {
#include "pico/pdm_microphone.h"
}

#include "tflite_model.h"

#include "dsp_pipeline.h"
#include "ml_model.h"

#include "ssd1306.h"
#include "hardware/i2c.h"
#include "idle.h"
#include "animation_controller.h"
#include "clock_controller.h"
#include "textRenderer/TextRenderer.h"

// constants
#define SAMPLE_RATE       16000
#define FFT_SIZE          256
#define SPECTRUM_SHIFT    4
#define INPUT_BUFFER_SIZE ((FFT_SIZE / 2) * SPECTRUM_SHIFT)
#define INPUT_SHIFT       0
#define SOUND_1           250
#define SOUND_2           500
#define C_NOTE_FREQ       523.3f
#define G_NOTE_FREQ       784.0f
#define CLOCK_BLINK_TIME  500


#define MILLIS() to_ms_since_boot(get_absolute_time())

//Buzzer
#define BUZZER_PIN 6
//Button
#define BUTTON_PIN 29

// microphone configuration
const struct pdm_microphone_config pdm_config = {
    // GPIO pin for the PDM DAT signal
    .gpio_data = 8,

    // GPIO pin for the PDM CLK signal
    .gpio_clk = 9,

    // PIO instance to use
    .pio = pio0,

    // PIO State Machine instance to use
    .pio_sm = 0,

    // sample rate in Hz
    .sample_rate = SAMPLE_RATE,

    // number of samples to buffer
    .sample_buffer_size = INPUT_BUFFER_SIZE,
};

q15_t capture_buffer_q15[INPUT_BUFFER_SIZE];
volatile int new_samples_captured = 0;

q15_t input_q15[INPUT_BUFFER_SIZE + (FFT_SIZE / 2)];

DSPPipeline dsp_pipeline(FFT_SIZE);
MLModel ml_model(tflite_model, 128 * 1024);

int8_t* scaled_spectrum = nullptr;
int32_t spectogram_divider;
float spectrogram_zero_point;

void on_pdm_samples_ready();


// Use the namespace for convenience
using namespace pico_ssd1306;

void setup_ssd1306(){
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

}



void set_pwm_frequency(uint gpio, uint32_t frequency) {
    
    // Find the PWM slice and channel associated with the GPIO pin
    uint slice_num = pwm_gpio_to_slice_num(gpio);
    uint channel = pwm_gpio_to_channel(gpio);

    // Calculate the clock divider and counter wrap value
    uint32_t clock = 125 * 1000 * 1000; // 125 MHz
    uint32_t divider16 = clock / (frequency * 4096);
    uint32_t wrap = (clock * 16) / (divider16 * frequency) - 1;

    // Set the PWM configuration
    pwm_set_clkdiv(slice_num, divider16 / 16.0);
    pwm_set_wrap(slice_num, wrap);

    // Set the PWM level (duty cycle)
    pwm_set_chan_level(slice_num, channel, wrap / 2); // 50% duty cycle
    pwm_set_enabled(slice_num, true);
}

void setup_pwm(uint gpio) {
    gpio_set_function(gpio, GPIO_FUNC_PWM); // Set GPIO function to PWM
    set_pwm_frequency(gpio, 440); // Set initial frequency (e.g., 440 Hz for A4)
    uint slice_num = pwm_gpio_to_slice_num(gpio);
    pwm_set_enabled(slice_num, false); // Enable PWM
}

void setup_button(uint gpio) {
    gpio_init(gpio);
    gpio_set_dir(gpio, GPIO_IN);
    gpio_pull_up(gpio);
}


int main( void )
{

    setup_ssd1306();
    SSD1306 display = SSD1306(i2c0, 0x3C, Size::W128xH64);
    display.setOrientation(0);

    AnimationController animation = AnimationController(&display);
    animation.setAnimation(Animation::IDLE);

    ClockController clock = ClockController(&display);
    const char time[] = __TIME__;
    
    clock.set_time(clock.time_to_millis(time)+30000);

    uint32_t frame_counter = 0;
    uint32_t last_time = MILLIS();

    // initialize stdio
    stdio_init_all();

    printf("hello pico meow detection\n");

    gpio_set_function(PICO_DEFAULT_LED_PIN, GPIO_FUNC_PWM);
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    
    uint pwm_slice_num = pwm_gpio_to_slice_num(PICO_DEFAULT_LED_PIN);
    uint pwm_chan_num = pwm_gpio_to_channel(PICO_DEFAULT_LED_PIN);

    uint pwm_slice_num_buzzer = pwm_gpio_to_slice_num(BUZZER_PIN);
    uint pwm_chan_num_buzzer = pwm_gpio_to_channel(BUZZER_PIN);


    pwm_set_clkdiv (pwm_slice_num_buzzer, 4.375);
    pwm_set_wrap(pwm_slice_num_buzzer, 64949);
    pwm_set_enabled(pwm_slice_num_buzzer, true);

    // Set period of 256 cycles (0 to 255 inclusive)
    pwm_set_wrap(pwm_slice_num, 256);

    // Set the PWM running
    pwm_set_enabled(pwm_slice_num, true);

    if (!ml_model.init()) {
        printf("Failed to initialize ML model!\n");
        while (1) { tight_loop_contents(); }
    }

    if (!dsp_pipeline.init()) {
        printf("Failed to initialize DSP Pipeline!\n");
        while (1) { tight_loop_contents(); }
    }

    scaled_spectrum = (int8_t*)ml_model.input_data();
    spectogram_divider = 64 * ml_model.input_scale(); 
    spectrogram_zero_point = ml_model.input_zero_point();

    // initialize the PDM microphone
    if (pdm_microphone_init(&pdm_config) < 0) {
        printf("PDM microphone initialization failed!\n");
        while (1) { tight_loop_contents(); }
    }

    // set callback that is called when all the samples in the library
    // internal sample buffer are ready for reading
    pdm_microphone_set_samples_ready_handler(on_pdm_samples_ready);

    // start capturing data from the PDM microphone
    if (pdm_microphone_start() < 0) {
        printf("PDM microphone start failed!\n");
        while (1) { tight_loop_contents(); }
    }

    
    int meow_counter = 0;
    int counter_offset = 0;
    bool play_sound = true;
    bool consecutive = false;
    int time_playing = 0;
    bool clock_mode = false;
    bool edit_blink = false;
    bool last_button_state = 1;
    int time_since_pressed = 0;
    bool holding = false;
    int clock_blink_time = 0;
    setup_pwm(BUZZER_PIN);
    setup_button(BUTTON_PIN);

    while (1) {
        if(!gpio_get(BUTTON_PIN)){
            if(last_button_state == 1){
                printf("Button Pressed\n");
                time_since_pressed = MILLIS();
                holding = true;
            }
            else{
                if(MILLIS() - time_since_pressed > 1000){
                    if(clock_mode){
                        if(clock.is_editing()){
                            printf("change cursor\n");
                            clock.change_cursor();
                        }
                        else{
                            clock.edit_mode(true);
                        }
                    }
                    time_since_pressed = MILLIS();
                    holding = false;
                }
            }
            last_button_state = 0;
        }
        else{
            if(last_button_state == 0 && holding){
                printf("Button Released\n");
                if(clock.is_editing()){
                    clock.increment();
                }
                else{
                    clock_mode = !clock_mode;
                }
                
                holding = false;
            }
            last_button_state = 1;
        }
        


        // wait for new samples
        while (new_samples_captured == 0) {
            tight_loop_contents();
        }
        new_samples_captured = 0;

        dsp_pipeline.shift_spectrogram(scaled_spectrum, SPECTRUM_SHIFT, 124);

        // move input buffer values over by INPUT_BUFFER_SIZE samples
        memmove(input_q15, &input_q15[INPUT_BUFFER_SIZE], (FFT_SIZE / 2));

        // copy new samples to end of the input buffer with a bit shift of INPUT_SHIFT
        arm_shift_q15(capture_buffer_q15, INPUT_SHIFT, input_q15 + (FFT_SIZE / 2), INPUT_BUFFER_SIZE);
    
        for (int i = 0; i < SPECTRUM_SHIFT; i++) {
            dsp_pipeline.calculate_spectrum(
                input_q15 + i * ((FFT_SIZE / 2)),
                scaled_spectrum + (129 * (124 - SPECTRUM_SHIFT + i)),
                spectogram_divider, spectrogram_zero_point
            );
        }
        uint32_t now = MILLIS();

        float prediction = ml_model.predict();

        if (prediction >= 0.8) {
          printf("\tMEOW\tdetected!\t(prediction = %f)\n\n", prediction);
          animation.start_meow();
          
          if(meow_counter < 999 && !consecutive){
            meow_counter++;
            consecutive = true;
          }
          play_sound = true;
          time_playing = now;

        } else {
            consecutive = false;
          //printf("\tMEOW\tNOT detected\t(prediction = %f)\n\n", prediction);
        }

        pwm_set_chan_level(pwm_slice_num, pwm_chan_num, prediction * 255);


        //Handle display
        
        if(play_sound){
            if(now - time_playing > SOUND_1){
                if(now - time_playing > SOUND_2){
                    printf("STOP_SOUND");
                    //set_pwm_frequency(BUZZER_PIN, 1);
                    play_sound = false;
                }
                else{
                    printf("PLAY_G");
                    //set_pwm_frequency(BUZZER_PIN, G_NOTE_FREQ);
                }
            }
            else{
                printf("PLAY_C");
                //set_pwm_frequency(BUZZER_PIN, C_NOTE_FREQ);
            }
        }

        
        clock.update_time(MILLIS());
        if(now - last_time > 250){
            if(clock_mode){
                display.clear();
                if(now - clock_blink_time > CLOCK_BLINK_TIME){
                    if(clock.is_editing()){
                        edit_blink = !edit_blink;
                    }
                    else if(clock_mode){
                        edit_blink = false;
                    }
                    clock_blink_time = MILLIS();
                }
                
                if(!edit_blink){
                    //display.clear();
                    clock.render_time(0);
                }
                else{
                    clock.render_cursor();
                }
                animation.update_half();
                display.sendBuffer();
            }
            else{
                if(meow_counter < 10){
                    counter_offset = 32;
                }
                else if(meow_counter < 100){
                    counter_offset = 16;
                }
                else{
                    counter_offset = 0;
                }
                
                
                display.clear();
                animation.update();
                animation.meow_update();
                char meow_counter_str[10];
                sprintf(meow_counter_str, "%d", meow_counter);
                drawTextDouble(&display, font_16x32, meow_counter_str, 72+counter_offset ,0);
                display.sendBuffer();
                frame_counter ++;
            }
            last_time = now;
        }

        

    }

    return 0;
}

void on_pdm_samples_ready()
{
    // callback from library when all the samples in the library
    // internal sample buffer are ready for reading 

    // read in the new samples
    new_samples_captured = pdm_microphone_read(capture_buffer_q15, INPUT_BUFFER_SIZE);
}
