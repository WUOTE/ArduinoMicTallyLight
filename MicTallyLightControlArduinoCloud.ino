#include "arduino_secrets.h"
#include "thingProperties.h"

// Direction control
const int dirPin = 13;
// Speed control (PWM)
const int pwmPin = 11;
// Brake control
const int brakePin = 8;

unsigned long previousMillis = 0;

// milliseconds
const long interval = 1;

/*
  Instead of constantly calculating required light's intensity at any given point
  a pre-generated lookup table is used.

  The lowest intensity used is 20/255 because at lower levels
  Mika mic tally light starts flickering unpleasantly.
  My experiments show that 20 is the lowest usable value.
  Changing the lowest value to 15, 20 and 25 still gives good results
  while providing smaller change in luminocity. If you want to do just that --
  use `Lookup table calculation\MyDistFunction.ipynb` and
  `Lookup table calculation\MicLookupTable.xlsx`.

  TODO: Test if the lookup table fits in memory on r4
*/

// 400 data points, min value: 20
const static int tallyTable[] = {20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 21, 21, 21, 21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 23, 23, 23, 23, 24, 24, 24, 25, 25, 25, 26, 26, 27, 27, 28, 28, 29, 30, 30, 31, 32, 33, 33, 34, 35, 36, 37, 38, 39, 40, 41, 43, 44, 45, 47, 48, 50, 51, 53, 55, 56, 58, 60, 62, 64, 66, 68, 70, 73, 75, 77, 80, 82, 85, 88, 90, 93, 96, 99, 102, 105, 108, 111, 114, 117, 121, 124, 127, 131, 134, 138, 141, 144, 148, 151, 155, 159, 162, 166, 169, 173, 177, 180, 183, 187, 190, 194, 197, 200, 204, 207, 210, 213, 216, 219, 222, 224, 227, 230, 232, 235, 237, 239, 241, 243, 244, 246, 248, 249, 250, 251, 252, 253, 254, 254, 255, 255, 255, 255, 255, 255, 255, 254, 254, 254, 254, 253, 253, 252, 252, 251, 251, 250, 250, 249, 248, 248, 247, 246, 245, 244, 243, 242, 241, 240, 239, 238, 237, 236, 234, 233, 232, 231, 229, 228, 226, 225, 224, 222, 221, 219, 218, 216, 215, 213, 211, 209, 208, 206, 205, 203, 201, 199, 198, 196, 194, 192, 190, 188, 187, 185, 183, 181, 179, 178, 176, 173, 172, 170, 168, 166, 164, 162, 160, 159, 157, 154, 153, 151, 149, 147, 145, 143, 142, 140, 138, 136, 134, 132, 130, 129, 127, 125, 123, 122, 120, 118, 116, 114, 113, 111, 109, 108, 106, 105, 103, 101, 100, 98, 97, 95, 94, 92, 91, 89, 88, 86, 85, 84, 82, 81, 79, 78, 77, 75, 74, 73, 72, 71, 69, 68, 67, 66, 65, 64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 50, 49, 48, 47, 46, 46, 45, 44, 43, 43, 42, 41, 41, 40, 40, 39, 38, 38, 37, 37, 36, 36, 35, 35, 34, 34, 34, 33, 33, 32, 32, 31, 31, 31, 30, 30, 30, 29, 29, 28, 28, 28, 27, 27, 27, 27, 26, 26, 26, 26, 25, 25, 25, 25, 25, 24, 24, 24, 24, 24, 24, 23, 23, 23, 23, 23, 23, 23, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20};

