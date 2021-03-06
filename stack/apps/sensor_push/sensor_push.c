/* * OSS-7 - An opensource implementation of the DASH7 Alliance Protocol for ultra
 * lowpower wireless sensor communication
 *
 * Copyright 2015 University of Antwerp
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


// This examples pushes sensor data to gateway(s) by manually constructing an ALP command with a file read result action
// (unsolicited message). The D7 session is configured to request ACKs. All received ACKs are printed.
// Temperature data is used as a sensor value, when a HTS221 is available, otherwise value 0 is used.

#include "hwleds.h"
#include "hwsystem.h"
#include "scheduler.h"
#include "timer.h"
#include "assert.h"

#include <stdio.h>
#include <stdlib.h>

#include "hwlcd.h"
#include "d7ap_stack.h"
#include "fs.h"
#include "log.h"

#include "../shared/shared.h"

#ifdef USE_HTS221
  #include "HTS221_Driver.h"
  #include "hwi2c.h"
#endif

#define SENSOR_FILE_ID           0x40
#define SENSOR_FILE_SIZE         2
#define SENSOR_INTERVAL_SEC	TIMER_TICKS_PER_SEC * 30

#ifdef USE_HTS221
  static i2c_handle_t* hts221_handle;
#endif

// Define the D7 interface configuration used for sending the ALP command on
static d7ap_master_session_config_t session_config = {
    .qos = {
        .qos_resp_mode = SESSION_RESP_MODE_ANY,
        .qos_retry_mode = SESSION_RETRY_MODE_NO,
        .qos_stop_on_error       = false,
        .qos_record              = false
    },
    .dormant_timeout = 0,
    .addressee = {
        .ctrl = {
            .nls_method = AES_NONE,
            .id_type = ID_TYPE_NOID,
        },
        .access_class = 0x01,
        .id = 0
    }
};



void execute_sensor_measurement()
{
  // first get the sensor reading ...
  int16_t temperature = 0; // in decicelsius. When there is no sensor, we just transmit 0 degrees

#if defined USE_HTS221
  HTS221_Get_Temperature(hts221_handle, &temperature);
#endif

  // Generate ALP command. We do this manually for now (until we have an API for this).
  // We will be sending a return file data action, without a preceding file read request.
  // This is an unsolicited message, where we push the sensor data to the gateway(s).
  // Please refer to the spec for the format

  uint8_t alp_command[4 + SENSOR_FILE_SIZE] = {
    // ALP Control byte
    ALP_OP_RETURN_FILE_DATA,
    // File Data Request operand:
    SENSOR_FILE_ID, // the file ID
    0, // offset in file
    SENSOR_FILE_SIZE // data length
    // the sensor data, see below
  };

  temperature = __builtin_bswap16(temperature); // convert to big endian before transmission
  memcpy(alp_command + 4, (uint8_t*)&temperature, SENSOR_FILE_SIZE);

  alp_layer_execute_command(alp_command, sizeof(alp_command), &session_config);
  timer_post_task_delay(&execute_sensor_measurement, SENSOR_INTERVAL_SEC);
}

void on_alp_command_completed_cb(uint8_t tag_id, bool success)
{
    if(success)
      log_print_string("Command completed successfully");
    else
      log_print_string("Command failed, no ack received");
}

void on_alp_command_result_cb(d7ap_session_result_t result, uint8_t* payload, uint8_t payload_length)
{
    log_print_string("recv response @ %i dB link budget from:", result.link_budget);
    log_print_data(result.addressee.id, 8);
}

static alp_init_args_t alp_init_args;

void bootstrap()
{

    log_print_string("Device booted\n");

    fs_init_args_t fs_init_args = (fs_init_args_t){
        .access_profiles_count = DEFAULT_ACCESS_PROFILES_COUNT,
        .access_profiles = default_access_profiles,
        .access_class = 0x21 // push only AC, no scanning
    };

    alp_init_args.alp_command_completed_cb = &on_alp_command_completed_cb;
    alp_init_args.alp_command_result_cb = &on_alp_command_result_cb;
    d7ap_stack_init(&fs_init_args, &alp_init_args, false, NULL);

#if defined USE_HTS221
    hts221_handle = i2c_init(0, 0, 100000);
    HTS221_DeActivate(hts221_handle);
    HTS221_Set_BduMode(hts221_handle, HTS221_ENABLE);
    HTS221_Set_Odr(hts221_handle, HTS221_ODR_7HZ);
    HTS221_Activate(hts221_handle);
#endif

    sched_register_task(&execute_sensor_measurement);
    timer_post_task_delay(&execute_sensor_measurement, SENSOR_INTERVAL_SEC);
}
