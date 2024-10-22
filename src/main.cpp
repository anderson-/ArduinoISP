// this sketch turns the Arduino into a AVRISP
// using the following pins:
// 10: slave reset
// 11: MOSI
// 12: MISO
// 13: SCK

// Put an LED (with resistor) on the following pins:
// 9: Heartbeat - shows the programmer is running
// 8: Error - Lights up if something goes wrong (use red if that makes sense)
// 7: Programming - In communication with the slave
//
// October 2009 by David A. Mellis
// - Added support for the read signature command
// 
// February 2009 by Randall Bohn
// - Added support for writing to EEPROM (what took so long?)
// Windows users should consider WinAVR's avrdude instead of the
// avrdude included with Arduino software.
//
// January 2008 by Randall Bohn
// - Thanks to Amplificar for helping me with the STK500 protocol
// - The AVRISP/STK500 (mk I) protocol is used in the arduino bootloader
// - The SPI functions herein were developed for the AVR910_ARD programmer 
// - More information at http://code.google.com/p/mega-isp

#include <Arduino.h>  // defines SS,MOSI,MISO,SCK
#define RESET SS

#define LED_HB 9
#define LED_ERR 8
#define LED_PMODE 7

#define HWVER 2
#define SWMAJ 1
#define SWMIN 18
#define JUMPER_1 16
#define JUMPER_2 17

// STK Definitions
#define STK_OK 0x10
#define STK_FAILED 0x11
#define STK_UNKNOWN 0x12
#define STK_INSYNC 0x14
#define STK_NOSYNC 0x15
#define CRC_EOP 0x20 //ok it is a space...

void pulse(int pin, int times);
void avrisp();
void loop_ISP ();
void loop_serial(void);

boolean serial_mode = true;
void (* loop_f)() = loop_ISP;

void setup() {
  pinMode(7, OUTPUT);
  pulse(7, 2);
  pinMode(8, OUTPUT);
  pulse(8, 2);
  pinMode(9, OUTPUT);
  pulse(9, 2);
  pinMode(13, OUTPUT);
  pinMode(JUMPER_1, OUTPUT);
  //pinMode(JUMPER_2, INPUT);
  
  delay(100);
  
  digitalWrite(JUMPER_1, HIGH);
  uint_fast8_t i;
  for (i = 0; i < 100; i++){
    if (!digitalRead(JUMPER_2)){
      serial_mode = false;
    }
    delay(10);
  }
  digitalWrite(JUMPER_1, LOW);
  for (i = 0; i < 100; i++){
    if (digitalRead(JUMPER_2)){
      serial_mode = false;
    }
    delay(10);
  }
  
  if (serial_mode){
    Serial.begin(9600);
    Serial1.begin(9600);
    digitalWrite(13, HIGH); 
    delay(100);
    digitalWrite(13, LOW);
    delay(100);
    digitalWrite(13, HIGH); 
    delay(100);
    digitalWrite(13, LOW);
    delay(100);
    loop_f = loop_serial;
  } else {
    Serial.begin(19200);
    loop_f = loop_ISP;
    digitalWrite(13, HIGH); 
    delay(500);
    digitalWrite(13, LOW);
    delay(500);    
  }
  
}

int error=0;
int pmode=0;
// address for reading and writing, set by 'U' command
int here;
uint8_t buff[256]; // global block storage

#define beget16(addr) (*addr * 256 + *(addr+1) )
typedef struct param {
  uint8_t devicecode;
  uint8_t revision;
  uint8_t progtype;
  uint8_t parmode;
  uint8_t polling;
  uint8_t selftimed;
  uint8_t lockbytes;
  uint8_t fusebytes;
  int flashpoll;
  int eeprompoll;
  int pagesize;
  int eepromsize;
  int flashsize;
} 
parameter;

parameter param;

// this provides a heartbeat on pin 9, so you can tell the software is running.
uint8_t hbval=128;
int8_t hbdelta=8;
void heartbeat() {
  if (hbval > 192) hbdelta = -hbdelta;
  if (hbval < 32) hbdelta = -hbdelta;
  hbval += hbdelta;
  analogWrite(LED_HB, hbval);
  delay(20); // changed! (for mega)
}

void loop_ISP (){
  // is pmode active?
  if (pmode) digitalWrite(LED_PMODE, HIGH); 
  else digitalWrite(LED_PMODE, LOW);
  // is there an error?
  if (error) digitalWrite(LED_ERR, HIGH); 
  else digitalWrite(LED_ERR, LOW);
  
  // light the heartbeat LED
  heartbeat();
  if (Serial.available()) {
    avrisp();
  }
}

void loop_serial(void) {
  if (Serial1.available()) {
    int fromBreadboardByte = Serial1.read();
    Serial.write(fromBreadboardByte); 
  }
  if (Serial.available()) {
    int toBreadboardByte = Serial.read();
    Serial1.write(toBreadboardByte); 
  }
}

