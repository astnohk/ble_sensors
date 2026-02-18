#include <Wire.h>
#include <bluefruit.h>

//#define USE_SERIAL

uint16_t conn_hdl = BLE_CONN_HANDLE_INVALID;

// Define hardware: LED and Button pins and states
const int LED_PIN = 7;
#define LED_OFF LOW
#define LED_ON HIGH

const int BUTTON_PIN = 13;
#define BUTTON_ACTIVE LOW
const int INTERRUPT_PIN = 5;

#define ADVERTISING_RAW_DATA_SIZE 16

#define RV8803_SLAVE_ADDR 0x32
//#define AMG8833_SLAVE_ADDR 0x68
#define AMG8833_SLAVE_ADDR 0x69

class RTC_RV8803
{
private:
  TwoWire *wire;
  uint8_t slave_addr;

public:
  void init(TwoWire *_wire, uint8_t _slave_addr)
  {
    this->wire = _wire;
    this->slave_addr = _slave_addr;
  }

  void read_date(void)
  {
    uint8_t date[7];
    int i = 0;
    this->wire->beginTransmission(this->slave_addr);
    this->wire->write(0x00); // SECONDS register
    this->wire->endTransmission();
    this->wire->requestFrom(this->slave_addr, 7);
    while (this->wire->available())
    {
      if (i < 7)
      {
        date[i] = this->wire->read();
        i++;
      }
      else
      {
        this->wire->read();
      }
    }
  }

  void set_timer_counter(uint16_t val)
  {
    this->wire->beginTransmission(this->slave_addr);
    this->wire->write(0x0B); // TIMER COUNTER 0 register
    this->wire->write(static_cast<uint8_t>(0xFF & val));
    this->wire->write(static_cast<uint8_t>(0x0F & (val >> 8)));
    this->wire->endTransmission();
  }

  void set_TE(bool te)
  {
    // Read current register
    uint8_t reg = 0x00;
    this->wire->beginTransmission(this->slave_addr);
    this->wire->write(0x0D); // EXTENSION register
    this->wire->endTransmission();
    this->wire->requestFrom(this->slave_addr, 1);
    while (this->wire->available())
    {
      reg = this->wire->read();
    }
    // Write flag
    this->wire->beginTransmission(this->slave_addr);
    this->wire->write(0x0D); // EXTENSION register
    this->wire->write((0xEF & reg) | (te ? 0x10 : 0x00));
    this->wire->endTransmission();
  }

  void set_TD(uint8_t td)
  {
    // Read current register
    uint8_t reg = 0x00;
    this->wire->beginTransmission(this->slave_addr);
    this->wire->write(0x0D); // EXTENSION register
    this->wire->endTransmission();
    this->wire->requestFrom(this->slave_addr, 1);
    while (this->wire->available())
    {
      reg = this->wire->read();
    }
    // Write flag
    this->wire->beginTransmission(this->slave_addr);
    this->wire->write(0x0D); // EXTENSION register
    this->wire->write((0xFC & reg) | (0x03 & td));
    this->wire->endTransmission();
  }

  void set_TIE(bool enable)
  {
    // Read current register
    uint8_t reg = 0x00;
    this->wire->beginTransmission(this->slave_addr);
    this->wire->write(0x0F); // CONTROL register
    this->wire->endTransmission();
    this->wire->requestFrom(this->slave_addr, 1);
    while (this->wire->available())
    {
      reg = this->wire->read();
    }
    // Write flag
    this->wire->beginTransmission(this->slave_addr);
    this->wire->write(0x0F); // CONTROL register
    this->wire->write((0xEE & reg) | (enable ? 0x10 : 0x00)); // Set TIE with clearing RESET flag
    this->wire->endTransmission();
  }

  void clear_flag(void)
  {
    this->wire->beginTransmission(this->slave_addr);
    this->wire->write(0x0E); // FLAG register
    this->wire->write(0x00);
    this->wire->endTransmission();
  }

