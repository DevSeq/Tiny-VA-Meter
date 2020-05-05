// Using Arduino 1.8.5
#include <Arduino.h>
#include <Wire.h>
// Using U8g2 2.24.3
#include <U8g2lib.h>
// Using Adafruit INA219 1.0.5
#include <Adafruit_INA219.h>

#define ENABLE_TERMINAL 1 // Comment in to disable serial communication. Release memory.
//#define DISPLAY_BUS_VOLTAGE 1 // Uncomment to display bus voltage on screen.

enum {
  MENU_MAIN,
  MENU_VA,
  MENU_P,
  MENU_SETTINGS_ENTER,
  MENU_SETTINGS_RANGE,
  MENU_SETTINGS_REFRESH,
  MENU_SETTINGS_SLEEP
};

enum {
  INA219_RANGE_32V_3A = 0,
  INA219_RANGE_32V_1A = 1,
  INA219_RANGE_16V_400mA = 2
};

#define BUTTON_PRESSED_STATE HIGH // Active state for button
#define USB_POWER_INPUT_STATE HIGH // Input pin state for USB powered
#define LONG_PRESS_DURATION 1000  // ms.
#define SEC_PR_HOUR 3600

#define POWER_SELECT_PIN 6        // Indicator input for USB or input power
#define BUTTON_PIN 2

Adafruit_INA219 ina219;
U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, SCL, SDA);

bool button_pressed = false;
bool wait_for_button_release = false;
bool sensor_sleep = false;
unsigned long button_pressed_start = 0;
unsigned long currentMillis = 0;
unsigned long last_refresh = 0;
unsigned long refresh_rate = 200;
uint8_t current_menu = MENU_MAIN;
uint8_t current_ina_range = INA219_RANGE_32V_3A;
float mAh = 0;
int power_select_known_state = 0;
unsigned long power_select_change_time = 0;


void print_four_lines(const char* line1, const char* line2, const char* line3, const char* line4)
{
  u8g2.firstPage();
  do {
    u8g2.drawStr(0,15,line1);
    u8g2.drawStr(0,31,line2);
    u8g2.drawStr(0,47,line3);
    u8g2.drawStr(0,63,line4);
  } while ( u8g2.nextPage() );
}

void print_two_lines(const char* line1, const char* line2)
{
  u8g2.firstPage();
  do {
    u8g2.drawStr(0,20,line1);
    u8g2.drawStr(0,40,line2);
  } while ( u8g2.nextPage() );
}

void print_one_line(const char* line1)
{
  u8g2.firstPage();
  do {
    u8g2.drawStr(0,20,line1);
  } while ( u8g2.nextPage() );
}

void setup(void) {
  // Configure pin BUTTON_PIN & POWER_SELECT_PIN as inputs and enable the internal pull-up resistor
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(POWER_SELECT_PIN, INPUT_PULLUP);
  // Initialize OLED
  u8g2.begin();
  u8g2.setFont(u8g2_font_ncenB14_tr); // choose a suitable font

#ifdef ENABLE_TERMINAL
  // Initialize Serial
  Serial.begin(115200);
#endif

  // Initialize the INA219.
  // By default the initialization will use the largest range (32V, 2A).  However
  // you can call a setCalibration function to change this range (see comments).
  ina219.begin();
}

