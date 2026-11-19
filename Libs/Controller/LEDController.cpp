#include "LEDController.h"
#include "app_runtime.h"

LEDController::LEDController()
{

    this->config = 
    {
        .enable = 1,
        .instablink_dis = 1,


        .max_current = 0,


        .out0_en = 1,
        .out1_en = 1,
        .out2_en = 1,


        .out0_fade_en = 0,
        .out1_fade_en = 0,
        .out2_fade_en = 0,
        .led_fade_time = 0,


        .out0_auto_en = 1,
        .out1_auto_en = 1,
        .out2_auto_en = 1,
        .out0_exp_en = 1,
        .out1_exp_en = 1,
        .out2_exp_en = 1,


        .out0_engine_ch = 0,
        .out1_engine_ch = 0,
        .out2_engine_ch = 0,
    };

    for(auto &pattern : patterns)
    {
        pattern.pause_t0 = 0;
        pattern.pause_t1 = 0;
        pattern.repeat = 0;
        pattern.reserved = 0;
        for(int i = 0; i < 5; i++)
        {
            pattern.PWM[i] = 0;
        }
        pattern.sloper0 = 0;
        pattern.sloper1 = 0;
        pattern.sloper2 = 0;
        pattern.sloper3 = 0;
    }

    for(auto &engine: this->engines)
    {
        engine.patterns[0] = nullptr;
        engine.patterns[1] = nullptr;
        engine.patterns[2] = nullptr;
        engine.patterns[3] = nullptr;
    }
}

void LEDController::init()
{
    // set dot current
    uint8_t txdata[3] = 
    {
        LED_DC_RED,
        LED_DC_GREEN,
        LED_DC_BLUE
    };

    App_I2cTransfer(LED_I2C_HANDLE, LED_I2C_ADDRESS << 1, LED_REG_DC, I2C_MEMADD_SIZE_8BIT, txdata, nullptr, sizeof(txdata), 10, 0);

}

/**
 * @brief Set a predefined animation for the LED controller.
 * @param animation The animation to set.
 */
