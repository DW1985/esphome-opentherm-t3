/*
 * OpenTherm protocol implementation. Originally taken from https://github.com/jpraus/arduino-opentherm, but
 * heavily modified to comply with ESPHome coding standards and provide better logging.
 * Original code is licensed under Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International
 * Public License, which is compatible with GPLv3 license, which covers C++ part of ESPHome project.
 */

#pragma once

#include "atomic"
#include <string>
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include "HardwareSerial.h"
#include "Stm32AppProtocol.h"

namespace esphome {
namespace opentherm {

template<class T> constexpr T read_bit(T value, uint8_t bit) { return (value >> bit) & 0x01; }

template<class T> constexpr T set_bit(T value, uint8_t bit) { return value |= (1UL << bit); }

template<class T> constexpr T clear_bit(T value, uint8_t bit) { return value &= ~(1UL << bit); }

template <class T> constexpr T write_bit(T value, uint8_t bit, uint8_t bit_value) {
  return bit_value ? set_bit(value, bit) : clear_bit(value, bit);
}

enum OperationMode {
  IDLE = 0,            // no operation
  RECEIVED,            // data frame received with valid start and stop bit
  SENT,                // all data written to output
  ERROR_PROTOCOL = 8,  // manchester protocol data transfer error
  ERROR_TIMEOUT = 9    // read timeout
};

enum ProtocolErrorType {
  NO_ERROR = 0,            // No error
  NO_TRANSITION = 1,       // No transition in the middle of the bit
  INVALID_STOP_BIT = 2,    // Stop bit wasn't present when expected
  PARITY_ERROR = 3,        // Parity check didn't pass
  NO_CHANGE_TOO_LONG = 4,  // No level change for too much timer ticks
};

enum MessageType {
  READ_DATA = 0,
  READ_ACK = 4,
  WRITE_DATA = 1,
  WRITE_ACK = 5,
  INVALID_DATA = 2,
  DATA_INVALID = 6,
  UNKNOWN_DATAID = 7
};

enum MessageId {
  STATUS = 0,
  CH_SETPOINT = 1,
  CONTROLLER_CONFIG = 2,
  DEVICE_CONFIG = 3,
  COMMAND_CODE = 4,
  FAULT_FLAGS = 5,
  REMOTE = 6,
  COOLING_CONTROL = 7,
  CH2_SETPOINT = 8,
  CH_SETPOINT_OVERRIDE = 9,
  TSP_COUNT = 10,
  TSP_COMMAND = 11,
  FHB_SIZE = 12,
  FHB_COMMAND = 13,
  MAX_MODULATION_LEVEL = 14,
  MAX_BOILER_CAPACITY = 15,  // u8_hb - u8_lb gives min modulation level
  ROOM_SETPOINT = 16,
  MODULATION_LEVEL = 17,
  CH_WATER_PRESSURE = 18,
  DHW_FLOW_RATE = 19,
  DAY_TIME = 20,
  DATE = 21,
  YEAR = 22,
  ROOM_SETPOINT_CH2 = 23,
  ROOM_TEMP = 24,
  FEED_TEMP = 25,
  DHW_TEMP = 26,
  OUTSIDE_TEMP = 27,
  RETURN_WATER_TEMP = 28,
  SOLAR_STORE_TEMP = 29,
  SOLAR_COLLECT_TEMP = 30,
  FEED_TEMP_CH2 = 31,
  DHW2_TEMP = 32,
  EXHAUST_TEMP = 33,
  FAN_SPEED = 35,
  FLAME_CURRENT = 36,
  ROOM_TEMP_CH2 = 37,
  REL_HUMIDITY = 38,
  DHW_BOUNDS = 48,
  CH_BOUNDS = 49,
  OTC_CURVE_BOUNDS = 50,
  DHW_SETPOINT = 56,
  MAX_CH_SETPOINT = 57,
  OTC_CURVE_RATIO = 58,

