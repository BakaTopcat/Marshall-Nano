//ver 3: eeprom feature included
//12.01.2021: AltSoftSerial introduced, D7 connected to DE+RE
//ver 4: 15.01.2021 menu/paint mode introduced
//16.01.2021 Adafruit changed to SSD1306Ascii
//21.01.2021: EEPROM CRC check introduced

#include <EEPROM.h>
#include <CRC32.h>

#include "SSD1306Ascii.h"
#include "SSD1306AsciiAvrI2c.h"
#include "topcat.h"
#define I2C_ADDRESS 0x3C
SSD1306AsciiAvrI2c oled;

/* AltSoftSerial
  Nano:
  Receive: 8 (D8) ICP1
  Transmit: 9 (D9) OC1A
*/
/* MAX485 connection:
   TX: DI >D9
   RX: RO >D8
   DE+RE: >D7, usually HIGH
   A: + (straight)
   B: - (inverse)
*/
#include <AltSoftSerial.h>
AltSoftSerial mySerial;
#define TXEN 7  //D7 pin

/* VISCA structures */
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

//32 bytes total too
struct adjust {
  char nam[10];
  byte len; //max 10
  byte cmd[10];
  byte hba1; //first half-byte position of adjustment variable, e.g command 01 00 0p > half byte position is 5.
  byte hba2; //if no second half-byte, it should be 0xFF (0x00 is fine too)
  byte mi; //minimum adjustment value
  byte ma; //maximum adjustment value
  byte last[7]; //last values for each 0-7 camera
};

//total paint1 and paint2 records
#define TOT_P1 15
#define TOT_P2 6
//records' offsets in EEPROM
#define P1OFFSET 0x020
#define P2OFFSET 0x200

//let's initialize the menu VISCA commands
const byte arMenu[4] = {0x01, 0x06, 0x06, 0x02};
const byte arUp[4]   = {0x01, 0x06, 0x06, 0x11};
const byte arDown[4] = {0x01, 0x06, 0x06, 0x12};
const byte arLeft[4] = {0x01, 0x06, 0x06, 0x14};
const byte arRight[4] = {0x01, 0x06, 0x06, 0x18};

// Encoder
#define ENC_S1 4 //D4
#define ENC_S2 5 //D5
#define ENC_SW 6 //D6
#include "encMinim.h"
// пин clk, пин dt, пин sw, направление (0/1)
encMinim enc(ENC_S1, ENC_S2, ENC_SW, 1);

#define DEF_CAM 1; //default cam
byte camnum = DEF_CAM;            //camera number. valid values 1 to 7
byte prevnum = 0;

bool led_state = false;

bool paint_mode = false; //true - paint mode, false - menu mode
bool prev_paint_mode = !paint_mode;

