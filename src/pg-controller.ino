#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <Adafruit_SSD1306.h>
#include <splash.h>

# define UPDATE_INTERVAL 500 // How often we update the state of the monitor when idle
# define BITS_MULTIPLIER 0.000125f // The voltage per bit

// I2C addresses for the two ADS1115 chips
# define CH12_ADDRESS 0x48
# define CH34_ADDRESS 0x49

// I2C address for the SSD1306 OLED display
#define OLED_RESET     -1
#define SCREEN_ADDRESS 0x3D

// Pins used to interface with the OLED screen
# define OLED_RST -1;
# define OLED_ADDRESS 0x3D

// OLED screen size
# define OLED_WIDTH 128
# define OLED_HEIGHT 64

// Pixel positions for various labels and readings
static const int chOffsetX[] = {0, 64, 0, 64};
static const int chOffsetY[] = {0, 0, 32, 32};

float multipliers[] = {100, 1, 1, 1};

Adafruit_ADS1115 ads1115_12, ads1115_34;
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);

void setup(void)
{
  Serial.begin(9600);

  // Initialize ADS1115 chips and OLED display
  ads1115_12.begin(CH12_ADDRESS);
  ads1115_34.begin(CH34_ADDRESS);

  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);

  // Initialize OLED display layout
  initDisplay();

  Serial.flush();
}

/********* FUNCTIONS **********/

void loop(void)
{
  // Check for incoming serial commands or update gauges
  handleSerialInput();
  updateGauges();
  delay(UPDATE_INTERVAL);
}

// Initialize the layout of the OLED display
void initDisplay(void) {
  display.clearDisplay();

  // Clear the buffer
  display.clearDisplay();

  // Make borders for channels
  display.drawRect(0, 0, display.width(), display.height(), SSD1306_WHITE);
  display.drawLine(0, display.height() / 2, display.width(), display.height() / 2, SSD1306_WHITE);
  display.drawLine(display.width() / 2, 0, display.width() / 2, display.height(), SSD1306_WHITE);

  display.setTextSize(1);             // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);        // Draw white text

  // Display channel labels
  for (int i = 0; i < 4; i++) {
    display.setCursor(chOffsetX[i] + 2, chOffsetY[i] + 2);
    display.println("Ch" + String(i + 1));
  }
  return;
}

// Retrieve a single message from the serial buffer
void handleSerialInput(void) {
  if (Serial.available() > 0) {
    String message = getMessage();
    String func = message.substring(3, 5);
    int channel = message.substring(1, 3).toInt();
    String response;
    if (func == F("RD")) {
      response = getPressure(channel);
    } else if (func == F("SM")) {
      response = updateMultiplier(channel, message.substring(5, 9).toFloat(), multipliers);
    } else {
      // If the function is unknown, send back an error message with the unknown function
      response = func + "?";
    }
    response = '*' + response + '\r';
    Serial.print(response);
  }
  return;
}

// Update the OLED display with pressure values
void updateGauges(void) {
  for (int i = 1; i < 3; i++) {
    String value = getPressure(i);
    updateDisplay(i, value);
    delay(UPDATE_INTERVAL);
  }
}


// update the LCD display used by the pressure gauge monitor
void updateDisplay(int channel, String value)
{
  channel = channel - 1; // switch from one to zero indexing

  // Blank out spot used by given channel to hide old reading
  display.fillRect(chOffsetX[channel] + 8, chOffsetY[channel] + 10,  54, 21, SSD1306_BLACK);
  // Write updated reading where the old reading was
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);

  display.setCursor(chOffsetX[channel] + 8, chOffsetY[channel] + 10);
  display.println(value.substring(0, 4));

  display.setTextSize(1);
  display.setCursor(chOffsetX[channel] + 8, chOffsetY[channel] + 24);
  display.println(value.substring(4, 8) + " Torr");
  // Update the display
  display.display();
  return;
}

// Retrieve a single message from the serial buffer. A message will always start with a '#' and be terminated by a carriage return, '\r'
String getMessage(void)
{
  String message = "";
  // Check if there is data available in the serial buffer
  if (Serial.available() > 0) {
    // Read the incoming character
    char incomingChar = Serial.read();
    // Check if the character is the start of the message
    if (incomingChar == '#') {
      // Process the message until a carriage return is encountered
      while (incomingChar != '\r') {
        // Append the character to the message
        message += incomingChar;
        // Read the next character
        incomingChar = Serial.read();
      }
    } else {
      // If the message is misaligned, flush the serial buffer and send an error.
      Serial.flush();
      message = '?';
    }
    return message;
  }
}

// Get pressure reading from the specified channel
String getPressure(int channel) {
  int16_t bits;
  // Determine the channel to read from
  switch (channel) {
    case 1:
      bits = ads1115_12.readADC_Differential_0_1();
      break;

    case 2:
      bits = ads1115_12.readADC_Differential_2_3();
      break;

    case 3:
      bits = ads1115_34.readADC_Differential_0_1();
      break;

    case 4:
      bits = ads1115_34.readADC_Differential_2_3();
      break;

    default:
      return "?";
  }
  // Convert the bits returned by the ADS1115 to a pressure
  float pressure = bits * BITS_MULTIPLIER * multipliers[channel];
  // Turn this float into a string which will either be displayed on the OLED or sent over the serial interface.
  char buffer[40];
  dtostre(pressure, buffer, 2, 4); // Set flags bit to 0b0100 to make the exponent symbol capital; 'E' rather than default 'e'
  String message = buffer;
  return message;
}


// Update multiplier for a specific channel
String updateMultiplier(int channel, float value, float multipliers[]) {
  multipliers[channel] = value;
  String response = F("PROGM_OK");
  return response;
}