void long_press()
{
    switch(current_menu) {
    case MENU_MAIN:
      current_menu = MENU_VA;
      break;
    case MENU_VA:
      break;
    case MENU_P:
      mAh = 0;
      break;
    case MENU_SETTINGS_ENTER:
      current_menu = MENU_SETTINGS_RANGE;
      break;
    case MENU_SETTINGS_RANGE:
    if(current_ina_range == INA219_RANGE_32V_3A) {
      current_ina_range = INA219_RANGE_32V_1A;
      ina219.setCalibration_32V_1A();
    } else if(current_ina_range == INA219_RANGE_32V_1A) {
      current_ina_range = INA219_RANGE_16V_400mA;
      ina219.setCalibration_16V_400mA();
    } else {
      current_ina_range = INA219_RANGE_32V_3A;
      ina219.setCalibration_32V_2A();
    }    
      break;
    case MENU_SETTINGS_REFRESH:
    if(refresh_rate == 100) {
      refresh_rate = 200;
    } else if(refresh_rate == 200) {
      refresh_rate = 500;
    } else if(refresh_rate == 500) {
      refresh_rate = 1000;
    } else {
      refresh_rate = 100;
    } 
      break;
    case MENU_SETTINGS_SLEEP:
      sensor_sleep = !sensor_sleep;
      if(!sensor_sleep){
        ina219.powerSave(false);
      }
      break;
    default:
      current_menu = MENU_MAIN;
      break;
  }
}
void short_press()
{
    switch(current_menu) {
    case MENU_MAIN:
      current_menu = MENU_VA;
      break;
    case MENU_VA:
      u8g2.setFont(u8g2_font_ncenB12_tr);
      current_menu = MENU_P;
      break;
    case MENU_P:
      u8g2.setFont(u8g2_font_ncenB14_tf);
      current_menu = MENU_SETTINGS_ENTER;
      break;
    case MENU_SETTINGS_ENTER:
      current_menu = MENU_VA;
      break;
    case MENU_SETTINGS_RANGE:
      current_menu = MENU_SETTINGS_REFRESH;
      break;
    case MENU_SETTINGS_REFRESH:
      current_menu = MENU_SETTINGS_SLEEP;
      break;
    case MENU_SETTINGS_SLEEP:
      current_menu = MENU_VA;
      break;
    default:
      current_menu = MENU_MAIN;
      break;
  }
}

void read_button()
{
  const int buttonState = digitalRead(BUTTON_PIN);
  // check if the pushbutton is pressed.
  if(buttonState == BUTTON_PRESSED_STATE) {
    if(button_pressed == true) {
      // Button is allready pressed.
      // Check if button has been held for LONG_PRESS_DURATION.
      if(!wait_for_button_release && (button_pressed_start + LONG_PRESS_DURATION < currentMillis)) {
        wait_for_button_release = true;
        long_press();
      }
    } else {
      // New button press.
      button_pressed_start = currentMillis;
      button_pressed = true;
    }
  } else {
    if(button_pressed == true)
    {
      // Button has just been released.
      // Don't trigger short press if long press was just released.
      if(!wait_for_button_release){ 
        short_press();
      }
    }
    wait_for_button_release = false;
    button_pressed = false;
  }
}

