#pragma once

#include "main.h"
#include "tim.h"
#include "BNO086.h"
#include "LEDController.h"
#include "LDC1101.h"
#include "TMAG5273.h"
#include "TMF8829.h"
#include "FSM.hpp"
#include "utils.h"
#include "Charger.h"
#include "eeprom.h"
#include <deque>
#include "version.h"
extern "C" {
    #include "ecat_slv.h"
    #include "utypes.h"
    #include "soes_pin_mapping_def.h"
    #include "thermistor.h"
 }

#define MAGNETIZATION_TIME 5000 //5ms
#define DEAD_TIME 10000 //100ms

#define ECAT_SPI_HANDLE &hspi4

#define DEAD_TIME_TIMER (&htim6)

#define N_PARAMETERS 10

#define FORCE_ESTIMATE_WINDOW_SIZE 20

#ifndef EPM_NUMBER
#define EPM_NUMBER 1 // EPM number, needed for hw interface
#endif

#define USE_BATTERIES


class FootController
{
private:

    //ECAT variables
    pin_mapping_typedef pin_mapping = {ECAT_SPI_HANDLE, ECAT_NCS_GPIO_Port, ECAT_NCS_Pin, EEPROM_LOADED_GPIO_Port, EEPROM_LOADED_Pin};
    esc_cfg_t config =
    {
       /* User input to stack */
       .user_arg = &pin_mapping, /* passed along to ESC_config and ESC_init */

       /* Mandatory input to stack */
       .use_interrupt = 0, /* flag telling the stack if the user application will use
                              interrupts, 0= Polling, 1 = Mixed Polling/Interrupt
                              and Interrupt */

       .watchdog_cnt = 100, /* non UNIT watchdog counter, for the application
                               developer to decide UNIT. This example set 100
                               cnt and by calling ecat_slv or
                               DIG_process(DIG_PROCESS_WD_FLAG) every 1ms,
                               it creates a watchdog running at ~100ms. */

       /* Optional input to stack for user application interaction with the stack
        * all functions given must be implemented in the application.
        */
       .set_defaults_hook = NULL, /* hook called after stack have loaded known
                                     default values, possible for application
                                     to load values not known in compile time
                                     or that need to be overwritten */
       .pre_state_change_hook = NULL, /* hook called before state transition */
       .post_state_change_hook = NULL, /* hook called after state transition */

       .application_hook = NULL, /* hook in application loop called when
                                    DIG_process(DIG_PROCESS_APP_HOOK_FLAG) */
       .safeoutput_override = NULL, /* user override of default safeoutput when stack
                                       stop outputs */

       .pre_object_download_hook = NULL, /* hook called before object download,
                                            if hook return != 0 the download will not
                                            take place */
       .post_object_download_hook = NULL, /* hook called after object download */

       .rxpdo_override = NULL, /* user override of default rxpdo */
       .txpdo_override = NULL, /* user override of default txpdo */

       /* Mandatory input to stack for SM and DC synchronous applications */
       .esc_hw_interrupt_enable = NULL, /* callback to function that enable IRQ
                                           based on the Event MASK */
       .esc_hw_interrupt_disable = NULL, /* callback to function that disable IRQ
                                            based on the Event MASK */

       /* Mandatory input for emulated eeprom */
       .esc_hw_eep_handler = NULL, /* callback to function that handle an emulated eeprom */

       /* Mandatory if Distributed Clocks get activated by the master */
       .esc_check_dc_handler = NULL /* Check DC synchronous settings, must be set if
                                      DC synchronisation activated, or state change
                                      PREOP->SAFEOP will fail.*/
    };

    float force_estimation_params[N_PARAMETERS];


    /**
     * @brief Read the force estimation parameters from the EEPROM.
     *        This function should be called during initialization to load the parameters
     *        used for force estimation.
     */
    void read_force_estimation_params_from_eeprom();

    /**
     * @brief Write the force estimation parameters to the EEPROM.
     *        This function can be used to update the parameters used for force estimation.
     */
    void write_force_estimation_params_to_eeprom();




public:

    //FSM variables
    FSM fsm_;
    FSMActions fsmActions_;

