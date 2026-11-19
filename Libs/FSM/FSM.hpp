/**
  * RSL File header
  * @author Lennart Nachtigall
  * @author Original author (C code) prob. Vasilious Tsounis
  * @copyright RSL - 2020
  * Note original implementation that was taken as example for this was from Vasilious
  */
 #pragma once

 #include "FSMTypes.hpp"

 #include <cstdint>
 enum class ErrorCodes {
    NO_ERROR = 0x0000,
    SENSOR_DATA_STALE = 0xFF19,
    OVER_CURRENT = 0x2220,
    OVER_TEMPERATURE = 0x4300,
    SENSOR_INIT_FAILED_IMU = 0xFF01,
    SENSOR_INIT_FAILED_LDC = 0xFF02,
    SENSOR_INIT_FAILED_IMU_LDC = 0xFF03,
    SENSOR_INIT_FAILED_TOF = 0xFF04,
    SENSOR_INIT_FAILED_IMU_TOF = 0xFF05,
    SENSOR_INIT_FAILED_LDC_TOF = 0xFF06,
    SENSOR_INIT_FAILED_IMU_LDC_TOF = 0xFF07,
    SENSOR_INIT_FAILED_HALL = 0xFF08,
    SENSOR_INIT_FAILED_IMU_HALL = 0xFF09,
    SENSOR_INIT_FAILED_LDC_HALL = 0xFF0A,
    SENSOR_INIT_FAILED_IMU_LDC_HALL = 0xFF0B,
    SENSOR_INIT_FAILED_TOF_HALL = 0xFF0C,
    SENSOR_INIT_FAILED_IMU_TOF_HALL = 0xFF0D,
    SENSOR_INIT_FAILED_LDC_TOF_HALL = 0xFF0E,
    SENSOR_INIT_FAILED_IMU_LDC_TOF_HALL = 0xFF0F,
    TIMER_INIT_FAILED = 0xFF10,
    EEPROM_PARAMS_INVALID = 0xFF11,
    CHARGER_OVER_CURRENT_FAULT = 0xFF12,
    CHARGER_OVER_VOLTAGE_FAULT = 0xFF13,
    CHARGER_WATCHDOG_FAULT = 0xFF14,
    CHARGER_NOT_RESPONDING = 0xFF15,
    TEMPERATURE_SENSORS_NOT_CONNECTED = 0xFF16,
    GATE_DRIVE_FAULT = 0xFF17,
    INVALID_INPUT_COMMAND = 0xFF18,
};

class FSM
{
public:
    FSM();
    void setControlWord(uint16_t input);
    void setMode(int8_t input);
    void exitFaultReaction();
    void triggerFaultReaction(ErrorCodes error_code);
    void run();
    const FSMStatus& state(){return  state_;}
    void init(const FSMActions& actions);
    void setWarning(ErrorCodes error_code){error_code_ = static_cast<uint16_t>(error_code);}
    uint16_t getStatusWord(){return status_word_;}
    uint8_t getOperationModeDisplay(){return mode_;}
    uint16_t getErrorCode(){return error_code_;}
    uint16_t error_code_ = 0;
private:
    FSMActions actions_;

    FSMStatus state_ = FSMStatus::NOT_READY_TO_SWITCH_ON;
    uint16_t control_word_ = 0;
    uint16_t status_word_ = 0;
    int8_t mode_ = 0;

    using StateAction = void (FSM::*)();
    StateAction next_state_ = nullptr;
    /* State functions declarations */
    void notReadyToSwitchOn();

    void switchOnDisabled();

    void readyToSwitchOn();

    void switchedOn();

    void operationEnabled();

    void quickStopActive();

    void faultReactionActive();

    void fault();
};
 
 
 