void update_screen()
{
  if(sensor_sleep) {
    ina219.powerSave(false);
    delay(2); // Allow sensor boot < 1 ms.
  }
  
  const float shuntvoltage = ina219.getShuntVoltage_mV();
  const float busvoltage = ina219.getBusVoltage_V();
  const float current_mA = ina219.getCurrent_mA();
  const float loadvoltage = busvoltage + (shuntvoltage / 1000);
  const float power_mw = ina219.getPower_mW();

  if(sensor_sleep) {
    ina219.powerSave(true);
  }

  const float samlpe_rate = (float)refresh_rate;

  mAh += current_mA / ((1000/samlpe_rate) * SEC_PR_HOUR);
  
  switch(current_menu) {
    case MENU_MAIN:
    power_select_known_state = digitalRead(POWER_SELECT_PIN);
    if(power_select_known_state == USB_POWER_INPUT_STATE)  {
      print_two_lines("Input range:", "0-32V 0-3.2A");
    } else {
      print_two_lines("Input range:", "3-15V 0-3.2A");
    }
      
      if(currentMillis >= power_select_change_time + 3000) {
        short_press();
      }
      break;
    case MENU_VA: // Display voltage and amps
    {
#ifdef DISPLAY_BUS_VOLTAGE
      String volt = String(loadvoltage) + "/" + String(busvoltage) + " V";
#else
      String volt = String(loadvoltage) + " V";
#endif
      String amp = String(current_mA) + " mA";
      print_two_lines(amp.c_str(), volt.c_str());
    }
      break;
    case MENU_P: // Display voltage, amps, watt and Ah
    {
#ifdef DISPLAY_BUS_VOLTAGE
      String volt = String(loadvoltage) + "/" + String(busvoltage) + " V";
#else
      String volt = String(loadvoltage) + " V";
#endif
      String amp = String(current_mA) + " mA";
      String power = String(power_mw) + " mW";
      String ampHours = String(mAh) + " mAh";
      print_four_lines(volt.c_str(),power.c_str(), amp.c_str(), ampHours.c_str());
    }
      break;
    case MENU_SETTINGS_ENTER:
      print_one_line("Settings");
      break;
    case MENU_SETTINGS_RANGE: // Display active INA219 sensor range
      if(current_ina_range == INA219_RANGE_32V_3A) {
        print_two_lines("Active range", "32V & 3.2A");
      } else if(current_ina_range == INA219_RANGE_32V_1A) {
        print_two_lines("Active range", "32V & 1A");
      } else { // INA219_RANGE_16V_400mA
        print_two_lines("Active range", "16V & 0.4A");
      }
      break;
    case MENU_SETTINGS_REFRESH: // Display active refresh rate
    {
      String rate = String(refresh_rate) + " ms";
      print_two_lines("Refresh rate", rate.c_str());
    }
      break;
    case MENU_SETTINGS_SLEEP: // Display active sensor sleep setting
      if(sensor_sleep) {
        print_two_lines("Sensor sleep", "Enabled");
      } else {
        print_two_lines("Sensor sleep", "Disabled");
      }
      break;
    default:
      current_menu = MENU_MAIN;
      break;
  }
  
#ifdef ENABLE_TERMINAL
  Serial.print("Millis:        "); Serial.print(currentMillis); Serial.println(" ms");
  Serial.print("Bus Voltage:   "); Serial.print(busvoltage); Serial.println(" V");
  Serial.print("Shunt Voltage: "); Serial.print(shuntvoltage); Serial.println(" mV");
  Serial.print("Load Voltage:  "); Serial.print(loadvoltage); Serial.println(" V");
  Serial.print("Current:       "); Serial.print(current_mA); Serial.println(" mA");
  Serial.print("Power:         "); Serial.print(power_mw); Serial.println(" mW");
  Serial.print("Comsumption:   "); Serial.print(mAh); Serial.println(" mAh");
  Serial.println("");
#endif
}
#ifdef ENABLE_TERMINAL
void receive_serial()
{
  String command;
  if(Serial.available()){
    command = Serial.readStringUntil('\n');
     
    if(command.equals("reset")){
      mAh = 0;
      Serial.println("mAh reset");
    } else if(command.equals("sleep on")) {
      sensor_sleep = true;
      Serial.println("sleep on");
    } else if(command.equals("sleep off")) {
      sensor_sleep = false;
      Serial.println("sleep off");
    } else if(command.equals("refresh 100")) {
      refresh_rate = 100;
      Serial.println("refresh rate 100");
    } else if(command.equals("refresh 200")) {
      refresh_rate = 200;
      Serial.println("refresh rate 200");
    } else if(command.equals("refresh 500")) {
      refresh_rate = 500;
      Serial.println("refresh rate 500");
    } else if(command.equals("refresh 1000")) {
      refresh_rate = 1000;
      Serial.println("refresh rate 1000");
    } else if(command.equals("range 0")) {
      current_ina_range = INA219_RANGE_32V_3A;
      ina219.setCalibration_32V_2A();
      Serial.println("range 32V 3.2A");
    } else if(command.equals("range 1")) {
      current_ina_range = INA219_RANGE_32V_1A;
      ina219.setCalibration_32V_1A();
      Serial.println("range 32V 1A");
    } else if(command.equals("range 2")) {
      current_ina_range = INA219_RANGE_16V_400mA;
      ina219.setCalibration_16V_400mA();
      Serial.println("range 16V 0.4A");
    } else {
      Serial.println("Commands:");
      Serial.println("- reset (reset mAh measurement)");
      Serial.println("- sleep on (Enable INA219 sleep between samples - reduce leak current)");
      Serial.println("- sleep off (Disable INA219 sleep between samples - fastest operation)");
      Serial.println("- refresh x (Set screen & serial refresh rate. x can be 100, 200, 500 or 1000)");
      Serial.println("- range x (Set INA219 range. x can be 0 for 3.2A, 1 for 1A or 2 for 0.4A)");
      Serial.println("");
    }
  }
}
#endif

void loop(void) {
  currentMillis = millis();
  read_button();
#ifdef ENABLE_TERMINAL
  receive_serial();
#endif
  if(last_refresh + refresh_rate <= currentMillis){
    last_refresh += refresh_rate;
    update_screen();
  }
  if(power_select_known_state != digitalRead(POWER_SELECT_PIN)){
    current_menu = MENU_MAIN;
    power_select_change_time = currentMillis;
  }
}