    struct
    {
        uint8_t imu_init_failed : 1; // IMU initialization failed
        uint8_t ldc_init_failed : 1; // LDC initialization failed
        uint8_t hall_init_failed : 1; // Hall sensor initialization failed
        uint8_t tof_init_failed : 1; // ToF initialization failed
        uint8_t charger_init_failed : 1; // Charger initialization failed
        uint8_t charger_oc_fault : 1; // Charger overcurrent fault
        uint8_t charger_ov_fault : 1; // Charger overvoltage fault
        uint8_t charger_wd_fault : 1; // Charger watchdog fault
        uint8_t eeprom_params_invalid : 1; // EEPROM parameters are invalid
        uint8_t timer_init_failed : 1; // Timer initialization failed
        uint8_t temperature_sensors_not_connected : 1; // Temperature sensors not connected
        uint8_t over_temperature_fault : 1; // Over temperature fault
        uint8_t gate_drive_fault : 1; // Gate drive fault
        uint8_t invalid_input_command : 1; // Invalid input command received via EtherCAT
    }controller_error_word;

    //LED Controller
    LEDController ledController;

    
    //Sensor variables
    //IMU
    BNO086 imu;
    //LDC
    LDC1101 ldc[4] =
    {
        LDC1101(LDC0_NCS_GPIO_Port, LDC0_NCS_Pin),
        LDC1101(LDC1_NCS_GPIO_Port, LDC1_NCS_Pin),
        LDC1101(LDC2_NCS_GPIO_Port, LDC2_NCS_Pin),
        LDC1101(LDC3_NCS_GPIO_Port, LDC3_NCS_Pin)
    };

    LDC1101 *active_ldc; // Pointer to the currently active LDC, used for SPI communication

    //Hall Sensors
    TMAG5273 hall0 = TMAG5273(TMAG5273::A1);
    TMAG5273 hall1 = TMAG5273(TMAG5273::B1);
    TMAG5273 hall2 = TMAG5273(TMAG5273::C1);
    TMAG5273 hall3 = TMAG5273(TMAG5273::D1);
    uint8_t contact_estimation = 0;
    std::deque<float> force_average;


    //TOF
    TMF8829 tof;

    //Controller variables
    bool requested_magnetization = false;   //Requested magnetization state
    bool requested_demagnetization = false; //Requested demagnetization state
    bool prev_mag = false;
    bool prev_demag = false;
    bool status_magnetization = false;      //Status of the magnetization
    bool requested_discharge = false;       //A discharge is requested
    bool dead_time_active = false;          //Dead time is active, no magnetization or demagnetization is allowed


    // Capacitor Charger
    Charger charger;



    FootController();

    /**
     * @brief Initialize the FootController. If this function fails, Error_Handler() is called.
     */
    void init();

    /**
     * @brief Run the EtherCAT communication loop.
     */
    void runCommunication();

    /**
     * @brief Run the FootController loop. Here, the FSM is executed and the sensors are updated.
     */
    void runControl();

    /**
     * @brief Magnetize or demagnetize the Magnet according to the requested state.
     * @param time Time in 1us to magnetize the magnet.
     */
    void magnetize(uint16_t time);

    /**
     * @brief Estimate the current holding Force.
     *        Currently, this function uses the TMAG5273 Hall sensors to estimate the force.
     *        The force estimation is based on the magnetic field strength measured by the Hall sensors and currently neglects the influence of the LDC sensors.
     *        The force estimation is found empirically and should be calibrated for each individual foot.
     * @return The estimated force.
     */
    float force_estimation();

    //FSM actions
    FSMStatus FSM_bg(FSMStatus state, uint16_t& status_word, int8_t& mode);
    FSMStatus FSM_notReadyToSwitchOn(FSMStatus state, uint16_t& status_word, int8_t& mode);
    FSMStatus FSM_switchOnDisabled(FSMStatus state, uint16_t& status_word, int8_t& mode);
    FSMStatus FSM_readyToSwitchOn(FSMStatus state, uint16_t& status_word, int8_t& mode);
    FSMStatus FSM_switchedOn(FSMStatus state, uint16_t& status_word, int8_t& mode);
    FSMStatus FSM_operationEnabled(FSMStatus state,  uint16_t& status_word, int8_t& mode);
    FSMStatus FSM_quickStopActive(FSMStatus state, uint16_t& status_word, int8_t& mode);
    FSMStatus FSM_faultReactionActive(FSMStatus state, uint16_t& status_word, int8_t& mode);
    FSMStatus FSM_fault(FSMStatus state, uint16_t& status_word, int8_t& mode);
};