  // HVAC Specific Message IDs
  HVAC_STATUS = 70,
  REL_VENT_SETPOINT = 71,
  DEVICE_VENT = 74,
  HVAC_VER_ID = 75,
  REL_VENTILATION = 77,
  REL_HUMID_EXHAUST = 78,
  EXHAUST_CO2 = 79,
  SUPPLY_INLET_TEMP = 80,
  SUPPLY_OUTLET_TEMP = 81,
  EXHAUST_INLET_TEMP = 82,
  EXHAUST_OUTLET_TEMP = 83,
  EXHAUST_FAN_SPEED = 84,
  SUPPLY_FAN_SPEED = 85,
  REMOTE_VENTILATION_PARAM = 86,
  NOM_REL_VENTILATION = 87,
  HVAC_NUM_TSP = 88,
  HVAC_IDX_TSP = 89,
  HVAC_FHB_SIZE = 90,
  HVAC_FHB_IDX = 91,

  RF_SIGNAL = 98,
  DHW_MODE = 99,
  OVERRIDE_FUNC = 100,

  // Solar Specific Message IDs
  SOLAR_MODE_FLAGS = 101,  // hb0-2 Controller storage mode
                           // lb0   Device fault
                           // lb1-3 Device mode status
                           // lb4-5 Device status
  SOLAR_ASF = 102,
  SOLAR_VERSION_ID = 103,
  SOLAR_PRODUCT_ID = 104,
  SOLAR_NUM_TSP = 105,
  SOLAR_IDX_TSP = 106,
  SOLAR_FHB_SIZE = 107,
  SOLAR_FHB_IDX = 108,
  SOLAR_STARTS = 109,
  SOLAR_HOURS = 110,
  SOLAR_ENERGY = 111,
  SOLAR_TOTAL_ENERGY = 112,

  FAILED_BURNER_STARTS = 113,
  BURNER_FLAME_LOW = 114,
  OEM_DIAGNOSTIC = 115,
  BURNER_STARTS = 116,
  CH_PUMP_STARTS = 117,
  DHW_PUMP_STARTS = 118,
  DHW_BURNER_STARTS = 119,
  BURNER_HOURS = 120,
  CH_PUMP_HOURS = 121,
  DHW_PUMP_HOURS = 122,
  DHW_BURNER_HOURS = 123,
  OT_VERSION_CONTROLLER = 124,
  OT_VERSION_DEVICE = 125,
  VERSION_CONTROLLER = 126,
  VERSION_DEVICE = 127
};

enum BitPositions { STOP_BIT = 33 };

/**
 * Structure to hold Opentherm data packet content.
 * Use f88(), u16() or s16() functions to get appropriate value of data packet accoridng to id of message.
 */
struct OpenthermData {
  uint8_t type;
  uint8_t id;
  uint8_t valueHB;
  uint8_t valueLB;

  OpenthermData() : type(0), id(0), valueHB(0), valueLB(0) {}

  /**
   * @return float representation of data packet value
   */
  float f88();

  /**
   * @param float number to set as value of this data packet
   */
  void f88(float value);

  /**
   * @return unsigned 16b integer representation of data packet value
   */
  uint16_t u16();

  /**
   * @param unsigned 16b integer number to set as value of this data packet
   */
  void u16(uint16_t value);

  /**
   * @return signed 16b integer representation of data packet value
   */
  int16_t s16();

  /**
   * @param signed 16b integer number to set as value of this data packet
   */
  void s16(int16_t value);
};

struct OpenThermError {
  ProtocolErrorType error_type;
  uint32_t capture;
  uint8_t clock;
  uint32_t data;
  uint8_t bit_pos;
};

/**
 * Opentherm static class that supports either listening or sending Opentherm data packets in the same time
 */
class OpenTherm {
public:
  OpenTherm(InternalGPIOPin *in_pin, InternalGPIOPin *out_pin, InternalGPIOPin *boot_pin, InternalGPIOPin *reset_pin, int32_t device_timeout = 800);