void LEDController::set_animation(LED_Animation_t animation)
{
    switch(animation)
    {
        case LED_ANIMATION_OFF:
            // Handle LED_ANIMATION_OFF
            this->config.out0_en = 0;
            this->config.out1_en = 0;
            this->config.out2_en = 0;
            break;
        case LED_ANIMATION_ALL:
            // Handle LED_ANIMATION_ALL
            // All LEDs on. for calibration
            this->config.out0_en = 1;
            this->config.out1_en = 1;
            this->config.out2_en = 1;

            this->patterns[0].pause_t0 = 0;
            this->patterns[0].pause_t1 = 0;
            this->patterns[0].repeat = 0xF; //Infinite
            this->patterns[0].PWM[0] = 0xFF;
            this->patterns[0].PWM[1] = 0xFF; // 75%
            this->patterns[0].PWM[2] = 0xFF; // 100%
            this->patterns[0].PWM[3] = 0xFF; // 75%
            this->patterns[0].PWM[4] = 0xFF;
            this->patterns[0].sloper0 = 0x01; // 50ms
            this->patterns[0].sloper1 = 0x01; // 50ms
            this->patterns[0].sloper2 = 0x01; // 50ms
            this->patterns[0].sloper3 = 0x01; // 50ms --> total pattern: 200ms
            this->set_engine_pattern(0, 0, 0);

            this->disable_engine_pattern(0, 1);
            this->disable_engine_pattern(0, 2);
            this->disable_engine_pattern(0, 3);
            this->config.out0_engine_ch = 0;
            this->config.out1_engine_ch = 0;
            this->config.out2_engine_ch = 0;

            break;
        case LED_ANIMATION_CONFIGURING:
            // Handle LED_ANIMATION_CONFIGURING
            // Blue-Green Blinking, 5Hz, 180° phase shift
            this->config.out0_en = 0; //Disable Red
            this->config.out1_en = 1; //Enable Green
            this->config.out2_en = 1; //Enable Blue

            this->patterns[0].pause_t0 = 0; //No Pause
            this->patterns[0].pause_t1 = 0; //No Pause
            this->patterns[0].repeat = 0xF; //Infinite
            this->patterns[0].PWM[0] = 0x00;
            this->patterns[0].PWM[1] = 0xBF; // 75%
            this->patterns[0].PWM[2] = 0xFF; // 100%
            this->patterns[0].PWM[3] = 0xBF; // 75%
            this->patterns[0].PWM[4] = 0x00;
            this->patterns[0].sloper0 = 0x01; // 50ms
            this->patterns[0].sloper1 = 0x01; // 50ms
            this->patterns[0].sloper2 = 0x01; // 50ms
            this->patterns[0].sloper3 = 0x01; // 50ms --> total pattern: 200ms
            this->set_engine_pattern(0, 0, 0); //Set Engine0 to Pattern0
            this->disable_engine_pattern(0, 1);
            this->disable_engine_pattern(0, 2);
            this->disable_engine_pattern(0, 3); //Disable all other patterns
            this->config.out1_engine_ch = 0; //Set Green to Engine0


            this->patterns[1].pause_t0 = 0; //No Pause
            this->patterns[1].pause_t1 = 0; //No Pause
            this->patterns[1].repeat = 0xF; //Infinite
            this->patterns[1].PWM[0] = 0xFF; // 100%
            this->patterns[1].PWM[1] = 0xBF; // 75%
            this->patterns[1].PWM[2] = 0x00; // 0%
            this->patterns[1].PWM[3] = 0xBF; // 75%
            this->patterns[1].PWM[4] = 0xFF; // 100%
            this->patterns[1].sloper0 = 0x01; // 50ms
            this->patterns[1].sloper1 = 0x01; // 50ms
            this->patterns[1].sloper2 = 0x01; // 50ms
            this->patterns[1].sloper3 = 0x01; // 50ms --> total pattern: 200ms
            this->set_engine_pattern(1, 0, 1); //Set Engine1 to Pattern1
            this->disable_engine_pattern(1, 1);
            this->disable_engine_pattern(1, 2);
            this->disable_engine_pattern(1, 3); //Disable all other patterns
            this->config.out2_engine_ch = 1; //Set Blue to Engine1

            break;
        case LED_ANIMATION_SENSOR_INIT_FAILED_IMU:
            // Handle LED_ANIMATION_SENSOR_INIT_FAILED_IMU
            // 2 quick red blinks (4Hz), followed by one quick red blink (after 1s)
            this->config.out0_en = 1;
            this->config.out1_en = 0;
            this->config.out2_en = 0;

            // two quick blinks
            this->patterns[0].pause_t0 = 0; //No Pause
            this->patterns[0].pause_t1 = 0x0; //No Pause
            this->patterns[0].repeat = 0x2; //Repeat once
            this->patterns[0].PWM[0] = 0x00;
            this->patterns[0].PWM[1] = 0xFF; // 100%
            this->patterns[0].PWM[2] = 0xFF; // 100%
            this->patterns[0].PWM[3] = 0x00;
            this->patterns[0].PWM[4] = 0x00;
            this->patterns[0].sloper0 = 0x01; // 50ms
            this->patterns[0].sloper1 = 0x01; // 50ms
            this->patterns[0].sloper2 = 0x01; // 50ms
            this->patterns[0].sloper3 = 0x02; // 100ms --> total pattern: 2 * 250ms
            this->set_engine_pattern(0, 0, 0); //Set Engine0 to Pattern0

            // one blink after 1s
            this->patterns[1].pause_t0 = 0xA; // 500ms
            this->patterns[1].pause_t1 = 0x9; // 450ms
            this->patterns[1].repeat = 0x1; // No repeat
            this->patterns[1].PWM[0] = 0x00;
            this->patterns[1].PWM[1] = 0xFF; // 100%
            this->patterns[1].PWM[2] = 0xFF; // 100%
            this->patterns[1].PWM[3] = 0x00;
            this->patterns[1].PWM[4] = 0x00;
            this->patterns[1].sloper0 = 0x01; // 50ms
            this->patterns[1].sloper1 = 0x01; // 50ms
            this->patterns[1].sloper2 = 0x01; // 50ms
            this->patterns[1].sloper3 = 0x08; // 400ms --> total pattern: 500ms + 150ms + 400ms + 450ms = 1500ms
            this->set_engine_pattern(0, 1, 1);

            this->disable_engine_pattern(0, 2);
            this->disable_engine_pattern(0, 3); //Disable all other patterns
            this->engines[0].repetitions = 3; //Set Engine0 to repeat infinitely
            this->config.out0_engine_ch = 0; //Set Red to Engine0

            break;
        case LED_ANIMATION_SENSOR_INIT_FAILED_LDC:
            // Handle LED_ANIMATION_SENSOR_INIT_FAILED_LDC
            // 2 quick red blinks (4Hz), followed by another two quick red blinks (after 1s) --> 1s period
            this->config.out0_en = 1;
            this->config.out1_en = 0;
            this->config.out2_en = 0;

            // two quick blinks with 1s period
            this->patterns[0].pause_t0 = 0x05; // 250ms
            this->patterns[0].pause_t1 = 0x05; // 250ms
            this->patterns[0].repeat = 0x2; // Repeat once
            this->patterns[0].PWM[0] = 0x00;
            this->patterns[0].PWM[1] = 0xFF; // 100%
            this->patterns[0].PWM[2] = 0xFF; // 100%
            this->patterns[0].PWM[3] = 0x00;
            this->patterns[0].PWM[4] = 0x00;
            this->patterns[0].sloper0 = 0x01; // 50ms
            this->patterns[0].sloper1 = 0x01; // 50ms
            this->patterns[0].sloper2 = 0x01; // 50ms
            this->patterns[0].sloper3 = 0x02; // 100ms --> total pattern: 2 * 250ms + 500ms = 1s
            this->set_engine_pattern(0, 0, 0); //Set Engine0 to Pattern0

            this->disable_engine_pattern(0, 1);
            this->disable_engine_pattern(0, 2);
            this->disable_engine_pattern(0, 3); //Disable all other patterns
            this->engines[0].repetitions = 3; //Set Engine0 to repeat infinitely
            this->config.out0_engine_ch = 0; //Set Red to Engine0

            break;
        case LED_ANIMATION_SENSOR_INIT_FAILED_TOF:
            // Handle LED_ANIMATION_SENSOR_INIT_FAILED_TOF
            // 2 quick red blinks (4Hz), followed by another three quick red blinks (after 1s) --> 1s period
            this->config.out0_en = 1;
            this->config.out1_en = 0;
            this->config.out2_en = 0;

            // two quick blinks with 1s period
            this->patterns[0].pause_t0 = 0x00; // No Pause
            this->patterns[0].pause_t1 = 0x0A; // 500ms
            this->patterns[0].repeat = 0x2; // Repeat once
            this->patterns[0].PWM[0] = 0x00;
            this->patterns[0].PWM[1] = 0xFF; // 100%
            this->patterns[0].PWM[2] = 0xFF; // 100%
            this->patterns[0].PWM[3] = 0x00;
            this->patterns[0].PWM[4] = 0x00;
            this->patterns[0].sloper0 = 0x01; // 50ms
            this->patterns[0].sloper1 = 0x01; // 50ms
            this->patterns[0].sloper2 = 0x01; // 50ms
            this->patterns[0].sloper3 = 0x02; // 100ms --> total pattern: 2 * 250ms + 500ms = 1s
            this->set_engine_pattern(0, 0, 0); //Set Engine0 to Pattern0

            // three quick blinks
            this->patterns[1].pause_t0 = 0x00; // No Pause
            this->patterns[1].pause_t1 = 0x05; // 250ms
            this->patterns[1].repeat = 0x03; // No Repeat
            this->patterns[1].PWM[0] = 0x00;
            this->patterns[1].PWM[1] = 0xFF; // 100%
            this->patterns[1].PWM[2] = 0xFF; // 100%
            this->patterns[1].PWM[3] = 0x00;
            this->patterns[1].PWM[4] = 0x00;
            this->patterns[1].sloper0 = 0x01; // 50ms
            this->patterns[1].sloper1 = 0x01; // 50ms
            this->patterns[1].sloper2 = 0x01; // 50ms
            this->patterns[1].sloper3 = 0x02; // 100ms --> total pattern: 3 * 250ms + 250ms = 1s
            this->set_engine_pattern(0, 1, 1); //Set Engine0 to Pattern1

            this->disable_engine_pattern(0, 2);
            this->disable_engine_pattern(0, 3); //Disable all other patterns
            this->engines[0].repetitions = 3; //Set Engine0 to repeat infinitely
            this->config.out0_engine_ch = 0; //Set Red to Engine0
            break;
        case LED_ANIMATION_SENSOR_INIT_FAILED_MAG:
            // Handle LED_ANIMATION_SENSOR_INIT_FAILED_MAG
            // 2 quick red blinks (4Hz), followed by one long blink
            this->config.out0_en = 1;
            this->config.out1_en = 0;
            this->config.out2_en = 0;

            // two quick blinks with 1s period
            this->patterns[0].pause_t0 = 0x00; // No Pause
            this->patterns[0].pause_t1 = 0x0A; // 500ms
            this->patterns[0].repeat = 0x2; // Repeat once
            this->patterns[0].PWM[0] = 0x00;
            this->patterns[0].PWM[1] = 0xFF; // 100%
            this->patterns[0].PWM[2] = 0xFF; // 100%
            this->patterns[0].PWM[3] = 0x00;
            this->patterns[0].PWM[4] = 0x00;
            this->patterns[0].sloper0 = 0x01; // 50ms
            this->patterns[0].sloper1 = 0x01; // 50ms
            this->patterns[0].sloper2 = 0x01; // 50ms
            this->patterns[0].sloper3 = 0x02; // 100ms --> total pattern: 2 * 250ms + 500ms = 1s
            this->set_engine_pattern(0, 0, 0); //Set Engine0 to Pattern0

            // one long blink
            this->patterns[1].pause_t0 = 0x00; // No Pause
            this->patterns[1].pause_t1 = 0x00; // 250ms
            this->patterns[1].repeat = 0x01; // No Repeat
            this->patterns[1].PWM[0] = 0x00;
            this->patterns[1].PWM[1] = 0xFF; // 100%
            this->patterns[1].PWM[2] = 0xFF; // 100%
            this->patterns[1].PWM[3] = 0xFF; // 100%
            this->patterns[1].PWM[4] = 0x00;
            this->patterns[1].sloper0 = 0x02; // 100ms
            this->patterns[1].sloper1 = 0x08; // 400ms
            this->patterns[1].sloper2 = 0x08; // 400ms
            this->patterns[1].sloper3 = 0x02; // 100ms --> total pattern: 1s
            this->set_engine_pattern(0, 1, 1); //Set Engine0 to Pattern1

            this->disable_engine_pattern(0, 2);
            this->disable_engine_pattern(0, 3); //Disable all other patterns
            this->engines[0].repetitions = 3; //Set Engine0 to repeat infinitely
            this->config.out0_engine_ch = 0; //Set Red to Engine0
            break;
        case LED_ANIMATION_APPLICATION_RUNNING:
            // Handle LED_ANIMATION_APPLICATION_RUNNING
            // Solid Green
            this->config.out0_en = 0;
            this->config.out1_en = 1;
            this->config.out2_en = 0;

            this->patterns[0].pause_t0 = 0; // No Pause
            this->patterns[0].pause_t1 = 0; // No Pause
            this->patterns[0].repeat = 0xF; // Infinite
            this->patterns[0].PWM[0] = 0xFF; // 100%
            this->patterns[0].PWM[1] = 0xFF; // 100%
            this->patterns[0].PWM[2] = 0xFF; // 100%
            this->patterns[0].PWM[3] = 0xFF; // 100%
            this->patterns[0].PWM[4] = 0xFF; // 100%
            this->patterns[0].sloper0 = 0x01; // 50ms
            this->patterns[0].sloper1 = 0x01; // 50ms
            this->patterns[0].sloper2 = 0x01; // 50ms
            this->patterns[0].sloper3 = 0x01; // 50ms --> total pattern: 4 * 50ms = 200ms
            this->set_engine_pattern(0, 0, 0); //Set Engine0 to Pattern0

            this->disable_engine_pattern(0, 1);
            this->disable_engine_pattern(0, 2);
            this->disable_engine_pattern(0, 3);
            this->engines[0].repetitions = 0x3; //Set Engine0 to repeat infinitely
            this->config.out1_engine_ch = 0; //Set Green to Engine0
            break;
        case LED_ANIMATION_CHARGER_NOT_RESPONDING:
        case LED_ANIMATION_UNCAUGHT_EXCEPTION:
            // Handle LED_ANIMATION_UNCAUGHT_EXCEPTION
            // Fast Red Blinking (5Hz)
            this->config.out0_en = 1;
            this->config.out1_en = 0;
            this->config.out2_en = 0;

            this->patterns[0].pause_t0 = 0; // No Pause
            this->patterns[0].pause_t1 = 0; // No Pause
            this->patterns[0].repeat = 0xF; // Infinite
            this->patterns[0].PWM[0] = 0x00;
            this->patterns[0].PWM[1] = 0xFF; // 100%
            this->patterns[0].PWM[2] = 0xFF; // 100%
            this->patterns[0].PWM[3] = 0x00;
            this->patterns[0].PWM[4] = 0x00;
            this->patterns[0].sloper0 = 0x01; // 50ms
            this->patterns[0].sloper1 = 0x01; // 50ms
            this->patterns[0].sloper2 = 0x01; // 50ms
            this->patterns[0].sloper3 = 0x01; // 50ms --> total pattern: 4 * 50ms = 200ms
            this->set_engine_pattern(0, 0, 0); //Set Engine0 to Pattern0

            this->disable_engine_pattern(0, 1);
            this->disable_engine_pattern(0, 2);
            this->disable_engine_pattern(0, 3);
            this->engines[0].repetitions = 0x3; //Set Engine0 to repeat infinitely
            this->config.out0_engine_ch = 0; //Set Red to Engine0
            break;
        case LED_ANIMATION_CYAN:
            // Handle LED_ANIMATION_APPLICATION_RUNNING
            // Solid Green, Solid Blue
            this->config.out0_en = 0;
            this->config.out1_en = 1;
            this->config.out2_en = 1;

            this->patterns[0].pause_t0 = 0; // No Pause
            this->patterns[0].pause_t1 = 0; // No Pause
            this->patterns[0].repeat = 0xF; // Infinite
            this->patterns[0].PWM[0] = 0xFF; // 100%
            this->patterns[0].PWM[1] = 0xFF; // 100%
            this->patterns[0].PWM[2] = 0xFF; // 100%
            this->patterns[0].PWM[3] = 0xFF; // 100%
            this->patterns[0].PWM[4] = 0xFF; // 100%
            this->patterns[0].sloper0 = 0x01; // 50ms
            this->patterns[0].sloper1 = 0x01; // 50ms
            this->patterns[0].sloper2 = 0x01; // 50ms
            this->patterns[0].sloper3 = 0x01; // 50ms --> total pattern: 4 * 50ms = 200ms
            this->set_engine_pattern(0, 0, 0); //Set Engine0 to Pattern0

            this->disable_engine_pattern(0, 1);
            this->disable_engine_pattern(0, 2);
            this->disable_engine_pattern(0, 3);
            this->engines[0].repetitions = 0x3; //Set Engine0 to repeat infinitely
            this->config.out1_engine_ch = 0; //Set Green to Engine0
            this->config.out2_engine_ch = 0; //Set Blue to Engine0
            break;
    }
    this->update_config();
}




