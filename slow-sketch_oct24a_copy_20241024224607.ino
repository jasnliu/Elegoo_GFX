#include <Elegoo_GFX.h>    // Core graphics library
#include <Elegoo_TFTLCD.h> // Hardware-specific library
#include <TouchScreen.h>
#include <SD.h>            // SD card library

#define SD_CS_PIN 10     // Chip select pin for SD card module

// Pin definitions for the touchscreen
#define YP A3  // Analog pin
#define XM A2  // Analog pin
#define YM 9   // Digital pin
#define XP 8   // Digital pin

// Touchscreen calibration constants for the ILI9341
#define TS_MINX 120
#define TS_MAXX 900
#define TS_MINY 70
#define TS_MAXY 920

// Touchscreen pressure calibration
#define MINPRESSURE 10
#define MAXPRESSURE 1000
#define TS_RESISTANCE 300  // Resistance value for X plate

// Pin definitions for the LCD
#define LCD_CS A3
#define LCD_CD A2
#define LCD_WR A1
#define LCD_RD A0
#define LCD_RESET A4

// Color definitions
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

// Box and pen configurations
#define BOXSIZE 40
#define BOXGAP  10  // Added gap between boxes
#define PENRADIUS 3

// Y positions for the text buttons (placed below the color buttons)
#define TEXT_BUTTON_Y_OFFSET (BOXSIZE + 20)

Elegoo_TFTLCD tft(LCD_CS, LCD_CD, LCD_WR, LCD_RD, LCD_RESET);
TouchScreen ts = TouchScreen(XP, YP, XM, YM, TS_RESISTANCE);

int oldcolor, currentcolor;

// SD Card variables
File file;
int16_t lastX = -9999;  // Invalid initial value to ensure the first point is written
int16_t lastY = -9999;  // Invalid initial value to ensure the first point is written

// Function Prototypes
void initializeLCD();
void drawUI();
void handleTouch(TSPoint p);
void changeColor(TSPoint p);
void handleTextButtons(TSPoint p);
void clearScreen();
void addToFile(int16_t x, int16_t y, int color);
void saveToSD();
void loadFromSD();

// Setup function
void setup(void) {
  Serial.begin(9600);
  initializeLCD();
  drawUI();
  pinMode(13, OUTPUT);  // Pin 13 for debugging LED

  // Initialize the SD card
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("SD card initialization failed!");
    return;
  }
  Serial.println("SD card initialized.");

  // Erase current.bin and previous.bin files if they exist
  SD.remove("current.bin");
  SD.remove("previous.bin");
  Serial.println("current.bin and previous.bin erased.");
}

// Main loop function
void loop() {
  digitalWrite(13, HIGH);
  TSPoint p = ts.getPoint();  // Read touch point
  digitalWrite(13, LOW);

  // Reconfigure touchscreen pins after reading
  pinMode(XM, OUTPUT);
  pinMode(YP, OUTPUT);

  // Check if a valid touch has been detected
  if (p.z > MINPRESSURE && p.z < MAXPRESSURE) {
    handleTouch(p);  // Handle touch input
  }
}

// Initialize the LCD and display information about the driver
void initializeLCD() {
  Serial.println(F("Paint!"));
  
  tft.reset();
  uint16_t identifier = tft.readID();
  
  switch(identifier) {
    case 0x9325:
      Serial.println(F("Found ILI9325 LCD driver"));
      break;
    case 0x9328:
      Serial.println(F("Found ILI9328 LCD driver"));
      break;
    case 0x4535:
      Serial.println(F("Found LGDP4535 LCD driver"));
      break;
    case 0x7575:
      Serial.println(F("Found HX8347G LCD driver"));
      break;
    case 0x9341:
      Serial.println(F("Found ILI9341 LCD driver"));
      break;
    case 0x8357:
      Serial.println(F("Found HX8357D LCD driver"));
      break;
    default:
      Serial.print(F("Unknown LCD driver chip: "));
      Serial.println(identifier, HEX);
      identifier = 0x9341;
  }
  
  tft.begin(identifier);
  tft.setRotation(2);
  tft.fillScreen(BLACK);
}

// Draw the user interface with color selection boxes and text buttons
void drawUI() {
  // Spread the color boxes evenly with gaps in between
  tft.fillRect(0, 0, BOXSIZE, BOXSIZE, GREEN);
  tft.fillRect(BOXSIZE + BOXGAP, 0, BOXSIZE, BOXSIZE, BLUE);
  tft.fillRect(2 * (BOXSIZE + BOXGAP), 0, BOXSIZE, BOXSIZE, RED);
  tft.fillRect(3 * (BOXSIZE + BOXGAP), 0, BOXSIZE, BOXSIZE, YELLOW);
  tft.fillRect(4 * (BOXSIZE + BOXGAP), 0, BOXSIZE, BOXSIZE, WHITE);

  currentcolor = RED;

  // Text buttons placed below the color boxes
  tft.fillRect(0, TEXT_BUTTON_Y_OFFSET, BOXSIZE, BOXSIZE, CYAN); // SV button
  tft.fillRect(BOXSIZE + BOXGAP, TEXT_BUTTON_Y_OFFSET, BOXSIZE, BOXSIZE, MAGENTA); // LD button
  tft.fillRect(2 * (BOXSIZE + BOXGAP), TEXT_BUTTON_Y_OFFSET, BOXSIZE, BOXSIZE, RED); // DEL button

  // Labels for the text buttons
  tft.setCursor(10, TEXT_BUTTON_Y_OFFSET + 10);
  tft.setTextColor(BLACK);
  tft.setTextSize(2);
  tft.println("SV");

  tft.setCursor(BOXSIZE + BOXGAP + 10, TEXT_BUTTON_Y_OFFSET + 10);
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.println("LD");

  tft.setCursor(2 * (BOXSIZE + BOXGAP) + 10, TEXT_BUTTON_Y_OFFSET + 10);
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.println("DEL");
}

