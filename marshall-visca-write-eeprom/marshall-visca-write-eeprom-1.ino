#include <EEPROM.h>
#include <CRC32.h>

#define UD 0 //up-down
#define OO 1 //on-off
struct updown {
  char nam[10];
  byte len;  //max 5
  byte up[5];
  byte down[5];
  byte mode;
  byte dummy[10]; //up to 32 bytes total
};
#define UPDOWN_MEANINGFUL_BYTES 22 //all bytes over this value are meaningless and CRC check is omitted

//32 bytes total too
struct adjust {
  char nam[10];
  byte len; //max 10
  byte cmd[10];
  byte hba1; //first half-byte position of adjustment variable, e.g command 01 00 0p > half byte position is 5.
  byte hba2; //if no first half-byte, it should be 0xFF
  byte mi; //minimum adjustment value
  byte ma; //maximum adjustment value
  byte last[7]; //last values for each 0-7 camera
};
#define ADJUST_MEANINGFUL_BYTES 25

#define P1OFFSET 32

#define TOT_P1 15
#define DUMM1 {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
const updown paint1[TOT_P1] = {
  { "RED       ", 4, {0x01, 0x04, 0x03, 0x02}, {0x01, 0x04, 0x03, 0x03}, UD, DUMM1}, //r-
  { "BLUE      ", 4, {0x01, 0x04, 0x04, 0x02}, {0x01, 0x04, 0x04, 0x03}, UD, DUMM1}, //r-
  { "GAIN      ", 4, {0x01, 0x04, 0x0C, 0x02}, {0x01, 0x04, 0x0C, 0x03}, UD, DUMM1}, //+
  { "SHUTTER   ", 4, {0x01, 0x04, 0x0A, 0x02}, {0x01, 0x04, 0x0A, 0x03}, UD, DUMM1}, //+
  { "IS        ", 4, {0x01, 0x04, 0x34, 0x02}, {0x01, 0x04, 0x34, 0x03}, OO, DUMM1}, //+
  { "DZOOM     ", 4, {0x01, 0x04, 0x06, 0x02}, {0x01, 0x04, 0x06, 0x03}, OO, DUMM1}, //command ok, does not react. > maybe dzoom adjust
  { "IRIS      ", 4, {0x01, 0x04, 0x0B, 0x02}, {0x01, 0x04, 0x0B, 0x03}, UD, DUMM1}, //r+, no iris @225 cam
  { "EXP.COMP. ", 4, {0x01, 0x04, 0x0E, 0x02}, {0x01, 0x04, 0x0E, 0x03}, UD, DUMM1}, //?? no in meny
  { "BLACK LVL ", 5, {0x01, 0x7E, 0x04, 0x15, 0x02}, {0x01, 0x7E, 0x04, 0x15, 0x03}, UD, DUMM1}, //r-
  { "WHITE LVL ", 5, {0x01, 0x7E, 0x04, 0x16, 0x02}, {0x01, 0x7E, 0x04, 0x16, 0x03}, UD, DUMM1}, //r-
  { "BACKLIGHT ", 4, {0x01, 0x04, 0x33, 0x02}, {0x01, 0x04, 0x33, 0x03}, OO, DUMM1}, //+
  { "WDR       ", 4, {0x01, 0x04, 0x3D, 0x02}, {0x01, 0x04, 0x3D, 0x03}, OO, DUMM1}, //+
  { "DWDR      ", 4, {0x01, 0x04, 0x3D, 0x06}, {0x01, 0x04, 0x3D, 0x03}, OO, DUMM1}, //r+, no effect
  { "SHARPNESS ", 4, {0x01, 0x04, 0x02, 0x02}, {0x01, 0x04, 0x02, 0x03}, UD, DUMM1}, //+
  { "STABILIZER", 4, {0x01, 0x04, 0x34, 0x02}, {0x01, 0x04, 0x34, 0x03}, OO, DUMM1}, //+
};