/**
 * @brief Set the pattern for a specific engine and engine pattern index.
 * @param engine_index The index of the engine (0-3).
 * @param engine_pattern_index The index of the pattern inside the engine, that needs to be changed (0-3).
 * @param pattern_index The index of the pattern to set (0-3).
 */
void LEDController::set_engine_pattern(uint8_t engine_index, uint8_t engine_pattern_index, uint8_t pattern_index)
{
    if(engine_index < 4 && engine_pattern_index < 4 && pattern_index < 4)
    {
        this->engines[engine_index].patterns[engine_pattern_index] = &this->patterns[pattern_index];
    }
}


/**
 * @brief Disable the pattern for a specific engine and engine pattern index.
 * @param engine_index The index of the engine (0-3).
 * @param engine_pattern_index The index of the pattern inside the engine (0-3).
 */
void LEDController::disable_engine_pattern(uint8_t engine_index, uint8_t pattern_index)
{
    if(engine_index < 4 && pattern_index < 4)
    {
        this->engines[engine_index].patterns[pattern_index] = nullptr;
    }
}


/**
 * @brief Write the updated Device, Engine and Pattern Configuration to the LED driver.
 */
void LEDController::update_config()
{
    // Stop Engines
    this->stop();


    // Write Device & Engine Configuration

    struct
    {
        LED_Controller_Config_t config;
        LED_Controller_Engine_Config_t engine_config;
    }config_frame;

    // Device Configuration
    config_frame.config = this->config;

    //Engine0 Configuration
    {
        config_frame.engine_config.engine0_rept = this->engines[0].repetitions;

        // Pattern0 Configuration
        config_frame.engine_config.e0o0_en = (this->engines[0].patterns[0] != nullptr);
        if (this->engines[0].patterns[0] == this->patterns)        config_frame.engine_config.engine0_order0 = 0;
        else if (this->engines[0].patterns[0] == this->patterns + 1)    config_frame.engine_config.engine0_order0 = 1;
        else if (this->engines[0].patterns[0] == this->patterns + 2)    config_frame.engine_config.engine0_order0 = 2;
        else if (this->engines[0].patterns[0] == this->patterns + 3)    config_frame.engine_config.engine0_order0 = 3;

        // Pattern1 Configuration
        config_frame.engine_config.e0o1_en = (this->engines[0].patterns[1] != nullptr);
        if (this->engines[0].patterns[1] == this->patterns)        config_frame.engine_config.engine0_order1 = 0;
        else if (this->engines[0].patterns[1] == this->patterns + 1)    config_frame.engine_config.engine0_order1 = 1;
        else if (this->engines[0].patterns[1] == this->patterns + 2)    config_frame.engine_config.engine0_order1 = 2;
        else if (this->engines[0].patterns[1] == this->patterns + 3)    config_frame.engine_config.engine0_order1 = 3;

        // Pattern2 Configuration
        config_frame.engine_config.e0o2_en = (this->engines[0].patterns[2] != nullptr);
        if (this->engines[0].patterns[2] == this->patterns)        config_frame.engine_config.engine0_order2 = 0;
        else if (this->engines[0].patterns[2] == this->patterns + 1)    config_frame.engine_config.engine0_order2 = 1;
        else if (this->engines[0].patterns[2] == this->patterns + 2)    config_frame.engine_config.engine0_order2 = 2;
        else if (this->engines[0].patterns[2] == this->patterns + 3)    config_frame.engine_config.engine0_order2 = 3;

        // Pattern3 Configuration
        config_frame.engine_config.e0o3_en = (this->engines[0].patterns[3] != nullptr);
        if (this->engines[0].patterns[3] == this->patterns)        config_frame.engine_config.engine0_order3 = 0;
        else if (this->engines[0].patterns[3] == this->patterns + 1)    config_frame.engine_config.engine0_order3 = 1;
        else if (this->engines[0].patterns[3] == this->patterns + 2)    config_frame.engine_config.engine0_order3 = 2;
        else if (this->engines[0].patterns[3] == this->patterns + 3)    config_frame.engine_config.engine0_order3 = 3;
    }



    // Engine1 Configuration
    {
        config_frame.engine_config.engine1_rept = this->engines[1].repetitions;

        // Pattern0 Configuration
        config_frame.engine_config.e1o0_en = (this->engines[1].patterns[0] != nullptr);
        if (this->engines[1].patterns[0] == this->patterns)        config_frame.engine_config.engine1_order0 = 0;
        else if (this->engines[1].patterns[0] == this->patterns + 1)    config_frame.engine_config.engine1_order0 = 1;
        else if (this->engines[1].patterns[0] == this->patterns + 2)    config_frame.engine_config.engine1_order0 = 2;
        else if (this->engines[1].patterns[0] == this->patterns + 3)    config_frame.engine_config.engine1_order0 = 3;

        // Pattern1 Configuration
        config_frame.engine_config.e1o1_en = (this->engines[1].patterns[1] != nullptr);
        if (this->engines[1].patterns[1] == this->patterns)        config_frame.engine_config.engine1_order1 = 0;
        else if (this->engines[1].patterns[1] == this->patterns + 1)    config_frame.engine_config.engine1_order1 = 1;
        else if (this->engines[1].patterns[1] == this->patterns + 2)    config_frame.engine_config.engine1_order1 = 2;
        else if (this->engines[1].patterns[1] == this->patterns + 3)    config_frame.engine_config.engine1_order1 = 3;

        // Pattern2 Configuration
        config_frame.engine_config.e1o2_en = (this->engines[1].patterns[2] != nullptr);
        if (this->engines[1].patterns[2] == this->patterns)        config_frame.engine_config.engine1_order2 = 0;
        else if (this->engines[1].patterns[2] == this->patterns + 1)    config_frame.engine_config.engine1_order2 = 1;
        else if (this->engines[1].patterns[2] == this->patterns + 2)    config_frame.engine_config.engine1_order2 = 2;
        else if (this->engines[1].patterns[2] == this->patterns + 3)    config_frame.engine_config.engine1_order2 = 3;

        // Pattern3 Configuration
        config_frame.engine_config.e1o3_en = (this->engines[1].patterns[3] != nullptr);
        if (this->engines[1].patterns[3] == this->patterns)        config_frame.engine_config.engine1_order3 = 0;
        else if (this->engines[1].patterns[3] == this->patterns + 1)    config_frame.engine_config.engine1_order3 = 1;
        else if (this->engines[1].patterns[3] == this->patterns + 2)    config_frame.engine_config.engine1_order3 = 2;
        else if (this->engines[1].patterns[3] == this->patterns + 3)    config_frame.engine_config.engine1_order3 = 3;
    }



    // Engine2 Configuration
    {
        config_frame.engine_config.engine2_rept = this->engines[2].repetitions;

        // Pattern0 Configuration
        config_frame.engine_config.e2o0_en = (this->engines[2].patterns[0] != nullptr);
        if (this->engines[2].patterns[0] == this->patterns)        config_frame.engine_config.engine2_order0 = 0;
        else if (this->engines[2].patterns[0] == this->patterns + 1)    config_frame.engine_config.engine2_order0 = 1;
        else if (this->engines[2].patterns[0] == this->patterns + 2)    config_frame.engine_config.engine2_order0 = 2;
        else if (this->engines[2].patterns[0] == this->patterns + 3)    config_frame.engine_config.engine2_order0 = 3;

        // Pattern1 Configuration
        config_frame.engine_config.e2o1_en = (this->engines[2].patterns[1] != nullptr);
        if (this->engines[2].patterns[1] == this->patterns)        config_frame.engine_config.engine2_order1 = 0;
        else if (this->engines[2].patterns[1] == this->patterns + 1)    config_frame.engine_config.engine2_order1 = 1;
        else if (this->engines[2].patterns[1] == this->patterns + 2)    config_frame.engine_config.engine2_order1 = 2;
        else if (this->engines[2].patterns[1] == this->patterns + 3)    config_frame.engine_config.engine2_order1 = 3;

        // Pattern2 Configuration
        config_frame.engine_config.e2o2_en = (this->engines[2].patterns[2] != nullptr);
        if (this->engines[2].patterns[2] == this->patterns)        config_frame.engine_config.engine2_order2 = 0;
        else if (this->engines[2].patterns[2] == this->patterns + 1)    config_frame.engine_config.engine2_order2 = 1;
        else if (this->engines[2].patterns[2] == this->patterns + 2)    config_frame.engine_config.engine2_order2 = 2;
        else if (this->engines[2].patterns[2] == this->patterns + 3)    config_frame.engine_config.engine2_order2 = 3;

        // Pattern3 Configuration
        config_frame.engine_config.e2o3_en = (this->engines[2].patterns[3] != nullptr);
        if (this->engines[2].patterns[3] == this->patterns)        config_frame.engine_config.engine2_order3 = 0;
        else if (this->engines[2].patterns[3] == this->patterns + 1)    config_frame.engine_config.engine2_order3 = 1;
        else if (this->engines[2].patterns[3] == this->patterns + 2)    config_frame.engine_config.engine2_order3 = 2;
        else if (this->engines[2].patterns[3] == this->patterns + 3)    config_frame.engine_config.engine2_order3 = 3;
    }



    // Engine3 Configuration
    {
        config_frame.engine_config.engine3_rept = this->engines[3].repetitions;
    
        // Pattern0 Configuration
        config_frame.engine_config.e3o0_en = (this->engines[3].patterns[0] != nullptr);
        if (this->engines[3].patterns[0] == this->patterns)        config_frame.engine_config.engine3_order0 = 0;
        else if (this->engines[3].patterns[0] == this->patterns + 1)    config_frame.engine_config.engine3_order0 = 1;
        else if (this->engines[3].patterns[0] == this->patterns + 2)    config_frame.engine_config.engine3_order0 = 2;
        else if (this->engines[3].patterns[0] == this->patterns + 3)    config_frame.engine_config.engine3_order0 = 3;
    
        // Pattern1 Configuration
        config_frame.engine_config.e3o1_en = (this->engines[3].patterns[1] != nullptr);
        if (this->engines[3].patterns[1] == this->patterns)        config_frame.engine_config.engine3_order1 = 0;
        else if (this->engines[3].patterns[1] == this->patterns + 1)    config_frame.engine_config.engine3_order1 = 1;
        else if (this->engines[3].patterns[1] == this->patterns + 2)    config_frame.engine_config.engine3_order1 = 2;
        else if (this->engines[3].patterns[1] == this->patterns + 3)    config_frame.engine_config.engine3_order1 = 3;
    
        // Pattern2 Configuration
        config_frame.engine_config.e3o2_en = (this->engines[3].patterns[2] != nullptr);
        if (this->engines[3].patterns[2] == this->patterns)        config_frame.engine_config.engine3_order2 = 0;
        else if (this->engines[3].patterns[2] == this->patterns + 1)    config_frame.engine_config.engine3_order2 = 1;
        else if (this->engines[3].patterns[2] == this->patterns + 2)    config_frame.engine_config.engine3_order2 = 2;
        else if (this->engines[3].patterns[2] == this->patterns + 3)    config_frame.engine_config.engine3_order2 = 3;
    
        // Pattern3 Configuration
        config_frame.engine_config.e3o3_en = (this->engines[3].patterns[3] != nullptr);
        if (this->engines[3].patterns[3] == this->patterns)        config_frame.engine_config.engine3_order3 = 0;
        else if (this->engines[3].patterns[3] == this->patterns + 1)    config_frame.engine_config.engine3_order3 = 1;
        else if (this->engines[3].patterns[3] == this->patterns + 2)    config_frame.engine_config.engine3_order3 = 2;
        else if (this->engines[3].patterns[3] == this->patterns + 3)    config_frame.engine_config.engine3_order3 = 3;

    }


    App_I2cTransfer(LED_I2C_HANDLE, LED_I2C_ADDRESS << 1, 0x00, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&config_frame, nullptr, sizeof(config_frame), 10, 0);


    // Write Pattern Configuration
    App_I2cTransfer(LED_I2C_HANDLE, LED_I2C_ADDRESS << 1, LED_PATTERN_REG_START, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&this->patterns, nullptr, sizeof(this->patterns), 10, 0);

    uint8_t update = 0x55;
    App_I2cTransfer(LED_I2C_HANDLE, LED_I2C_ADDRESS << 1, LED_UPDATE_CMD, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&update, nullptr, sizeof(update), 10, 0);

    // Start Engines
    this->start();
}


/**
 * @brief Start the Engines.
 */
void LEDController::start()
{
    uint8_t txdata = 0xFF;
    App_I2cTransfer(LED_I2C_HANDLE, LED_I2C_ADDRESS << 1, LED_START_CMD, I2C_MEMADD_SIZE_8BIT, &txdata, nullptr, 1, 10, 0);
}

/**
 * @brief Stop the Engines.
 */
void LEDController::stop()
{
    uint8_t txdata = 0xAA;
    App_I2cTransfer(LED_I2C_HANDLE, LED_I2C_ADDRESS << 1, LED_STOP_CMD, I2C_MEMADD_SIZE_8BIT, &txdata, nullptr, 1, 10, 0);
}
