#include <Wire.h>
#include <bluefruit.h>

//#define USE_SERIAL

uint16_t conn_hdl = BLE_CONN_HANDLE_INVALID;

// RTC timer interrupt interval [sec]
#define RTC_TIME_INTERVAL 60
// Duration of transmitting BLE advertising [millisec]
#define BLE_ADVERTISING_DURATION 5000

// Define hardware: LED and Button pins and states
#ifdef ARDUINO_NRF52840_ITSYBITSY
  //// Adafruit ItsyBitsy
  const int LED_PIN = 3;
# define LED_OFF LOW
# define LED_ON HIGH

  const int BUTTON_PIN = 4;
# define BUTTON_ACTIVE LOW

  const int INTERRUPT_NRF_PORT = 1;
  const int INTERRUPT_NRF_PIN = 8;
#endif
#ifdef ARDUINO_NRF52840_FEATHER
# if USB_VID == 0x239A
# elif USB_VID == 0x1B4F
    //// SparkFun
    const int LED_PIN = 7;
#   define LED_OFF LOW
#   define LED_ON HIGH

    const int BUTTON_PIN = 13;
#   define BUTTON_ACTIVE LOW

    const int INTERRUPT_NRF_PORT = 0;
    const int INTERRUPT_NRF_PIN = 5;
# endif
#endif

#define ADVERTISING_RAW_DATA_SIZE (2*2)

#define RV8803_SLAVE_ADDR 0x32
#define HTU21D_SLAVE_ADDR 0x40

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

class HTU21D
{
private:
  TwoWire *wire;
  uint8_t slave_addr;
  int16_t value;

public:
  void init(TwoWire *_wire, uint8_t _slave_addr)
  {
    this->wire = _wire;
    this->slave_addr = _slave_addr;
  }

  int16_t get_value(void)
  {
    return this->value;
  }

  void reset(void)
  {
    this->wire->beginTransmission(this->slave_addr);
    this->wire->write(0xFE);
    this->wire->endTransmission();
  }

  int16_t read_temperature()
  {
    this->wire->beginTransmission(this->slave_addr);
    this->wire->write(0xF3);
    this->wire->endTransmission();
    uint16_t val = 0;
    for (int i = 0; i < 5 && val == 0; i++)
    {
      delay(100);
      this->wire->requestFrom(this->slave_addr, 3);
      while (this->wire->available())
      {
        uint8_t data = this->wire->read();
        if ((val & 0xFF00) == 0)
        {
          val = (val << 8) | data;
        }
      }
    }
    float fval = (175.72 * (0xFFFC & val)) / 65536.0 - 46.85;
    return (int16_t)roundf(4.0 * fval);
  }

  int16_t read_humidity()
  {
    this->wire->beginTransmission(this->slave_addr);
    this->wire->write(0xF5);
    this->wire->endTransmission();
    uint16_t val = 0;
    for (int i = 0; i < 5 && val == 0; i++)
    {
      delay(100);
      this->wire->requestFrom(this->slave_addr, 3);
      while (this->wire->available())
      {
        uint8_t data = this->wire->read();
        if ((val & 0xFF00) == 0)
        {
          val = (val << 8) | data;
        }
      }
    }
    float fval = (125.0 * (0xFFFC & val)) / 65536.0 - 6.0;
    return (int16_t)roundf(4.0 * fval);
  }
};

RTC_RV8803 rtc;
HTU21D sensor;

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
  nrf_gpio_cfg_sense_input(NRF_GPIO_PIN_MAP(INTERRUPT_NRF_PORT, INTERRUPT_NRF_PIN), NRF_GPIO_PIN_PULLUP, NRF_GPIO_PIN_SENSE_LOW); // Receive reset signal as low enabled (neg INT)

  // RTC
  rtc.init(&Wire, RV8803_SLAVE_ADDR);
  rtc.set_TIE(false); // Disable countdown timer interrupt signal output on INT pin
  rtc.set_TE(false); // Disable countdown timer
  rtc.set_TD(2); // Set 1Hz clock frequency
  rtc.set_timer_counter(RTC_TIME_INTERVAL); // 30sec time interval
  rtc.clear_flag(); // Clear all flags

  // Thermal sensor
  sensor.init(&Wire, HTU21D_SLAVE_ADDR);

  // Initialize Bluetooth:
  Bluefruit.begin();
  Bluefruit.Periph.setConnectCallback(connect_callback);
  // Set max power. Accepted values are: -40, -30, -20, -16, -12, -8, -4, 0, 4
  Bluefruit.setTxPower(4);
  Bluefruit.setName("SparkFun_nRF52840_TH");

  // Start advertising device
  Bluefruit.Advertising.addTxPower();
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  // Set advertising interval (in unit of 0.625ms):
  Bluefruit.Advertising.setInterval(244, 244);
  // number of seconds in fast mode:
  Bluefruit.Advertising.setFastTimeout(30);

  delay(500); // Wait

#ifdef USE_SERIAL
  Serial.write("setup is done.\n");
#endif
}

void loop()
{
  int16_t temp = sensor.read_temperature();
  int16_t humid = sensor.read_humidity();
#ifdef USE_SERIAL
  Serial.print(temp / 4.0);
  Serial.print(humid / 4.0);
#endif
  int8_t adv_data[2 + ADVERTISING_RAW_DATA_SIZE];
  adv_data[0] = 0xff; // UUID
  adv_data[1] = 0xf4; // UUID
  adv_data[2] = 0xff & (temp >> 8);
  adv_data[3] = 0xff & temp;
  adv_data[4] = 0xff & (humid >> 8);
  adv_data[5] = 0xff & humid;

  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE); // Use General Discovery Mode to send advertising unlimitedly
  Bluefruit.Advertising.addData(0xFF, adv_data, 2 + ADVERTISING_RAW_DATA_SIZE); // Put sensor data on advertising
  Bluefruit.Advertising.start(0);

  delay(BLE_ADVERTISING_DURATION);

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
