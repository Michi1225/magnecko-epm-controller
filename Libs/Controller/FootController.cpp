#include "FootController.h"
#include "FSMTypes.hpp"
#include "LEDController.h"
#include "main.h"
#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_tim.h"
#include "thermistor.h"


FootController::FootController() : fsm_(),
                                   fsmActions_(),
                                   imu(),
                                   hall0(TMAG5273::A1),
                                   hall1(TMAG5273::B1),
                                   hall2(TMAG5273::C1),
                                   hall3(TMAG5273::D1),
                                   tof(),
                                   charger()
{
    this->controller_error_word = {
        .imu_init_failed = 0,
        .ldc_init_failed = 0,
        .hall_init_failed = 0,
        .tof_init_failed = 0,
        .charger_init_failed = 0,
        .charger_oc_fault = 0,
        .charger_ov_fault = 0,
        .charger_wd_fault = 0,
        .eeprom_params_invalid = 0,
        .timer_init_failed = 0,
        .temperature_sensors_not_connected = 0,
        .over_temperature_fault = 0,
        .gate_drive_fault = 0,
        .invalid_input_command = 0,
    };

    this->ldc[0] = LDC1101(LDC0_NCS_GPIO_Port, LDC0_NCS_Pin);
    this->ldc[1] = LDC1101(LDC1_NCS_GPIO_Port, LDC1_NCS_Pin);
    this->ldc[2] = LDC1101(LDC2_NCS_GPIO_Port, LDC2_NCS_Pin);
    this->ldc[3] = LDC1101(LDC3_NCS_GPIO_Port, LDC3_NCS_Pin);
    
    
    this->ldc[0].next = &this->ldc[1];
    this->ldc[1].next = &this->ldc[2];
    this->ldc[2].next = &this->ldc[3];
    this->ldc[3].next = &this->ldc[0];

    this->requested_magnetization = false;
    this->requested_demagnetization = false;
    this->prev_demag = false;
    this->prev_mag = false;

}