/*
  Additional static data point sets -- used instead of the map()
  e.g. map(intensityValue, 0, 255, 15, 255)

  400 data points, min value: 15
  { 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 16, 16, 16, 16, 16, 16, 16, 16, 17, 17, 17, 17, 17, 18, 18, 18, 18, 19, 19, 20, 20, 20, 21, 21, 22, 22, 23, 23, 24, 24, 25, 25, 26, 27, 28, 29, 30, 30, 31, 32, 33, 34, 36, 37, 38, 40, 41, 42, 44, 45, 47, 49, 50, 52, 54, 56, 58, 60, 62, 64, 66, 69, 71, 74, 76, 78, 81, 84, 87, 90, 92, 95, 98, 101, 104, 108, 111, 114, 118, 121, 125, 128, 132, 135, 138, 142, 146, 149, 153, 157, 160, 164, 168, 171, 175, 178, 182, 186, 189, 193, 196, 199, 203, 206, 209, 212, 215, 218, 221, 224, 227, 229, 232, 234, 236, 239, 241, 243, 244, 246, 247, 249, 250, 251, 252, 253, 254, 254, 255, 255, 255, 255, 255, 255, 255, 254, 254, 254, 254, 253, 253, 252, 252, 251, 251, 250, 249, 249, 248, 247, 247, 246, 245, 244, 243, 242, 241, 240, 239, 238, 236, 235, 234, 233, 231, 230, 229, 227, 226, 224, 223, 222, 220, 219, 217, 215, 214, 212, 210, 209, 207, 205, 204, 202, 200, 198, 196, 195, 193, 191, 189, 187, 185, 184, 181, 180, 178, 176, 174, 172, 170, 168, 166, 164, 162, 160, 158, 157, 154, 152, 151, 149, 147, 145, 143, 141, 139, 137, 135, 133, 132, 129, 128, 126, 124, 122, 120, 119, 117, 115, 113, 111, 110, 108, 106, 104, 103, 101, 100, 98, 97, 95, 93, 92, 90, 89, 87, 85, 84, 83, 81, 80, 78, 77, 76, 74, 73, 72, 70, 69, 68, 67, 65, 64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 53, 52, 51, 50, 50, 49, 48, 47, 46, 45, 44, 43, 43, 42, 41, 40, 40, 39, 38, 38, 37, 37, 36, 35, 34, 34, 33, 33, 32, 32, 31, 31, 30, 30, 29, 29, 28, 28, 27, 27, 26, 26, 26, 25, 25, 25, 24, 24, 24, 23, 23, 23, 23, 22, 22, 22, 22, 21, 21, 21, 21, 20, 20, 20, 20, 20, 19, 19, 19, 19, 18, 18, 18, 18, 18, 18, 18, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15 };

  400 data points, min value: 10
  { 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 13, 13, 13, 14, 14, 14, 15, 15, 15, 16, 16, 17, 17, 18, 18, 19, 20, 20, 21, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 34, 35, 36, 38, 39, 41, 43, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62, 65, 67, 70, 72, 75, 78, 80, 83, 86, 89, 92, 95, 98, 101, 105, 108, 111, 115, 118, 122, 125, 129, 133, 136, 140, 143, 147, 151, 154, 158, 162, 166, 169, 173, 177, 180, 184, 188, 191, 195, 198, 202, 205, 208, 211, 215, 217, 220, 223, 226, 229, 231, 234, 236, 238, 240, 242, 244, 246, 247, 249, 250, 251, 252, 253, 254, 254, 255, 255, 255, 255, 255, 255, 255, 254, 254, 254, 254, 253, 253, 252, 252, 251, 251, 250, 249, 249, 248, 247, 247, 245, 245, 244, 243, 242, 240, 239, 238, 237, 236, 235, 233, 232, 231, 230, 228, 227, 225, 224, 222, 221, 219, 218, 216, 215, 213, 211, 209, 208, 206, 204, 203, 201, 199, 197, 195, 193, 191, 190, 188, 186, 184, 182, 180, 178, 176, 174, 172, 170, 168, 166, 164, 162, 160, 158, 156, 154, 152, 150, 148, 146, 145, 142, 141, 139, 137, 135, 133, 131, 129, 127, 125, 123, 121, 119, 118, 116, 114, 112, 110, 108, 107, 105, 103, 101, 100, 98, 96, 95, 93, 91, 90, 88, 87, 85, 84, 82, 80, 79, 78, 76, 75, 73, 72, 71, 69, 68, 67, 65, 64, 63, 61, 60, 59, 58, 57, 56, 55, 54, 52, 51, 50, 49, 48, 47, 46, 45, 44, 43, 43, 42, 41, 40, 39, 38, 38, 37, 36, 35, 34, 34, 33, 32, 32, 31, 31, 30, 29, 29, 28, 28, 27, 27, 26, 26, 25, 25, 24, 23, 23, 23, 22, 22, 21, 21, 21, 20, 20, 20, 19, 19, 18, 18, 18, 18, 17, 17, 17, 17, 16, 16, 16, 16, 15, 15, 15, 15, 15, 14, 14, 14, 14, 14, 14, 13, 13, 13, 13, 13, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10 };

  400 data points, min value: 0
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 6, 7, 7, 8, 8, 9, 10, 10, 11, 12, 13, 14, 14, 15, 16, 17, 18, 20, 21, 22, 23, 25, 26, 27, 29, 31, 32, 34, 36, 38, 39, 41, 43, 45, 48, 50, 52, 55, 57, 60, 62, 65, 67, 70, 73, 76, 79, 82, 85, 88, 92, 95, 99, 102, 105, 109, 113, 116, 120, 124, 128, 131, 135, 139, 143, 147, 150, 154, 158, 162, 166, 170, 174, 177, 181, 185, 189, 192, 196, 199, 203, 206, 210, 213, 216, 219, 222, 225, 228, 230, 233, 235, 238, 240, 242, 244, 245, 247, 248, 250, 251, 252, 253, 254, 254, 255, 255, 255, 255, 255, 255, 255, 254, 254, 254, 254, 253, 253, 252, 252, 251, 251, 250, 249, 248, 248, 247, 246, 245, 244, 243, 242, 241, 240, 239, 238, 237, 235, 234, 233, 231, 230, 228, 227, 226, 224, 223, 221, 220, 218, 216, 214, 213, 211, 209, 207, 206, 204, 202, 200, 199, 196, 195, 193, 191, 189, 187, 185, 183, 181, 179, 177, 175, 173, 171, 169, 167, 165, 163, 161, 158, 156, 154, 152, 150, 148, 146, 144, 142, 140, 138, 136, 134, 132, 130, 128, 126, 124, 122, 120, 118, 116, 114, 112, 110, 108, 106, 104, 102, 101, 99, 97, 95, 94, 92, 90, 88, 87, 85, 83, 81, 80, 78, 77, 75, 73, 72, 70, 69, 67, 66, 64, 63, 62, 60, 59, 57, 56, 55, 53, 52, 51, 50, 49, 48, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 29, 28, 27, 26, 25, 25, 24, 23, 23, 22, 21, 21, 20, 20, 19, 18, 18, 17, 17, 16, 15, 15, 15, 14, 14, 13, 13, 12, 12, 11, 11, 11, 10, 10, 10, 9, 9, 8, 8, 8, 8, 7, 7, 7, 7, 6, 6, 6, 6, 6, 5, 5, 5, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
*/

