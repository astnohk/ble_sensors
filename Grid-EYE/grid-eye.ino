/* BLE Example for SparkFun Pro nRF52840 Mini 
 *  
 *  This example demonstrates how to use the Bluefruit
 *  library to both send and receive data to the
 *  nRF52840 via BLE.
 *  
 *  Using a BLE development app like Nordic's nRF Connect
 *  https://www.nordicsemi.com/eng/Products/Nordic-mobile-Apps/nRF-Connect-for-Mobile
 *  The BLE UART service can be written to to turn the
 *  on-board LED on/off, or read from to monitor the 
 *  status of the button.
 *  
 *  See the tutorial for more information:
 *  https://learn.sparkfun.com/tutorials/nrf52840-development-with-arduino-and-circuitpython#arduino-examples  
*/
#include <Wire.h>
#include <bluefruit.h>

uint16_t conn_hdl = BLE_CONN_HANDLE_INVALID;

// Define hardware: LED and Button pins and states
const int LED_PIN = 7;
#define LED_OFF LOW
#define LED_ON HIGH

const int BUTTON_PIN = 13;
#define BUTTON_ACTIVE LOW
int lastButtonState = -1;

#define ADVERTISING_RAW_DATA_SIZE 16

//#define AMG8833_SLAVE_ADDR 0x68
#define AMG8833_SLAVE_ADDR 0x69

class GridEYE
{
private:
  TwoWire *wire;
  uint8_t slave_addr;
  int8_t values[64];

public:
  void init(TwoWire *_wire, uint8_t _slave_addr)
  {
    this->wire = _wire;
    this->slave_addr = _slave_addr;
  }

  uint8_t values_size(void)
  {
    return 64;
  }

  int8_t *get_values(void)
  {
    return this->values;
  }

  void reset(void)
  {
    this->wire->beginTransmission(this->slave_addr);
    this->wire->write(0x01);
    this->wire->write(0x30);
    this->wire->endTransmission();
  }

  void set_framerate(bool enable_high_framerate)
  {
    this->wire->beginTransmission(this->slave_addr);
    this->wire->write(0x02);
    this->wire->write(enable_high_framerate ? 0x00 : 0x01);
    this->wire->endTransmission();
  }

  void set_average(bool enable)
  {
    uint8_t data[10] = {0x1F, 0x50, 0x1F, 0x45, 0x1F, 0x57, 0x07, 0x00, 0x1F, 0x00};
    data[7] = enable ? 0x20 : 0x00;
    this->wire->beginTransmission(this->slave_addr);
    this->wire->write(data, 10);
    this->wire->endTransmission();
  }

  uint8_t read_status(void)
  {
    uint8_t data;
    this->wire->beginTransmission(this->slave_addr);
    this->wire->write(0x04); // Register addres 0x04
    this->wire->endTransmission();
    this->wire->requestFrom(this->slave_addr, 1);
    while (this->wire->available())
    {
      data = this->wire->read();
    }
    return data;
  }

  void read_temperature(void)
  {
    size_t i;
    int8_t bytes;
    uint8_t data[2];
    for (i = 0; i < 64; i++)
    {
      this->wire->beginTransmission(this->slave_addr);
      this->wire->write(0x80 + 2*i); // Start register addres 0x80
      this->wire->endTransmission();
      this->wire->requestFrom(this->slave_addr, 2);
      bytes = 0;
      while (this->wire->available())
      {
        data[bytes] = this->wire->read();
        if (bytes < 1)
        {
          bytes = 1;
        }
      }
      this->values[i] = static_cast<int8_t>((0xC0 & (data[1] << 6)) | (0x3F & (data[0] >> 2)));
    }
  }

  void resample4x4()
  {
    // Resample values in-place
    for (int i = 0; i < 4; i++)
    {
      for (int k = 0; k < 4; k++)
      {
        this->values[4 * i + k] = max(
          max(this->values[8 * (2 * i) + 2 * k], this->values[8 * (2 * i) + 2 * k + 1]),
          max(this->values[8 * (2 * i + 1) + 2 * k], this->values[8 * (2 * i + 1) + 2 * k + 1]));
      }
    }
  }
};

GridEYE sensor;

void connect_callback(uint16_t conn_handle)
{
  conn_hdl = conn_handle;
}

void setup()
{
  delay(500);
  // Initialize hardware:
  // Serial is the USB serial port
  Serial.begin(9600);
  // Initialize I2C
  Wire.begin();
  Wire.setPins(8, 11);
  // Turn on-board blue LED off
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LED_OFF);
  // Set Button to input mode
  pinMode(BUTTON_PIN, INPUT);
  // Thermal sensor
  sensor.init(&Wire, AMG8833_SLAVE_ADDR);
  sensor.reset();
  sensor.set_framerate(false);
  sensor.set_average(true);

  // Initialize Bluetooth:
  Bluefruit.begin();
  Bluefruit.Periph.setConnectCallback(connect_callback);
  // Set max power. Accepted values are: -40, -30, -20, -16, -12, -8, -4, 0, 4
  Bluefruit.setTxPower(4);
  Bluefruit.setName("SparkFun_nRF52840_Grid-EYE");

  // Start advertising device
  Bluefruit.Advertising.addTxPower();
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  // Set advertising interval (in unit of 0.625ms):
  Bluefruit.Advertising.setInterval(32, 244);
  // number of seconds in fast mode:
  Bluefruit.Advertising.setFastTimeout(30);

  Serial.write("setup is done.\n");
}

void loop()
{
  sensor.read_temperature();
  int8_t *values = sensor.get_values();
  sensor.resample4x4();
  Serial.println(values[0]);
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addData(0xFF, values, ADVERTISING_RAW_DATA_SIZE);
  Bluefruit.Advertising.start(0);

  delay(1000);

  if (conn_hdl != BLE_CONN_HANDLE_INVALID)
  {
    Bluefruit.disconnect(conn_hdl);
  }
  conn_hdl = BLE_CONN_HANDLE_INVALID;
  Bluefruit.Advertising.stop();
  Bluefruit.Advertising.clearData();
  delay(5000);
}