// Handle touch input
void handleTouch(TSPoint p) {
  // Map touch input to screen coordinates
  p.x = map(p.x, TS_MINX, TS_MAXX, tft.width(), 0);
  p.y = tft.height() - map(p.y, TS_MINY, TS_MAXY, tft.height(), 0);

  if (p.y < BOXSIZE) {
    changeColor(p);  // Change drawing color if touch is in color box area
  } else if (p.y >= TEXT_BUTTON_Y_OFFSET && p.y < TEXT_BUTTON_Y_OFFSET + BOXSIZE) {
    handleTextButtons(p);  // Handle text button presses
  } else if (p.y < tft.height()) {
    tft.fillCircle(p.x, p.y, PENRADIUS, currentcolor);  // Draw circle at touch point
    addToFile(p.x, p.y, currentcolor);  // Add touch coordinates to the buffer
  }
}

// Change the current drawing color based on touch input in the palette area
void changeColor(TSPoint p) {
  oldcolor = currentcolor;

  if (p.x < BOXSIZE) {
    currentcolor = GREEN;
  } else if (p.x < (BOXSIZE + BOXGAP) * 2) {
    currentcolor = BLUE;
  } else if (p.x < (BOXSIZE + BOXGAP) * 3) {
    currentcolor = RED;
  } else if (p.x < (BOXSIZE + BOXGAP) * 4) {
    currentcolor = YELLOW;
  } else if (p.x < (BOXSIZE + BOXGAP) * 5) {
    clearScreen();  // Erase the screen if the "DEL" box is touched
  }
}

// Handle presses on the SV, LD, and DEL buttons
void handleTextButtons(TSPoint p) {
  if (p.x < BOXSIZE) {
    // SV button functionality
    saveToSD();
  } else if (p.x < (BOXSIZE + BOXGAP) * 2) {
    // LD button functionality
    loadFromSD();
  } else if (p.x < (BOXSIZE + BOXGAP) * 3) {
    // DEL button functionality
    clearScreen();
  }
}

// Add touch coordinates (x, y) and color to the current.bin file if not within 7x7 grid
void addToFile(int16_t x, int16_t y, int color) {
  // Check if the new point is within a 7x7 grid from the last point
  if (abs(x - lastX) <= 3 && abs(y - lastY) <= 3) {
    // If the new point is within the 7x7 grid, skip writing
    return;
  }

  // Open the current.bin file for appending data
  file = SD.open("current.bin", FILE_WRITE);
  if (file) {
    // Append x, y, and color values as bytes
    file.write((uint8_t*)&x, sizeof(x));  // Write x coordinate (2 bytes)
    file.write((uint8_t*)&y, sizeof(y));  // Write y coordinate (2 bytes)
    file.write((uint8_t*)&color, sizeof(color));  // Write color (2 bytes)
    file.close();  // Close the file after writing

    // Update the last written point
    lastX = x;
    lastY = y;
  } else {
    Serial.println("Failed to open current.bin for writing!");
  }
}

// Save the current state (current.bin) by copying it to previous.bin
void saveToSD() {
  SD.remove("previous.bin");
  File currentFile = SD.open("current.bin", FILE_READ);
  File previousFile = SD.open("previous.bin", FILE_WRITE);

  if (currentFile && previousFile) {
    // Copy current.bin to previous.bin
    while (currentFile.available()) {
      previousFile.write(currentFile.read());
    }
    currentFile.close();
    previousFile.close();
    Serial.println("Saved current drawing to previous.bin.");
  } else {
    Serial.println("Failed to open current.bin or previous.bin for saving!");
  }
}

// Load the previous state (previous.bin), copy it to current.bin, and render the pixels
void loadFromSD() {
  clearScreen();
  SD.remove("current.bin");
  File previousFile = SD.open("previous.bin", FILE_READ);
  File currentFile = SD.open("current.bin", FILE_WRITE);  // Overwrite current.bin

  if (previousFile && currentFile) {
    // Copy previous.bin to current.bin
    while (previousFile.available()) {
      currentFile.write(previousFile.read());
    }

    // Close files after copying
    previousFile.close();
    currentFile.close();

    // Now load the data from current.bin and render the pixels on the screen
    currentFile = SD.open("current.bin", FILE_READ);  // Reopen for reading

    if (currentFile) {
      while (currentFile.available()) {
        int16_t x, y;
        uint16_t color;

        // Read x, y, and color from the file
        currentFile.read((uint8_t*)&x, sizeof(x));
        currentFile.read((uint8_t*)&y, sizeof(y));
        currentFile.read((uint8_t*)&color, sizeof(color));

        // Render the loaded pixel on the screen
        tft.fillCircle(x, y, PENRADIUS, color);
      }
      currentFile.close();
      Serial.println("Loaded and rendered previous drawing from previous.bin.");
    } else {
      Serial.println("Failed to reopen current.bin for reading!");
    }
  } else {
    Serial.println("Failed to open previous.bin or current.bin for loading!");
  }
}

// Clear the screen except for the top palette area and text buttons
void clearScreen() {
  Serial.println("begin clear screen");
  tft.fillRect(0, BOXSIZE + BOXSIZE + 10, tft.width(), tft.height() - (BOXSIZE + BOXSIZE + 10), BLACK);
  Serial.println("screen cleared");
  SD.remove("current.bin");
  Serial.println("removed current.bin after clearning screen");
}