#define TOT_P2 6
#define DUMM2 {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x0A}
const adjust paint2[TOT_P2] = {
  //hba positions:     0-1   2-3   4-5   6-7   8-9 10-11 12-13
  { "CONTRAST  ", 6, {0x01, 0x7E, 0x04, 0x51, 0x00, 0x00}, 9, 11, 0x00, 0x14, DUMM2}, //r-
  { "BRIGHTNESS", 6, {0x01, 0x7E, 0x04, 0x52, 0x00, 0x00}, 9, 11, 0x00, 0x14, DUMM2}, //r-
  //hba positions:     0-1   2-3   4-5   6-7   8-9 10-11 12-13
  { "COLOR GAIN", 7, {0x01, 0x04, 0x49, 0x00, 0x00, 0x00, 0x00}, 0xFF, 13, 0x0, 0xE, DUMM2}, //+
  { "COLOR HUE ", 7, {0x01, 0x04, 0x4F, 0x00, 0x00, 0x00, 0x00}, 0xFF, 13, 0x0, 0xE, DUMM2}, //+, not found in menu
  //hba positions:     0-1   2-3   4-5   6-7   8-9 10-11 12-13
  { "GAMMA     ", 4, {0x01, 0x04, 0x5B, 0x00}, 6, 7, 0x00, 0x15, DUMM2}, //+
  { "NR        ", 4, {0x01, 0x04, 0x53, 0x00}, 0xFF, 7, 0x0, 0x5, DUMM2}, //+, DNR Mode
};

#define CMD1 01 04 1E 00 00 00 01 00 00
#define WB_MANUAL 01 04 35 05
//#define 01 04 3F 01 7F

#define SPEED 57600

void setup() {
  Serial.begin(SPEED);
  Serial.println("EEPROM WRITE");
}

//#define ERASE_MODE

void loop() {
#ifdef ERASE_MODE
  //clear EEPROM
  unsigned long time;
  time = millis();
  for (int i = 0; i < 1024; i++) {
    EEPROM.update(i, 0xFF);
    Serial.println(i);
  }
  Serial.print("EEPROM cleared in ms:");
  Serial.println(millis() - time);
#else
  CRC32 crc;
  byte i, j, k;
  int p2offset = P1OFFSET + sizeof(paint1);;

  for (i = 0; i < TOT_P1; i++) {
    EEPROM.put(P1OFFSET + i * sizeof(paint1[i]), paint1[i]);
  }
  for (i = 0; i < TOT_P2; i++) {
    EEPROM.put(p2offset + i * sizeof(paint2[i]), paint2[i]);
  }

  Serial.println("EEPROM WRITE OK");
  Serial.print("p1offset=0x");   Serial.println(P1OFFSET, HEX);
  Serial.print("p1 size = ");    Serial.println(sizeof(paint1[0]));
  Serial.print("p2offset=0x");   Serial.println(p2offset, HEX);
  Serial.print("p2 size = ");    Serial.println(sizeof(paint2[0]));

  Serial.println("BYTES TO CALCULATE CRC");
  crc.reset();
  for (i = 0; i < TOT_P1; i++) {
    for (j = 0; j <  UPDOWN_MEANINGFUL_BYTES; j++) {
      EEPROM.get(P1OFFSET + i * sizeof(paint1[i]) + j, k);
      Serial.print(P1OFFSET + i * sizeof(paint1[i]) + j, HEX);
      Serial.print(' ');
      crc.update(k);
    }
    Serial.println();
  }
  for (i = 0; i < TOT_P2; i++) {
    for (j = 0; j <  ADJUST_MEANINGFUL_BYTES; j++) {
      EEPROM.get(p2offset + i * sizeof(paint2[i]) + j, k);
      Serial.print(p2offset + i * sizeof(paint2[i]) + j, HEX);
      Serial.print(' ');
      crc.update(k);
    }
    Serial.println();
  }

  uint32_t checksum = crc.finalize();
  Serial.print("CRC=0x");
  Serial.println(checksum, HEX);
#endif

  for (;;);
}