void loop (){
  loop_f(); 
}

uint8_t getch() {
  while(!Serial.available());
  return Serial.read();
}
void readbytes(int n) {
  for (int x = 0; x < n; x++) {
    buff[x] = Serial.read();
  }
}

#define PTIME 30
void pulse(int pin, int times) {
  do {
    digitalWrite(pin, HIGH);
    delay(PTIME);
    digitalWrite(pin, LOW);
    delay(PTIME);
  } 
  while (times--);
}

void spi_init() {
  SPCR = 0x53;
}

void spi_wait() {
  do {
  } 
  while (!(SPSR & (1 << SPIF)));
}

uint8_t spi_send(uint8_t b) {
  uint8_t reply;
  SPDR=b;
  spi_wait();
  reply = SPDR;
  return reply;
}

uint8_t spi_transaction(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
  spi_send(a); 
  spi_send(b);
  spi_send(c);
  return spi_send(d);
}

void empty_reply() {
  if (CRC_EOP == getch()) {
    Serial.print((char)STK_INSYNC);
    Serial.print((char)STK_OK);
  } 
  else {
    Serial.print((char)STK_NOSYNC);
  }
}

void breply(uint8_t b) {
  if (CRC_EOP == getch()) {
    Serial.print((char)STK_INSYNC);
    Serial.print((char)b);
    Serial.print((char)STK_OK);
  } 
  else {
    Serial.print((char)STK_NOSYNC);
  }
}

void get_version(uint8_t c) {
  switch(c) {
  case 0x80:
    breply(HWVER);
    break;
  case 0x81:
    breply(SWMAJ);
    break;
  case 0x82:
    breply(SWMIN);
    break;
  case 0x93:
    breply('S'); // serial programmer
    break;
  default:
    breply(0);
  }
}

void set_parameters() {
  // call this after reading paramter packet into buff[]
  param.devicecode = buff[0];
  param.revision = buff[1];
  param.progtype = buff[2];
  param.parmode = buff[3];
  param.polling = buff[4];
  param.selftimed = buff[5];
  param.lockbytes = buff[6];
  param.fusebytes = buff[7];
  param.flashpoll = buff[8]; 
  // ignore buff[9] (= buff[8])
  //getch(); // discard second value

  // WARNING: not sure about the byte order of the following
  // following are 16 bits (big endian)
  param.eeprompoll = beget16(&buff[10]);
  param.pagesize = beget16(&buff[12]);
  param.eepromsize = beget16(&buff[14]);

  // 32 bits flashsize (big endian)
  param.flashsize = buff[16] * 0x01000000
    + buff[17] * 0x00010000
    + buff[18] * 0x00000100
    + buff[19];

}

void start_pmode() {
  spi_init();
  // following delays may not work on all targets...
  pinMode(RESET, OUTPUT);
  digitalWrite(RESET, HIGH);
  pinMode(SCK, OUTPUT);
  digitalWrite(SCK, LOW);
  delay(50);
  digitalWrite(RESET, LOW);
  delay(50);
  pinMode(MISO, INPUT);
  pinMode(MOSI, OUTPUT);
  spi_transaction(0xAC, 0x53, 0x00, 0x00);
  pmode = 1;
}

void end_pmode() {
  pinMode(MISO, INPUT);
  pinMode(MOSI, INPUT);
  pinMode(SCK, INPUT);
  pinMode(RESET, INPUT);
  pmode = 0;
}

void universal() {
  int w;
  uint8_t ch;

  for (w = 0; w < 4; w++) {
    buff[w] = getch();
  }
  ch = spi_transaction(buff[0], buff[1], buff[2], buff[3]);
  breply(ch);
}

void flash(uint8_t hilo, int addr, uint8_t data) {
  spi_transaction(0x40+8*hilo, 
  addr>>8 & 0xFF, 
  addr & 0xFF,
  data);
}
void commit(int addr) {
  spi_transaction(0x4C, (addr >> 8) & 0xFF, addr & 0xFF, 0);
}

//#define _current_page(x) (here & 0xFFFFE0)
int current_page(int addr) {
  if (param.pagesize == 32) return here & 0xFFFFFFF0;
  if (param.pagesize == 64) return here & 0xFFFFFFE0;
  if (param.pagesize == 128) return here & 0xFFFFFFC0;
  if (param.pagesize == 256) return here & 0xFFFFFF80;
  return here;
}
uint8_t write_flash(int length) {
  if (param.pagesize < 1) return STK_FAILED;
  //if (param.pagesize != 64) return STK_FAILED;
  int page = current_page(here);
  int x = 0;
  while (x < length) {
    if (page != current_page(here)) {
      commit(page);
      page = current_page(here);
    }
    flash(LOW, here, buff[x++]);
    flash(HIGH, here, buff[x++]);
    here++;
  }

  commit(page);

  return STK_OK;
}