  /**
   * Setup pins.
   */
  bool initialize();

  void delayed_initialize();
  void loop();

  /**
   * Use this function to check whether listen() function already captured a valid data packet.
   *
   * @return true if data packet has been captured from line by listen() function.
   */
  bool has_message() { return mode_ == OperationMode::RECEIVED; }

  /**
   * Use this to retrive data packed captured by listen() function. Data packet is ready when has_message() function
   * returns true. This function can be called multiple times until stop() is called.
   *
   * @param data reference to data structure to which fill the data packet data.
   * @return true if packet was ready and was filled into data structure passed, false otherwise.
   */
  bool get_message(OpenthermData &data);

  /**
   * Immediately send out Opentherm data packet to line connected on given pin.
   * Completed data transfer is indicated by is_sent() function.
   * Error state is indicated by is_error() function.
   *
   * @param data Opentherm data packet.
   */
  void send(OpenthermData &data);

  /**
   * Stops listening for data packet or sending out data packet and resets internal state of this class.
   * Stops all timers and unattaches all interrupts.
   */
  void stop();

  /**
   * Get protocol error details in case a protocol error occured.
   * @param error reference to data structure to which fill the error details
   * @return true if protocol error occured during last conversation, false otherwise.
   */
  bool get_protocol_error(OpenThermError &error);

  /**
   * Use this function to check whether send() function already finished sending data packed to line.
   *
   * @return true if data packet has been sent, false otherwise.
   */
  bool is_sent() { return mode_ == OperationMode::SENT; }

  /**
   * Indicates whether listinig or sending is not in progress.
   * That also means that no timers are running and no interrupts are attached.
   *
   * @return true if listening nor sending is in progress.
   */
  bool is_idle() { return mode_ == OperationMode::IDLE; }

  /**
   * Indicates whether last listen() or send() operation ends up with an error. Includes both timeout and
   * protocol errors.
   *
   * @return true if last listen() or send() operation ends up with an error.
   */
  bool is_error() {
    return mode_ == OperationMode::ERROR_TIMEOUT || mode_ == OperationMode::ERROR_PROTOCOL;
  }

  /**
   * Indicates whether last listen() or send() operation ends up with a *timeout* error
   * @return true if last listen() or send() operation ends up with a *timeout* error.
   */
  bool is_timeout() { return mode_ == OperationMode::ERROR_TIMEOUT; }

  /**
   * Indicates whether last listen() or send() operation ends up with a *protocol* error
   * @return true if last listen() or send() operation ends up with a *protocol* error.
   */
  bool is_protocol_error() {
    return mode_ == OperationMode::ERROR_PROTOCOL;
  }

  OperationMode get_mode() {
    return mode_;
  }

  void debug_data(OpenthermData &data);

  const char *protocol_error_to_str(ProtocolErrorType error_type);
  const char *message_type_to_str(MessageType message_type);
  const char *operation_mode_to_str(OperationMode mode);
  const char *message_id_to_str(MessageId id);

private:
  InternalGPIOPin *in_pin_;
  InternalGPIOPin *out_pin_;
  InternalGPIOPin *boot_pin_;
  InternalGPIOPin *reset_pin_;

  uint32_t data_;
  std::atomic<OperationMode> mode_;

  HardwareSerial Serial1;
  ProtocolSerializer m_Serializer;
  uint8_t m_TxBuffer[ProtocolSerializer::BUFFER_SIZE];

  void onGenericStatusResponse(const GenericStatusResponse *resp);
  void onOtCommandResponse(const OtCommandResponse *resp);

  bool check_parity_(uint32_t val);

  void bit_read_(uint8_t value);
  ProtocolErrorType verify_stop_bit_(uint8_t value);
  void write_bit_(uint8_t high, uint8_t clock);
};

}  // namespace opentherm
}  // namespace esphome