/*
  Array size:
  https://www.arduino.cc/reference/en/language/variables/utilities/sizeof/
*/
int sizeOftallyTable = sizeof(tallyTable) / sizeof(tallyTable[0]);

// USB Serial Control Variables
const byte numChars = 32;
// an array to store the received data
char receivedChars[numChars];
boolean newData = false;
String currentUSBMode = "off";

void setup()
{
  Serial.begin(115200);
  delay(1500);

  initProperties();
  pinMode(dirPin, OUTPUT);
  pinMode(pwmPin, OUTPUT);
  pinMode(brakePin, OUTPUT);

  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  setDebugMessageLevel(2);
  ArduinoCloud.printDebugInfo();

  Serial.println("<Arduino is ready>");
}

void loop()
{
  ArduinoCloud.update();

  // Check for USB Serial commands
  recvWithEndMarker();
  processUSBMode();

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval)
  {
    previousMillis = currentMillis;

    // Prioritize USB mode if set, otherwise use Cloud mode
    String activeMode = (currentUSBMode != "off") ? currentUSBMode : tallyMode;

    if (activeMode == "idle" || activeMode == "hot")
    {
      static int currentIndex = 0;

      // Read and map the current intensity value
      int lightIntensity = tallyTable[currentIndex];

      // Set light intensity from the lookup table
      analogWrite(pwmPin, lightIntensity);

      // Increment index and wrap around
      currentIndex = (currentIndex + 1) % sizeOftallyTable;
    }
  }
}

// USB Serial Command Reception
void recvWithEndMarker()
{
  static byte ndx = 0;
  char endMarker = '\n';
  char rc;

  while (Serial.available() > 0 && newData == false)
  {
    rc = Serial.read();

    if (rc != endMarker)
    {
      receivedChars[ndx] = rc;
      ndx++;
      if (ndx >= numChars)
      {
        ndx = numChars - 1;
      }
    }
    else
    {
      receivedChars[ndx] = '\0';
      ndx = 0;
      currentUSBMode = receivedChars;
      Serial.println(currentUSBMode);
      newData = true;
    }
  }
}

// Process USB Mode
void processUSBMode()
{
  if (!newData)
    return;

  Serial.print("Setting USB mode: ");
  Serial.println(currentUSBMode);

  // Directly set mode similar to onTallyModeChange()
  if (currentUSBMode == "off")
  {
    digitalWrite(dirPin, HIGH);
    digitalWrite(brakePin, HIGH);
    analogWrite(pwmPin, 0);
  }
  else if (currentUSBMode == "idle")
  {
    digitalWrite(dirPin, LOW);
    digitalWrite(brakePin, LOW);
  }
  else if (currentUSBMode == "hot")
  {
    digitalWrite(dirPin, HIGH);
    digitalWrite(brakePin, LOW);
  }
  else if (currentUSBMode == "red")
  {
    digitalWrite(dirPin, HIGH);
    digitalWrite(brakePin, LOW);
    analogWrite(pwmPin, 255);
  }
  else if (currentUSBMode == "white")
  {
    digitalWrite(dirPin, LOW);
    digitalWrite(brakePin, LOW);
    analogWrite(pwmPin, 255);
  }

  newData = false;
}

// Handle commands from Arduino Cloud Dashboard
void onTallyModeChange()
{
  Serial.print("Cloud mode changed to: ");
  Serial.println(tallyMode);

  // Only change mode if USB mode is not actively overriding
  if (currentUSBMode == "off")
  {
    if (tallyMode == "off")
    {
      digitalWrite(dirPin, HIGH);
      digitalWrite(brakePin, HIGH);
      analogWrite(pwmPin, 0);
    }
    else if (tallyMode == "idle")
    {
      digitalWrite(dirPin, LOW);
      digitalWrite(brakePin, LOW);
    }
    else if (tallyMode == "hot")
    {
      digitalWrite(dirPin, HIGH);
      digitalWrite(brakePin, LOW);
    }
    else if (tallyMode == "red")
    {
      digitalWrite(dirPin, HIGH);
      digitalWrite(brakePin, LOW);
      analogWrite(pwmPin, 255);
    }
    else if (tallyMode == "white")
    {
      digitalWrite(dirPin, LOW);
      digitalWrite(brakePin, LOW);
      analogWrite(pwmPin, 255);
    }
  }
}