uint8_t write_eeprom(int length) {
  // here is a word address, so we use here*2
  // this writes byte-by-byte,
  // page writing may be faster (4 bytes at a time)
  for (int x = 0; x < length; x++) {
    spi_transaction(0xC0, 0x00, here*2+x, buff[x]);
    delay(45);
  } 
  return STK_OK;
}

void program_page() {
  char result = (char) STK_FAILED;
  int length = 256 * getch() + getch();
  if (length > 256) {
    Serial.print((char) STK_FAILED);
    return;
  }
  char memtype = getch();
  for (int x = 0; x < length; x++) {
    buff[x] = getch();
  }
  if (CRC_EOP == getch()) {
    Serial.print((char) STK_INSYNC);
    if (memtype == 'F') result = (char)write_flash(length);
    if (memtype == 'E') result = (char)write_eeprom(length);
    Serial.print(result);
  } 
  else {
    Serial.print((char) STK_NOSYNC);
  }
}
uint8_t flash_read(uint8_t hilo, int addr) {
  return spi_transaction(0x20 + hilo * 8,
  (addr >> 8) & 0xFF,
  addr & 0xFF,
  0);
}

char flash_read_page(int length) {
  for (int x = 0; x < length; x+=2) {
    uint8_t low = flash_read(LOW, here);
    Serial.print((char) low);
    uint8_t high = flash_read(HIGH, here);
    Serial.print((char) high);
    here++;
  }
  return STK_OK;
}

char eeprom_read_page(int length) {
  // here again we have a word address
  for (int x = 0; x < length; x++) {
    uint8_t ee = spi_transaction(0xA0, 0x00, here*2+x, 0xFF);
    Serial.print((char) ee);
  }
  return STK_OK;
}

void read_page() {
  char result = (char)STK_FAILED;
  int length = 256 * getch() + getch();
  char memtype = getch();
  if (CRC_EOP != getch()) {
    Serial.print((char) STK_NOSYNC);
    return;
  }
  Serial.print((char) STK_INSYNC);
  if (memtype == 'F') result = flash_read_page(length);
  if (memtype == 'E') result = eeprom_read_page(length);
  Serial.print(result);
  return;
}

void read_signature() {
  if (CRC_EOP != getch()) {
    Serial.print((char) STK_NOSYNC);
    return;
  }
  Serial.print((char) STK_INSYNC);
  uint8_t high = spi_transaction(0x30, 0x00, 0x00, 0x00);
  Serial.print((char) high);
  uint8_t middle = spi_transaction(0x30, 0x00, 0x01, 0x00);
  Serial.print((char) middle);
  uint8_t low = spi_transaction(0x30, 0x00, 0x02, 0x00);
  Serial.print((char) low);
  Serial.print((char) STK_OK);
}
//////////////////////////////////////////
//////////////////////////////////////////


////////////////////////////////////
////////////////////////////////////
void avrisp() { 
  // uint8_t data, low, high;
  uint8_t ch = getch();
  switch (ch) {
  case '0': // signon
    empty_reply();
    break;
  case '1':
    if (getch() == CRC_EOP) {
      Serial.print((char) STK_INSYNC);
      Serial.print("AVR ISP");
      Serial.print((char) STK_OK);
    }
    break;
  case 'A':
    get_version(getch());
    break;
  case 'B':
    readbytes(20);
    set_parameters();
    empty_reply();
    break;
  case 'E': // extended parameters - ignore for now
    readbytes(5);
    empty_reply();
    break;

  case 'P':
    start_pmode();
    empty_reply();
    break;
  case 'U':
    here = getch() + 256 * getch();
    empty_reply();
    break;

  case 0x60: //STK_PROG_FLASH
    getch(); // low = getch();
    getch(); // high = getch();
    empty_reply();
    break;
  case 0x61: //STK_PROG_DATA
    getch(); // data = getch();
    empty_reply();
    break;

  case 0x64: //STK_PROG_PAGE
    program_page();
    break;

  case 0x74: //STK_READ_PAGE
    read_page();    
    break;

  case 'V':
    universal();
    break;
  case 'Q':
    error=0;
    end_pmode();
    empty_reply();
    break;

  case 0x75: //STK_READ_SIGN
    read_signature();
    break;

    // expecting a command, not CRC_EOP
    // this is how we can get back in sync
  case CRC_EOP:
    Serial.print((char) STK_NOSYNC);
    break;

    // anything else we will return STK_UNKNOWN
  default:
    if (CRC_EOP == getch()) 
      Serial.print((char)STK_UNKNOWN);
    else
      Serial.print((char)STK_NOSYNC);
  }
}