void setup() {
#define TOT_BAUDS 6
#define DEF_BAUDRATE 3  //default baudrate
  const long baudrates[TOT_BAUDS] = {9600, 14400, 19200, 38400, 57600, 115200};
  int8_t baudrate = DEF_BAUDRATE; //38400 in array above
  int8_t prevbaud = -1; //sometimes it drops to -1 so we need the signed type
#define UNST_BAUD 5   //everything equal or above this baudrate will be considered as unstable and warning will display

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(TXEN, OUTPUT);
  digitalWrite(TXEN, HIGH); //enable TX

  //display setup
  oled.begin(&Adafruit128x64, I2C_ADDRESS);

  oled.setFont(topcat); //  oled.setFont(fixed_bold10x15);
  //initial logo
  oled.clear();
  oled.println(F("M A R S H A L L\ntiny RCP\nby Kirill Ageyev\n(c) 2021"));
  delay(1000);

  //let's read the EEPROM values
  //camnum occupies address 0x00
  //baudrate occupies address 0x01
  EEPROM.get(0x00, camnum);
  if ((camnum < 1) or (camnum > 7)) {
    camnum = DEF_CAM;
  }
  EEPROM.get(0x01, baudrate);
  if ((baudrate < 0) or (baudrate > TOT_BAUDS - 1)) {
    baudrate = DEF_BAUDRATE;
  }

  //camera selection
  oled.clear(); oled.set1X();
  oled.println(F("Camera:"));
  do {
    enc.tick();

    if (enc.isLeft()) camnum--;
    if (enc.isRight()) camnum++;
    if (camnum < 1) camnum = 7;
    if (camnum > 7) camnum = 1;

    //camnum changed, displaying routine
    if (camnum != prevnum) {
      oled.setCursor(0, 2);  oled.set2X();
      oled.print(camnum);
    }

    //remembering the camera number to check whether it's changed in next iteration
    prevnum = camnum;
  } while (enc.isClick() == false);

  //baud rate selection
  oled.clear();  oled.set1X();
  oled.println(F("Baud rate:"));
  do {
    enc.tick();

    if (enc.isLeft()) baudrate--;
    if (enc.isRight()) baudrate++;
    if (baudrate < 0) baudrate = TOT_BAUDS - 1;
    if (baudrate > TOT_BAUDS - 1) baudrate = 0;

    //baudrate changed, displaying routine
    if (baudrate != prevbaud) {
      oled.setCursor(0, 2);  oled.set2X();
      oled.print(baudrates[baudrate]);
      oled.print(F("   ")); //extra spaces for overwriting the shorter values on screen

      //unstable baudrate display routine
      oled.set1X();  oled.setCursor(0, 6);
      oled.print(baudrate >= UNST_BAUD ? F("unstable") : F("        "));
    }

    //remembering the baudrate to check whether it's changed in next iteration
    prevbaud = baudrate;
  } while (enc.isClick() == false);

  //RCP mode selection
  oled.clear();  oled.set1X();
  oled.println(F("RCP mode:"));

  do {
    enc.tick();
    if (enc.isLeft()) paint_mode = false;
    if (enc.isRight()) paint_mode = true;

    //mode changed, displaying routine
    if (paint_mode != prev_paint_mode) {
      oled.setCursor(0, 2);  oled.set2X();
      oled.print(paint_mode ? F("PAINT") : F("MENU "));
    }
    //remembering the mode to check whether it's changed in next iteration
    prev_paint_mode = paint_mode;
  } while (enc.isClick() == false);

  if (!paint_mode)
  { //MENU mode, displaying routine

    //okay, we've got the baud rate and camera#, now it's time to display some inctructions and proceed to loop cycle

    /*    //max string length in text size 1 is 21 symbol
        oled.clear();  oled.set1X();  oled.setFont(Adafruit5x7);
        oled.print(F("Camera #: ")); oled.println(camnum);
        oled.print(F("Baud rate: ")); oled.println(baudrates[baudrate]);

        /* 0x18: arrow up
           0x19: arrow down
           0x1A: arrow right
           0x1B: arrow left */
    /*   oled.println(F("Turn to move\n  Up and Down \nHold and turn to move\n  Left and Right\nClick to select"));
    */
    //max string length 16, lines 4
    oled.clear();  oled.set1X();  //oled.setFont(topcat);
    oled.print(F("c#:")); oled.print(camnum);
    oled.print(F(" baud:")); oled.println(baudrates[baudrate]);

    oled.println(F("Turn to UP\x12\DOWN\nHold+turn to L\x1DR\nClick to select"));

  } else { //PAINT mode, display prepare
    oled.clear();  oled.set1X();  //oled.setFont(topcat);//oled.setFont(fixed_bold10x15);

    //EEPROM CRC check routine
#define UPDOWN_MEANINGFUL_BYTES 22 //all bytes over this value are meaningless and CRC check is omitted
#define ADJUST_MEANINGFUL_BYTES 25

    CRC32 crc;
    crc.reset();
    byte i, j, k;
    for (i = 0; i < TOT_P1; i++) {
      for (j = 0; j <  UPDOWN_MEANINGFUL_BYTES; j++) {
        EEPROM.get(P1OFFSET + i * 32 + j, k);
        crc.update(k);
      }
    }
    for (i = 0; i < TOT_P2; i++) {
      for (j = 0; j <  ADJUST_MEANINGFUL_BYTES; j++) {
        EEPROM.get(P2OFFSET + i * 32 + j, k);
        crc.update(k);
      }
    }
    uint32_t checksum = crc.finalize();
    if (checksum != 0xA16B89FE) {
      oled.println("EEPROM CRC\ncheck fail\ncall\n+38 067 6902542");
      for (;;);
    }
    //end of EEPROM CRC check routine
  }
  EEPROM.update(0x00, camnum);
  EEPROM.update(0x01, baudrate);

  //serial setup
  Serial.begin(baudrates[baudrate]);
  mySerial.begin(baudrates[baudrate]);
} //of setup()


int8_t curr_paint = 0;
int8_t prev_paint = -1;
bool selection = true; //selection or adjusting
int8_t curr_value = 0;
int8_t prev_value = -1;

updown p1;
adjust p2;