void FootController::init()
{
    //LED Controller Initialization
    this->ledController.init();
    this->ledController.set_animation(LED_ANIMATION_CONFIGURING);
    uint8_t errorcode = 0;

    uint8_t tim_error = 0;
    tim_error |= HAL_TIM_OnePulse_Start(TIM_DRV1, CHANNEL_DRV1); //Start One Pulse for DRV1
    tim_error |= HAL_TIM_OnePulse_Start(TIM_DRV2, CHANNEL_DRV2); //Start One Pulse for DRV2


    HAL_Delay(100);

    HAL_GPIO_WritePin(GD_nEN_GPIO_Port, GD_nEN_Pin, GPIO_PIN_RESET); //Enable Gate Drivers

    HAL_Delay(100);

    Obj.Magnet_Command = 0;

    //FSM initialization
    this->fsmActions_.background_ = std::bind(&FootController::FSM_bg, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    this->fsmActions_.notReadyToSwitchOn_ = std::bind(&FootController::FSM_notReadyToSwitchOn, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    this->fsmActions_.switchOnDisabled_ = std::bind(&FootController::FSM_switchOnDisabled, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    this->fsmActions_.readyToSwitchOn_ = std::bind(&FootController::FSM_readyToSwitchOn, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    this->fsmActions_.switchedOn_ = std::bind(&FootController::FSM_switchedOn, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    this->fsmActions_.operationEnabled_ = std::bind(&FootController::FSM_operationEnabled, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    this->fsmActions_.quickStopActive_ = std::bind(&FootController::FSM_quickStopActive, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    this->fsmActions_.faultReactionActive_ = std::bind(&FootController::FSM_faultReactionActive, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    this->fsmActions_.fault_ = std::bind(&FootController::FSM_fault, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

    fsm_.init(this->fsmActions_);

    // Read force estimation parameters from EEPROM
    #ifdef EEPROM_WRITE
    this->write_force_estimation_params_to_eeprom();
    #endif
    this->read_force_estimation_params_from_eeprom();

    
    //ECAT initialization
    HAL_GPIO_WritePin(nRST_ECAT_GPIO_Port, nRST_ECAT_Pin, GPIO_PIN_RESET);
    HAL_Delay(0);
    HAL_GPIO_WritePin(nRST_ECAT_GPIO_Port, nRST_ECAT_Pin, GPIO_PIN_SET);
    while(!HAL_GPIO_ReadPin(EEPROM_LOADED_GPIO_Port, EEPROM_LOADED_Pin)){} //Wait for EEPROM to be loaded
    ecat_slv_init(&this->config);

    // Temporarily store the git version in the Force_Estimate_Params array for debugging purposes
    const char* version = GIT_VERSION;
    memcpy(Obj.Force_Estimate_Params, version, strlen(version) + 1);
    Obj.EPM_Number = EPM_NUMBER; // EPM number, needed for hw interface




    //Sensor initialization 
    if(imu.init(1000) != 0) 
    {
        this->controller_error_word.imu_init_failed = 1;
    }
    
    for(LDC1101 &ldc : this->ldc)
    {
        if(ldc.init() != HAL_OK) 
        {
            this->controller_error_word.ldc_init_failed = 1;
        }else
        {
            ldc.start_measurement();
        }
    }

    ts_init(); // Initialize temperature sensors

    this->active_ldc = &this->ldc[0]; // Pointer to the currently active LDC, used for SPI communication
    this->ldc[0].next = &this->ldc[1];
    this->ldc[1].next = &this->ldc[2];
    this->ldc[2].next = &this->ldc[3];
    this->ldc[3].next = &this->ldc[0]; // Circular linked list of LDCs for easy iteration



    if(TMAG5273::init() != 0) 
    {
        this->controller_error_word.hall_init_failed = 1;
    }

    if(tof.init(100) != 0) 
    {
        this->controller_error_word.tof_init_failed = 1;
    }



    HAL_Delay(10);
    
    if(imu.start(1000) != 0) 
    {
        this->controller_error_word.imu_init_failed = 1;
    }

    if(tof.start_ranging() != 0) 
    {
        this->controller_error_word.tof_init_failed = 1;
    }

#ifndef USE_BATTERIES
    // Charger Initialization
    if(!charger.wait_ready(100)) 
    {
        this->controller_error_word.charger_init_failed = 1;
    }
#endif



    // Drive Timer Initialization
    tim_error |= HAL_TIM_Base_Start_IT(TIM_CONTROL); //Start Control Timer
    tim_error |= HAL_TIM_Base_Start_IT(TIM_IMU); //Start BNO Timer

    DEAD_TIME_TIMER->Instance->ARR = DEAD_TIME; //Set Dead Time Duration

    if(tim_error != HAL_OK) this->controller_error_word.timer_init_failed = 1; //Set timer init failed flag if any of the timers failed to start


    HAL_GPIO_WritePin(DISCHARGE_GPIO_Port, DISCHARGE_Pin, GPIO_PIN_SET);

    this->ledController.set_animation(LED_ANIMATION_CYAN);
}

void FootController::runCommunication()
{
    ecat_slv();
    if(this->charger.initialized)
    {
        this->charger.transmit_receive(); //Run Charger Communication Loop
    }
}

void FootController::magnetize(uint16_t time)
{
    if(time < 1000 ||time > 10000 || this->dead_time_active || this->controller_error_word.over_temperature_fault) return; 


    if(this->requested_magnetization && !this->requested_demagnetization)
    {
        // Magnetization was requiested
        TIM_DRV2->Instance->CCR4 = 20000 - time; // pulse width in us
        //This is needed, since OPM only supports CH1 and CH2...
        TIM8->CCER |= TIM_CCER_CC4E;
        TIM8->BDTR |= TIM_BDTR_MOE;
        TIM_DRV2->Instance->CR1 |= TIM_CR1_CEN; // Start Timer
        this->status_magnetization = true;
        HAL_GPIO_WritePin(MAG_STAT_GPIO_Port, MAG_STAT_Pin, GPIO_PIN_SET); //Set Magnetization Status to 1
    }else if(!this->requested_magnetization && this->requested_demagnetization)
    {
        // Demagnetization was requested
        this->status_magnetization = false;
        TIM_DRV1->Instance->CCR1 = 20000 - time; // pulse width in us
        TIM_DRV1->Instance->CR1 |= TIM_CR1_CEN; // Start Timer
        HAL_GPIO_WritePin(MAG_STAT_GPIO_Port, MAG_STAT_Pin, GPIO_PIN_RESET); //Set Magnetization Status to 0
    } else 
    {
        return; //No valid request, do nothing
    }
    //Set Magnetization Status
    this->dead_time_active = true; //Set Dead Time Active
    DEAD_TIME_TIMER->Instance->CNT = 0; //Reset Dead Time Timer
    HAL_TIM_Base_Start_IT(DEAD_TIME_TIMER); //Start Dead Time Timer
}

void FootController::runControl()
{
    this->fsm_.run();
}

FSMStatus FootController::FSM_bg(FSMStatus state, uint16_t &status_word, int8_t &mode)
{
    // Controller Status
    fsm_.setControlWord(Obj.Control_Word);

    uint8_t warning_active = false;

    // Handle Faults and Warnings
    // Sensor Initialization Failures
    Obj.Error_Code = 0; // Reset Error Code
    if(this->controller_error_word.imu_init_failed)
    {
        Obj.Error_Code = static_cast<uint16_t>(ErrorCodes::SENSOR_INIT_FAILED_IMU);
        status_word |= FSMStatusWord::WARNING_STATUS;
        warning_active = true;
    }if(this->controller_error_word.ldc_init_failed)
    {
        Obj.Error_Code |= static_cast<uint16_t>(ErrorCodes::SENSOR_INIT_FAILED_LDC);
        status_word |= FSMStatusWord::WARNING_STATUS;
        warning_active = true;
    }if(this->controller_error_word.hall_init_failed)
    {
        Obj.Error_Code |= static_cast<uint16_t>(ErrorCodes::SENSOR_INIT_FAILED_HALL);
        status_word |= FSMStatusWord::WARNING_STATUS;
        warning_active = true;
    }if(this->controller_error_word.tof_init_failed)
    {
        Obj.Error_Code |= static_cast<uint16_t>(ErrorCodes::SENSOR_INIT_FAILED_TOF);
        status_word |= FSMStatusWord::WARNING_STATUS;
        warning_active = true;
    }

    // // EEPROM Parameters Invalid
    if(this->controller_error_word.eeprom_params_invalid)
    {
        Obj.Error_Code = static_cast<uint16_t>(ErrorCodes::EEPROM_PARAMS_INVALID);
        status_word |= FSMStatusWord::WARNING_STATUS;
        warning_active = true;
    }

    // Charger Faults
    if(this->controller_error_word.charger_init_failed)
    {
        Obj.Error_Code = static_cast<uint16_t>(ErrorCodes::CHARGER_NOT_RESPONDING);
        status_word |= FSMStatusWord::WARNING_STATUS;
        warning_active = true;
    }else
    {
        if(this->controller_error_word.charger_oc_fault)
        {
            Obj.Error_Code = static_cast<uint16_t>(ErrorCodes::CHARGER_OVER_CURRENT_FAULT);
            status_word |= FSMStatusWord::WARNING_STATUS;
            warning_active = true;
        }if(this->controller_error_word.charger_ov_fault)
        {
            Obj.Error_Code = static_cast<uint16_t>(ErrorCodes::CHARGER_OVER_VOLTAGE_FAULT);
            status_word |= FSMStatusWord::WARNING_STATUS;
            warning_active = true;
        }
        if(this->controller_error_word.charger_wd_fault)
        {
            Obj.Error_Code = static_cast<uint16_t>(ErrorCodes::CHARGER_WATCHDOG_FAULT);
            status_word |= FSMStatusWord::WARNING_STATUS;
            warning_active = true;
        }
    }

    // Invalid Input Command
    if(this->controller_error_word.invalid_input_command)
    {
        Obj.Error_Code = static_cast<uint16_t>(ErrorCodes::INVALID_INPUT_COMMAND);
        status_word |= FSMStatusWord::WARNING_STATUS;
        warning_active = true;
    }

    // Gate Drive Fault
    bool gate_drive_fault = (HAL_GPIO_ReadPin(GD_nFLT_GPIO_Port, GD_nFLT_Pin) == GPIO_PIN_RESET) &&
                            (HAL_GPIO_ReadPin(GD_nEN_GPIO_Port, GD_nEN_Pin) == GPIO_PIN_RESET);
    if(gate_drive_fault)
    {
        this->controller_error_word.gate_drive_fault = 1;
        Obj.Error_Code = static_cast<uint16_t>(ErrorCodes::GATE_DRIVE_FAULT);
        status_word |= FSMStatusWord::WARNING_STATUS;
        warning_active = true;
    }else
    {
        this->controller_error_word.gate_drive_fault = 0;
    }


    // Temperature Sensors
    Thermistor_TypeDef_t thermistor_type = PT1000; // Use the NTC10K_3977K thermistor for temperature measurement
    float temperature = get_temperature(thermistor_type);
    if((temperature >= 240.0f && (thermistor_type == PT1000)) || 
       (temperature <= -40.0f && (thermistor_type == NTC10K_3977K)))
    // Temperature Sensor not connected or out of range, trigger warning
    {
        this->controller_error_word.temperature_sensors_not_connected = 1;
        Obj.Error_Code = static_cast<uint16_t>(ErrorCodes::TEMPERATURE_SENSORS_NOT_CONNECTED);
        status_word |= FSMStatusWord::WARNING_STATUS;
        warning_active = true;

    }else if (temperature >= 80.0f) // Over temperature fault, trigger warning
    {
        this->controller_error_word.over_temperature_fault = 1;
        Obj.Error_Code = static_cast<uint16_t>(ErrorCodes::OVER_TEMPERATURE);
        status_word |= FSMStatusWord::FAULT_STATUS;
        this->fsm_.triggerFaultReaction(ErrorCodes::OVER_TEMPERATURE);
        warning_active = true;
    }
    else Obj.Temperature = 100 * temperature;

    // Timer Initialization Failure
    if(this->controller_error_word.timer_init_failed)
    {
        status_word |= FSMStatusWord::FAULT_STATUS;
        Obj.Error_Code = static_cast<uint16_t>(ErrorCodes::TIMER_INIT_FAILED);
        warning_active = true;
    }

    if(!warning_active) status_word &= ~FSMStatusWord::WARNING_STATUS; // Clear warning status if no warnings are active


    //Handle Sensors
    //IMU
    Obj.IMU_Data.Acc_X = this->imu.output_lin_accel[0];
    Obj.IMU_Data.Acc_Y = this->imu.output_lin_accel[1];
    Obj.IMU_Data.Acc_Z = this->imu.output_lin_accel[2];

    Obj.IMU_Data.Gyro_X = this->imu.output_gyro[0];
    Obj.IMU_Data.Gyro_Y = this->imu.output_gyro[1];
    Obj.IMU_Data.Gyro_Z = this->imu.output_gyro[2];

    Obj.IMU_Data.Quat_I = this->imu.output_quat[0];
    Obj.IMU_Data.Quat_J = this->imu.output_quat[1];
    Obj.IMU_Data.Quat_K = this->imu.output_quat[2];
    Obj.IMU_Data.Quat_R = this->imu.output_quat[3];


    //ToF
    for(int i = 0; i < 64; i++)
    {
        Obj.ToF_Distance[i] = this->tof.data_frame.pixel_data[i / 8][i % 8].distance;
        Obj.ToF_SNR[i] = this->tof.data_frame.pixel_data[i / 8][i % 8].snr;
    }



    // LDC
    for(int i = 0; i < 4; i++)
    {
        Obj.LDC_Frequency[i] = this->ldc[i].rx_data.l_data;
        Obj.LDC_RP[i] = this->ldc[i].rx_data.rp_data;
    }

    // Hall Sensors
    Obj.HALL_Mag_X[0] = this->hall0.raw_bx;
    Obj.HALL_Mag_Y[0] = this->hall0.raw_by;
    Obj.HALL_Mag_Z[0] = this->hall0.raw_bz;
    Obj.HALL_Mag_X[1] = this->hall1.raw_bx;
    Obj.HALL_Mag_Y[1] = this->hall1.raw_by;
    Obj.HALL_Mag_Z[1] = this->hall1.raw_bz;
    Obj.HALL_Mag_X[2] = this->hall2.raw_bx;
    Obj.HALL_Mag_Y[2] = this->hall2.raw_by;
    Obj.HALL_Mag_Z[2] = this->hall2.raw_bz;
    Obj.HALL_Mag_X[3] = this->hall3.raw_bx;
    Obj.HALL_Mag_Y[3] = this->hall3.raw_by;
    Obj.HALL_Mag_Z[3] = this->hall3.raw_bz;

    
    // Force Estimation
    float current_estimate = this->force_estimation();
    this->force_average.push_back(current_estimate);
    if(this->force_average.size() > FORCE_ESTIMATE_WINDOW_SIZE) this->force_average.pop_front();
    float mag_force_average_sum = 0.0f;
    for(auto &val : this->force_average)
    {
        mag_force_average_sum += val;
    }
    Obj.Force_Estimate = mag_force_average_sum / this->force_average.size();

    // Capacitor Voltage
    Obj.Capacitor_Voltage = this->charger.status.vout_10mV;


    Obj.Status_Word = fsm_.getStatusWord();
    Obj.Magnet_Status = this->status_magnetization;

    return state;
}

FSMStatus FootController::FSM_notReadyToSwitchOn(FSMStatus state, uint16_t &status_word, int8_t &mode)
{
    return state;
}

FSMStatus FootController::FSM_switchOnDisabled(FSMStatus state, uint16_t &status_word, int8_t &mode)
{
    HAL_GPIO_WritePin(DISCHARGE_GPIO_Port, DISCHARGE_Pin, GPIO_PIN_RESET); //Discharge Caps
    this->charger.tx_data.enable = 0; //Disable Charging
    if(state != FSMStatus::SWITCH_ON_DISABLED) this->ledController.set_animation(LED_ANIMATION_CYAN);
    status_word = status_word & ~FSMStatusWord::ROTOR_ALIGNING_STATUS;
    return state;
}

FSMStatus FootController::FSM_readyToSwitchOn(FSMStatus state, uint16_t &status_word, int8_t &mode)
{
    this->charger.tx_data.enable = 1; //Enable Charging
    HAL_GPIO_WritePin(DISCHARGE_GPIO_Port, DISCHARGE_Pin, GPIO_PIN_RESET); //Discharge Caps
    status_word = status_word & ~FSMStatusWord::ROTOR_ALIGNING_STATUS;
    //TODO: Set Red/Green LED to indicate Ready to Switch On
    return state;
}

FSMStatus FootController::FSM_switchedOn(FSMStatus state, uint16_t &status_word, int8_t &mode)
{
    status_word = status_word & ~FSMStatusWord::ROTOR_ALIGNING_STATUS;
    if(state != FSMStatus::SWITCHED_ON) this->ledController.set_animation(LED_ANIMATION_ALL);
    return state;
}

FSMStatus FootController::FSM_operationEnabled(FSMStatus state, uint16_t &status_word, int8_t &mode)
{
    if(state != FSMStatus::OPERATION_ENABLED) // Just once when entering the state
    {
       this->ledController.set_animation(LED_ANIMATION_APPLICATION_RUNNING);
       this->requested_demagnetization = false; //Reset requested demagnetization state
       this->requested_magnetization = false; //Reset requested magnetization state
    }


    HAL_GPIO_WritePin(DISCHARGE_GPIO_Port, DISCHARGE_Pin, GPIO_PIN_SET);
    //Magnetization state
    //Handle Magnetization/Demagnetization requests
    if(this->requested_magnetization && this->requested_demagnetization)
    {
        this->controller_error_word.invalid_input_command = 1; //Set error flag for invalid input command
        this->requested_magnetization = false; //Reset requested magnetization state
        this->requested_demagnetization = false; //Reset requested demagnetization state
    }
    //Either Mafgnetization or Demagnetization was requested
    else if(this->requested_magnetization != this->requested_demagnetization)
    {
        this->magnetize(MAGNETIZATION_TIME);
        this->requested_magnetization = false; //Reset requested magnetization state
        this->requested_demagnetization = false; //Reset requested demagnetization state
    }
    return state;
}

FSMStatus FootController::FSM_quickStopActive(FSMStatus state, uint16_t &status_word, int8_t &mode)
{
    return state;
}

FSMStatus FootController::FSM_faultReactionActive(FSMStatus state, uint16_t &status_word, int8_t &mode)
{
    HAL_GPIO_WritePin(DISCHARGE_GPIO_Port, DISCHARGE_Pin, GPIO_PIN_RESET); //Discharge Caps
    Error_Handler(); //Handle Fault
    return state;
}

FSMStatus FootController::FSM_fault(FSMStatus state, uint16_t &status_word, int8_t &mode)
{
    return state;
}

float FootController::force_estimation()
{
    // this->hall0.read_magnitude();
    // this->hall1.read_magnitude();
    // this->hall2.read_magnitude();
    // this->hall3.read_magnitude();

    return TMAG5273::force_estimation(
        this->hall0.b_mag,
        this->hall1.b_mag,
        this->hall2.b_mag,
        this->hall3.b_mag
    ) * this->status_magnetization;
}

void FootController::read_force_estimation_params_from_eeprom()
{
    // Read the force estimation parameters from the EEPROM
    uint8_t data[N_PARAMETERS * sizeof(float)];
    EEPROM_Read(0, 0, data, sizeof(data));

    // TODO: Validate parameters
    uint8_t is_valid = 1;
    if(is_valid)
    {
        std::memcpy(this->force_estimation_params, data, sizeof(this->force_estimation_params));
    }else this->controller_error_word.eeprom_params_invalid = 1; //Set error flag if parameters are invalid
}

void FootController::write_force_estimation_params_to_eeprom()
{
    // Write the force estimation parameters to the EEPROM
    #ifdef EEPROM_WRITE
    EEPROM_Write(0, 0, eeprom_img, sizeof(eeprom_img));
    #endif
}