  uint8_t read_flag(void)
  {
    uint8_t reg = 0x00;
    this->wire->beginTransmission(this->slave_addr);
    this->wire->write(0x0E); // FLAG register
    this->wire->endTransmission();
    this->wire->requestFrom(this->slave_addr, 1);
    while (this->wire->available())
    {
      reg = this->wire->read();
    }
    return reg;
  }
};

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
      int16_t temp = static_cast<int16_t>((data[1] << 8) | data[0]);
      // Check 12-bits 2's complement
      if (temp > 0x0800)
      {
        temp = (0x0FFF & temp) - 0x1000;
      }
      // Clamp at 0x7F (clamp within 10-bits because it should be 8-bits after 2-bits right shifting)
      temp = temp > 0x7F ? 0x7F : temp;
      this->values[i] = static_cast<int8_t>(temp >> 2);
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

RTC_RV8803 rtc;
GridEYE sensor;

void connect_callback(uint16_t conn_handle)
{
  conn_hdl = conn_handle;
}

void setup()
{
  // Initialize hardware:
#ifdef USE_SERIAL
  // Serial is the USB serial port
  Serial.begin(9600);
  time_t timeout = millis();
  while (!Serial)
  {
    if ((millis() - timeout) < 2000)
    {
      delay(100);
    }
    else
    {
      break;
    }
  }
  Serial.print("hello\n");
#else
  delay(500);
#endif
  // Initialize I2C
  
  // I2C
#ifdef ARDUINO_NRF52840_ITSYBITSY
  //// Adafruit ItsyBitsy
  Wire.setPins(21, 22); // SDA: 21, SCL: 22
#endif
#ifdef ARDUINO_NRF52840_FEATHER
# if USB_VID == 0x239A
# elif USB_VID == 0x1B4F
  //// SparkFun
  Wire.setPins(8, 11); // SDA: 8, SCL: 11
# endif
#endif
  Wire.begin();

  // Turn on-board blue LED off
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LED_OFF);
  // Set Button to input mode
  pinMode(BUTTON_PIN, INPUT);
  nrf_gpio_cfg_sense_input(INTERRUPT_PIN, NRF_GPIO_PIN_PULLUP, NRF_GPIO_PIN_SENSE_LOW); // Receive reset signal as low enabled (neg INT)

  // RTC
  rtc.init(&Wire, RV8803_SLAVE_ADDR);
  rtc.set_TIE(false); // Disable countdown timer interrupt signal output on INT pin
  rtc.set_TE(false); // Disable countdown timer
  rtc.clear_flag(); // Clear all flags
  rtc.set_timer_counter(30); // 30sec time interval
  rtc.set_TD(2); // Set 1Hz clock frequency

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
  Bluefruit.Advertising.setInterval(244, 244);
  // number of seconds in fast mode:
  Bluefruit.Advertising.setFastTimeout(30);

  delay(500); // Wait for sensing

#ifdef USE_SERIAL
  Serial.write("setup is done.\n");
#endif
}

void loop()
{
  sensor.read_temperature();
  int8_t adv_data[18];
  adv_data[0] = 0xff; // UUID
  adv_data[1] = 0xff; // UUID
  sensor.resample4x4();
  int8_t *values = sensor.get_values();
  memcpy(adv_data + 2, values, 16); // Put sensor data just after UUID values

  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE); // Use General Discovery Mode to send advertising unlimitedly
  Bluefruit.Advertising.addData(0xFF, adv_data, 2 + ADVERTISING_RAW_DATA_SIZE); // Put sensor data on advertising
  Bluefruit.Advertising.start(0);

  delay(3000);

  if (conn_hdl != BLE_CONN_HANDLE_INVALID)
  {
    Bluefruit.disconnect(conn_hdl);
  }
  conn_hdl = BLE_CONN_HANDLE_INVALID;
  Bluefruit.Advertising.stop();
  Bluefruit.Advertising.clearData();

  rtc.set_TIE(true); // Enable RTC countdown timer interrupt signal output on INT pin
  rtc.set_TE(true); // Enable RTC countdown timer interrupt
  sd_power_system_off();
}