void loop() {
  enc.tick();

  if (!paint_mode) {
    //MENU MODE
    if (enc.isLeft()) cmd(camnum, arUp, 4);
    if (enc.isRight()) cmd(camnum, arDown, 4);
    if (enc.isLeftH()) cmd(camnum, arLeft, 4);
    if (enc.isRightH()) cmd(camnum, arRight, 4);
    if (enc.isClick()) cmd(camnum, arMenu, 4);
  } else {
    //PAINT MODE
    if (enc.isClick()) selection = !selection; //toggle selection <> adjusting

    if (selection) { //selection mode
      oled.invertDisplay(false);
      prev_value = -1;//this need to refresh the adjustment mode next time, only once

      if (enc.isLeft()) curr_paint--;
      if (enc.isRight()) curr_paint++;
      if (curr_paint < 0) curr_paint = TOT_P1 + TOT_P2 - 1;
      if (curr_paint > TOT_P1 + TOT_P2 - 1) curr_paint = 0;

      //paint changed, getting from EEPROM and displaying routine
      if (curr_paint != prev_paint) {
        oled.clear(); oled.set1X();
        if (curr_paint < TOT_P1) { //it's paint1 (updown)
          EEPROM.get(P1OFFSET + curr_paint * 32, p1); //32 bytes = size of paint record in EEPROM
          oled.print(p1.nam);
          oled.set2X();  oled.setCursor(0, 2);
          oled.print(p1.mode == UD ? F("Up-Down") : F("On-Off"));
        } else { //it's paint2 (adjust)
          EEPROM.get(P1OFFSET + curr_paint * 32, p2);
          oled.print(p2.nam);
          oled.set2X();  oled.setCursor(0, 2);
          oled.println(p2.last[camnum - 1]); //'last' array is 0...6 while camnum is 1...7
          curr_value = p2.last[camnum - 1];
          oled.set1X();
          oled.print("Cam #"); oled.print(camnum);
        }
        prev_paint = curr_paint;
      }
    } else { //adjustment mode
      oled.invertDisplay(true);
      prev_paint = -1; //this need to refresh the selection mode next time, only once

      if (enc.isLeft()) {
        curr_value--;
        if (curr_paint < TOT_P1) { //it's paint1 (updown)
          cmd(camnum, p1.down, p1.len);
        }
      }
      if (enc.isRight()) {
        curr_value++;
        if (curr_paint < TOT_P1) { //it's paint1 (updown)
          cmd(camnum, p1.up, p1.len);
        }
      }

      if (curr_paint >= TOT_P1) { //it's paint2 (adjust)
        if (curr_value < p2.mi) curr_value = p2.mi;
        if (curr_value > p2.ma) curr_value = p2.ma;

        //value changed, displaying/saving/command routine
        if (curr_value != prev_value) {
          oled.set2X();  oled.setCursor(0, 2);
          oled.print(curr_value); oled.clearToEOL();
          //value printed, now let's save it to EEPROM
          p2.last[camnum - 1] = curr_value;   //'last' array is 0...6 while camnum is 1...7
          EEPROM.put(P1OFFSET + curr_paint * 32, p2);
          //last value saved, now let's add some 'p' and process a command
          byte pcmd[10];
          memcpy(pcmd, p2.cmd, p2.len);

          //hba2: lower half-byte
          if ((p2.hba2 % 2) == 0) { //if hba2 is even: put to left halfbyte
            pcmd[p2.hba2 / 2] &= B00001111; //ones mean bits here untouched, zeros clear bits and we put a value there
            pcmd[p2.hba2 / 2] += curr_value * 16;  //shifted the value to left and added
          } else { //hba2 is odd: put to right halfbyte
            pcmd[p2.hba2 / 2] &= B11110000; //ones mean bits here untouched, zeros clear bits and we put a value there
            pcmd[p2.hba2 / 2] += (curr_value & B00001111);  //removed the upper half-byte of curr_value
          }

          //hba1: upper half-byte
          if (p2.hba1 != 0xFF)
            if ((p2.hba1 % 2) == 0) { //if hba1 is even: put to left halfbyte
              pcmd[p2.hba1 / 2] &= B00001111; //ones mean bits here untouched, zeros clear bits and we put a value there
              pcmd[p2.hba1 / 2] += (curr_value & B11110000); //removed the lower half-byte of curr_value
            } else { //hba1 is odd: put to right halfbyte
              pcmd[p2.hba1 / 2] &= B11110000; //ones mean bits here untouched, zeros clear bits and we put a value there
              pcmd[p2.hba1 / 2] += (curr_value / 16); //aka >> 4, shifted the value to right and added
            }

          cmd(camnum, pcmd, p2.len);

          prev_value = curr_value;
        }
      }
    } //of adjustment mode
  } //of PAINT MODE
} //of loop()

#define DEBUG 0

//executing the VISCA command
void cmd(byte cam, byte str[], byte len) {
#if (DEBUG == 1)
  Serial.write(0x80 + cam);
  Serial.write(str, len);
  Serial.write(0xFF);
#else
  mySerial.write(0x80 + cam);
  mySerial.write(str, len);
  mySerial.write(0xFF);
#endif

  //toggle built-in led
  digitalWrite(LED_BUILTIN, (led_state) ? HIGH : LOW);
  led_state = !led_state;
}
