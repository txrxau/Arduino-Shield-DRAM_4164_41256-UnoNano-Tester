//This code is based on the DRAMarduino project: https://forum.defence-force.org/viewtopic.php?t=1699
//Is for testing 4164 and 41246 memory chips.
//It has been expanded with the help of Copilot and tested with an Arduino Uno R4 Wifi, but it should work with others, taking the pins into account. See the electronics side of the project.
//I focused on this project because it's very simple, inexpensive, and just what I needed for the memory chips I wanted to test. 
//With it I have tested MB8264, MN4164, MB81256, MN41256 and D41256 chips.
//The tests included are:
//• Fill: Fills cells with zeros, ones, or a specific pattern. This is more or less the original test, but chips with some problem may pass this test but fail one or all of the following. This is why the original project is quite limited, unless the tests are expanded.
//• Checkerboard: Alternating zeros and ones.
//• Retention: Writes to memory and reads the data after a timeout.
//• Inversion: Checks for bits that change their value after being written.
//• WalkingBits: Checks for stuck cells.
//• MATS++, MarchC, MarchA: These three tests expand the scope to include:
//- Stuck-at errors (cell always at 0 or 1)
//- Coupling faults
//- Transition faults (cell does not transition correctly from 0 to 1)
//- Addressing faults
//- Interference faults between neighboring cells
//- Conditional access faults (reading or writing fails depending on the state of other cells)
//• Address Aliasing: Check if different row and column combinations accidentally access the same memory cell, indicating a fault in the address logic. However, this test still needs further work (made faster) and is disabled.
//The code requires the digitalWrite library. Install it into the IDE.
//After turning on the Arduino, or changing the chips, press the Reset key once to avoid possible errors.
//Sometimes, a single error in $0 may immediately appear at the start of the first test. Reset until the tests start correctly, unless it's a real error.
//The code allows you to run a given sequence of tests (you need to modify and recompile the code) or use a menu via the terminal (tested in the Arduino IDE) at 9600. The terminal displays the running tests and their results. It is recommended to use the terminal console.
//During the tests, the green LED keep flashing. At the end of a partial or full test, both flash. After all the tests, if everything is OK, the green LED remains lit; if there is at least one error, the red LED remains lit.
//If during the tests there are more than 100 errors, the remaining tests are canceled.

//Please, take a look to the readAddress function
//There are unused or commented-out parts of the code; this is a WiP.


//#include <digitalWriteFast.h>
//#include "fastDigital.h"

//serial terminal at 9600

// Pines de dirección y control
#define BUS_SIZE     9
#define MAX_ERRORS   100

#define XA0  18
#define XA1   2
#define XA2  19
#define XA3   6
#define XA4   5
#define XA5   4
#define XA6   7
#define XA7   3
#define XA8  14

#define DI   15
#define DO    8
#define WE   16
#define RAS  17
#define CAS   9

#define M_TYPE 10
#define R_LED  11
#define G_LED  12

uint8_t T_RAS = 1;     // RAS-to-CAS delay (μs)
uint8_t T_CAS = 1;     // CAS access delay (μs)
uint8_t T_WRITE = 1;   // Minimum writing pulse (μs)

volatile uint16_t err_cnt;
volatile char bus_size;

static inline void digitalWrite(uint8_t pin, uint8_t val) __attribute__((always_inline, unused));

const uint8_t a_bus[BUS_SIZE] = {
  XA0, XA1, XA2, XA3, XA4, XA5, XA6, XA7, XA8
};

void printProgressBar(uint16_t current, uint16_t total) {
  const int barWidth = 20; // Adjust the bar size here
  int progress = (current * barWidth) / total;

  Serial.print("[");
  for (int i = 0; i < barWidth; i++) {
    if (i < progress) Serial.print("=");
    else Serial.print(" ");
  }
  Serial.print("] ");
  Serial.print((current * 100) / total);
  Serial.println("%");
}

inline void setBus(uint16_t a) {
  for (int i = 0; i < BUS_SIZE; i++) {
    digitalWrite(a_bus[i], (a >> i) & 1);
  }
}

inline void writeAddress(uint16_t r, uint16_t c, uint8_t v) {
  setBus(r);
  digitalWrite(RAS, LOW);
  digitalWrite(WE, LOW);
  digitalWrite(DI, v & 1 ? HIGH : LOW);
  setBus(c);
  digitalWrite(CAS, LOW);
  digitalWrite(RAS, HIGH);
  digitalWrite(CAS, HIGH);
  digitalWrite(WE, HIGH);
}


inline uint8_t readAddress(uint16_t r, uint16_t c) {
  setBus(r);
  digitalWrite(RAS, LOW);
  setBus(c);
  digitalWrite(CAS, LOW);

  //The delay time have a nuance. The code works without it.
  //There is an inherent delay while toggling one GPIO to Write or Read
  //Even the Fast version, which is dozens of times faster.
  //The delay can be on the order of 250ns on a 16MHz Arduino Nano with FastWrite/Read
  //The Arduino Uno R4 WiFi (48 MHz) pin toggle time with FastWrite/Read is around ~180ns, and with some register tricks, can be lowered to ~80ns
  //Some non-Arduino dev boards manage as low as 20ns
  //Do as you consider here
  
  //delayMicroseconds(T_CAS);
  
  //NOP takes 1 clock cycle. In the Arduinos @16MHz that is 62.5ns
  //__asm__("nop\n\t"); 
  //__asm__("nop\n\t");
  //__asm__("nop\n\t");

  uint8_t ret = digitalRead(DO);
  digitalWrite(RAS, HIGH);
  digitalWrite(CAS, HIGH);
  return ret;
}

uint8_t error(int r, int c) {
  unsigned long a = ((unsigned long)r << bus_size) + c;
  digitalWrite(R_LED, LOW);
  //interrupts();
  Serial.print(" FAILED $");
  Serial.println(a, HEX);
  err_cnt += 1;
  //Serial.flush();
  return (err_cnt >= MAX_ERRORS);
}

void ok() {
  interrupts();
  Serial.println();
  //Serial.print("Errors: "); Serial.println(err_cnt);
  if (err_cnt == 0) 
  {
    digitalWrite(R_LED, HIGH);
    digitalWrite(G_LED, LOW);
    Serial.print("Errors: "); Serial.println(err_cnt);
    Serial.println();
    Serial.println("DRAM OK");
  } 
  else if (err_cnt <= 50)
  {
    digitalWrite(R_LED, LOW);
    digitalWrite(G_LED, LOW);
    Serial.print("Errors: "); Serial.println(err_cnt);
      Serial.println();
      Serial.println(" ERRORS FOUND. DRAM MAY WORK");
  }
  else 
  {
    digitalWrite(R_LED, LOW);
    digitalWrite(G_LED, HIGH);
    if (err_cnt >= MAX_ERRORS)
      Serial.println();
      Serial.println(" FAILED. TOO MANY ERRORS");
      Serial.println(" ERROR THRESHOLD EXCEEDED");
  }

  for (int i = 0; i < BUS_SIZE; i++)
    pinMode(a_bus[i], INPUT);

  pinMode(CAS, INPUT_PULLUP);
  pinMode(RAS, INPUT_PULLUP);
  pinMode(WE, INPUT_PULLUP);
  pinMode(DI, INPUT);
  Serial.flush();
}

inline void blink() {
  digitalWrite(G_LED, LOW);
  digitalWrite(R_LED, LOW);
  delay(500);
  digitalWrite(R_LED, HIGH);
  digitalWrite(G_LED, HIGH);
}

inline void green(int v) 
{
  blinkGreenActivity(v);
  //digitalWrite(G_LED, v);
  //delay(50);
}

uint8_t ledstate = LOW;
unsigned long lastToggle = 0;
inline void blinkGreenActivity(int ledState) {
  unsigned long now = millis();
  if (now - lastToggle > 50) 
  {
    ledstate = !ledstate;
    digitalWrite(G_LED, ledState);
    lastToggle = now;
  }
}

inline void fill(uint8_t v) {
  uint16_t r, c;
  uint16_t max_r = 1 << bus_size;
  uint16_t max_c = 1 << bus_size;
  v &= 1;

  int g = 0;

  if (err_cnt >= MAX_ERRORS) return;

  for (r = 0; r < max_r; r++) 
  {
    //printProgressBar(r + 1, max_r);
    green(g? LOW : HIGH);
    for (c = 0; c < max_c; c++) {
      writeAddress(r, c, v);
      if (readAddress(r, c) != v)
        if (error(r, c)) return;
    }
    g ^= 1;
  }
  blink();
}

inline void fillx(uint8_t pattern) {
  if (err_cnt >= MAX_ERRORS) return;
  uint16_t r, c;
  uint8_t v;
  uint16_t max_r = 1 << bus_size;
  uint16_t max_c = 1 << bus_size;
  int g = 0;
  for (r = 0; r < max_r; r++) 
  {
    //printProgressBar(r + 1, max_r);
    green(g? LOW : HIGH);
    uint8_t p = pattern;
    for (c = 0; c < max_c; c++) 
    {
      v = p & 1;
      writeAddress(r, c, v);
      p = (p >> 1) | (v << 7);
    }
    g ^= 1;
  }
  blink();
}

inline void readx(uint8_t pattern) 
{
  if (err_cnt >= MAX_ERRORS) return;
  uint16_t r, c;
  uint8_t v;
  uint16_t max_r = 1 << bus_size;
  uint16_t max_c = 1 << bus_size;
  int g = 0;
  for (r = 0; r < max_r; r++) 
  {
    green(g? LOW : HIGH);
    //printProgressBar(r + 1, max_r);
    uint8_t p = pattern;
    for (c = 0; c < max_c; c++) 
    {
      v = p & 1;
      if (readAddress(r, c) != v)
        if (error(r, c)) return;
      p = (p >> 1) | (v << 7);
    }
    g ^= 1;
  }
  blink();
}

//DATA filling
void testFill() 
{
  Serial.println("Fill test:");
  Serial.println("  Fill 1111"); fill(0b11111111);
  Serial.println("  Fill 0000"); fill(0b00000000);
//  Serial.println("...fillx 1010"); fillx(0b10101010);
//  Serial.println("...fillx 0101"); fillx(0b01010101);
}

//Checkerboard
void testCheckerboard() 
{
  Serial.println("Checkerboard test:");

  Serial.println("  Checkerboard 1010");
  fillx(0b10101010); readx(0b10101010);
  Serial.println("  Checkerboard 0101");
  fillx(0b01010101); readx(0b01010101);
  Serial.println("  Checkerboard 1001");
  fillx(0b10011001); readx(0b10011001);
  Serial.println("  Checkerboard 0110");
  fillx(0b01100110); readx(0b01100110);
}

//Retention test
void testRetention() 
{
  Serial.println("Retention test 1000ms delay:");

  Serial.println("  Retention 1010");
  fillx(0b10101010);
  delay(1000);
  readx(0b10101010);
  Serial.println("  Retention 0101");
  fillx(0b01010101);
  delay(1000);
  readx(0b01010101);
}

//Inversion test
void testInversion() 
{
  Serial.println("Inversion test:");
  Serial.println("  Inversion 1/3");
  fillx(0b11001100);
  readx(0b11001100);
  Serial.println("  Inversion 2/3");
  fillx(0b01100110);
  readx(0b01100110);
  Serial.println("  Inversion 3/3");
  fillx(0b00110011);
  readx(0b00110011);
}

//Address aliasing (sloooowwww)
void testAddressAliasing() 
{
  Serial.println("Address aliasing test");
  uint16_t max_r = 1 << bus_size;
  uint16_t max_c = 1 << bus_size;
  for (uint16_t r = 0; r < max_r; r++) {
    for (uint16_t c = 0; c < max_c; c++) {
      fill(0);
      writeAddress(r, c, 1);
      for (uint16_t r2 = 0; r2 < max_r; r2++) {
        for (uint16_t c2 = 0; c2 < max_c; c2++) {
          if ((r2 != r || c2 != c) && readAddress(r2, c2) != 0) {
            error(r2, c2);
            return;
          }
        }
      }
    }
  }
  blink();
}

//Walking bits
void testWalkingBits() 
{
  Serial.println("Walking bits test:");
  for (uint8_t i = 0; i < 8; i++)
   {
    uint8_t pattern = 1 << i;
    Serial.println("  Walking Bits 1/2");
    fillx(pattern);
    readx(pattern);
    Serial.println("  Walking Bits 2/2");
    pattern = ~(1 << i);
    fillx(pattern);
    readx(pattern);
  return;
  }
}

//Modified Algorithmic Test Sequence
inline void testMATSpp() {
  Serial.println("MATS++ test...");
  const uint16_t max = 1 << bus_size;
  int g = 0;
  // Phase 1: Write 0
  for (uint16_t r = 0; r < max; r++) {
    green(g? HIGH : LOW);
    for (uint16_t c = 0; c < max; c++) {
      writeAddress(r, c, 0);
    }
    g ^= 1;
  }
  // Phase 2: Read 0, write 1
  for (uint16_t r = 0; r < max; r++) {
    green(g? HIGH : LOW);
    for (uint16_t c = 0; c < max; c++) {
      if (readAddress(r, c) != 0) if (error(r, c)) return;
      writeAddress(r, c, 1);
    }
    g ^= 1;
  }
  // Phase 3: Read 1, write 0
  for (int r = max - 1; r >= 0; r--) {
    green(g? HIGH : LOW);
    for (int c = max - 1; c >= 0; c--) {
      if (readAddress(r, c) != 1) if (error(r, c)) return;
      writeAddress(r, c, 0);
    }
    g ^= 1;
  }
  blink();
}

inline void testMarchC() {
  Serial.println("March C test...");
  const uint16_t max = 1 << bus_size;
  int g = 0;
  // Phase 1: Write 0
  for (uint16_t r = 0; r < max; r++) {
    green(g? HIGH : LOW);
    for (uint16_t c = 0; c < max; c++) {
      writeAddress(r, c, 0);
    }
    g ^= 1;
  }
  // Phase 2: Ascending – read 0, write 1, read 1
  for (uint16_t r = 0; r < max; r++) {
    green(g? HIGH : LOW);
    for (uint16_t c = 0; c < max; c++) {
      if (readAddress(r, c) != 0) if (error(r, c)) return;
      writeAddress(r, c, 1);
      if (readAddress(r, c) != 1) if (error(r, c)) return;
    }
    g ^= 1;
  }
  // Phase 3: Descending – read 1, write 0, read 0
  for (int r = max - 1; r >= 0; r--) {
    green(g? HIGH : LOW);
    for (int c = max - 1; c >= 0; c--) {
      if (readAddress(r, c) != 1) if (error(r, c)) return;
      writeAddress(r, c, 0);
      if (readAddress(r, c) != 0) if (error(r, c)) return;
    }
    g ^= 1;
  }
  blink();
}

inline void testMarchA() {
  Serial.println("March A test...");
  const uint16_t max = 1 << bus_size;
  int g = 0;
  // Phase 1: Write 0
  for (uint16_t r = 0; r < max; r++) {
    green(g? HIGH : LOW);
    for (uint16_t c = 0; c < max; c++) {
      writeAddress(r, c, 0);
    }
    g ^= 1;
  }
  // Phase 2: Ascending – read 0, write 1
  for (uint16_t r = 0; r < max; r++) {
    green(g? HIGH : LOW);
    for (uint16_t c = 0; c < max; c++) {
      if (readAddress(r, c) != 0) if (error(r, c)) return;
      writeAddress(r, c, 1);
    }
    g ^= 1;
  }
  // Phase 3: Ascending – read 1
  for (uint16_t r = 0; r < max; r++) {
    green(g? HIGH : LOW);
    for (uint16_t c = 0; c < max; c++) {
      if (readAddress(r, c) != 1) if (error(r, c)) return;
    }
    g ^= 1;
  }
  // Phase 4: Descending – read 1, write 0, read 0
  for (int r = max - 1; r >= 0; r--) {
	green(g? HIGH : LOW);
    for (int c = max - 1; c >= 0; c--) {
      if (readAddress(r, c) != 1) if (error(r, c)) return;
      writeAddress(r, c, 0);
      if (readAddress(r, c) != 0) if (error(r, c)) return;
    }
    g ^= 1;
  }
  blink();
}

inline void testMarchAv2() {
  Serial.println("March A test...");
  int g = 0;
  uint16_t max_r = 1 << bus_size;
  uint16_t max_c = 1 << bus_size;
  // Phase 1 ascending: Write 0
  for (uint16_t r = 0; r < max_r; r++) {
	green(g? HIGH : LOW);
    for (uint16_t c = 0; c < max_c; c++) {
      writeAddress(r, c, 0);
    }
	g ^= 1;
  }
  // Phase 2 ascending: Read 0, Write 1
  for (uint16_t r = 0; r < max_r; r++) {
	green(g? HIGH : LOW);
    for (uint16_t c = 0; c < max_c; c++) {
      if (readAddress(r, c) != 0) {
        error(r, c); return;
      }
      writeAddress(r, c, 1);
    }
	g ^= 1;
  }
  // Phase 3 descending: Read 1, Write 0
  for (int16_t r = max_r - 1; r >= 0; r--) {
	green(g? HIGH : LOW);
    for (int16_t c = max_c - 1; c >= 0; c--) {
      if (readAddress(r, c) != 1) {
        error(r, c); return;
      }
      writeAddress(r, c, 0);
    }
	g ^= 1;
  }
  // Pase 4 descending : Read 0, Write 1
  for (int16_t r = max_r - 1; r >= 0; r--) {
	green(g? HIGH : LOW);
    for (int16_t c = max_c - 1; c >= 0; c--) {
      if (readAddress(r, c) != 0) {
        error(r, c); return;
      }
      writeAddress(r, c, 1);
    }
	g ^= 1;
  }
  // Phase 5 ascending: Read 1
  for (uint16_t r = 0; r < max_r; r++) {
	green(g? HIGH : LOW);
    for (uint16_t c = 0; c < max_c; c++) {
      if (readAddress(r, c) != 1) {
        error(r, c); return;
      }
    }
	g ^= 1;
  }
  blink();
}

void testRandomAccess(uint16_t numTests) {
  Serial.println("Random access...");
  uint16_t max_r = 1 << bus_size;
  uint16_t max_c = 1 << bus_size;
  uint16_t r, c;
  byte v;
  int g = 0;
  // Initialize pseudorandom seed
  randomSeed(millis());  // Ensures variability between executions
  for (uint16_t i = 0; i < numTests; i++) {
    green(g? HIGH : LOW);
    // Random address and data
    r = random(0, max_r);
    c = random(0, max_c);
    v = random(0, 2);  // Solo 0 o 1
    writeAddress(r, c, v);
    byte r_val = readAddress(r, c);
    if (r_val != v) {
      error(r, c);
      return;
    }
    g ^= 1;
  }
  blink();
}

void testIntegrityFast() {
  Serial.println();
  Serial.println("Faster integrity test");
  uint16_t max = 1 << bus_size;
  uint8_t pattern1 = 0b10101010;
  uint8_t pattern2 = 0b01010101;
  uint8_t pattern3 = 0b11001100;
  uint8_t pattern4 = 0b00110011;
  // Basic writing/reading test
  Serial.println("Basic 0s...");
  fill(0);
  Serial.println("Basic 1s...");
  fill(1);
  // Checkerboard
  Serial.println("Checkerboard 1...");
  fillx(pattern1);
  readx(pattern1);
  Serial.println("Checkerboard 2...");
  fillx(pattern2);
  readx(pattern2);
  // Minimum retention
  Serial.println("Retention, faster (200ms)...");
  fillx(pattern4);
  delay(200); // Very short retention
  readx(pattern4);
  // Quick investment
  Serial.println("Inversion...");
  fillx(pattern3);
  fillx(~pattern3);
  readx(~pattern3);
  // Walking bits compacto
  Serial.println("Walking Bits, faster...");
  for (uint8_t i = 0; i < 4; i++) {
    uint8_t pat = 1 << (i * 2);      // Salta posiciones para acelerar
    fillx(pat);
    readx(pat);
    fillx(~pat);
    readx(~pat);
  }
  // Walking bits compacto
  Serial.println("Random access, faster...");
  int t = bus_size == BUS_SIZE ? 262144/2:64535/2;
  testRandomAccess(t);
  // Aliasing simplificado
  //Serial.println("Aliasing partial, faster...");
  //for (uint16_t r = 0; r < max; r += max / 8) {
  //  for (uint16_t c = 0; c < max; c += max / 8) {
  //    fill(0);
  //    writeAddress(r, c, 1);
  //    for (uint16_t r2 = r; r2 < r + max / 16; r2++) {
  //      for (uint16_t c2 = c; c2 < c + max / 16; c2++) {
  //        if ((r2 != r || c2 != c) && readAddress(r2, c2) != 0) {
  //          error(r2, c2);
  //          return;
  //        }
  //      }
  //    }
  //  }
  //}
  ok();
}

void testIntegrityFull() {
Serial.println();
Serial.println("Integrity test");
    testFill();
    testCheckerboard();
    testRetention();
    testInversion();
    testWalkingBits();
    testMATSpp();
    testMarchC();
    testMarchA();
    int t = bus_size == BUS_SIZE ? 262144:64535;
    testRandomAccess(t);
}

void readDRAMType()
{
  pinMode(M_TYPE, INPUT_PULLUP);
  bus_size = BUS_SIZE - 1;
  uint16_t test_addr = 0xF0;
  if (digitalRead(M_TYPE)) 
  {
    //writeAddress(test_addr, test_addr, 0);
    //writeAddress(test_addr | (1 << 8), test_addr | (1 << 8), 1);
    //if (readAddress(test_addr, test_addr) == 0)
      bus_size = BUS_SIZE;
  }
  Serial.print("DRAM Type selected: ");
  Serial.println(bus_size == BUS_SIZE ? "256Kx1" : "64Kx1");
}

int maxSpeed = 62;
void readSpeed()
{
    maxSpeed = 1.0/F_CPU* 1000000000.0;
    Serial.println("Max Speed able to test: ");
    Serial.print(maxSpeed);
    Serial.print(" ns");
    Serial.println();
}

void setup() 
{


  err_cnt = 0;

  pinMode(R_LED, OUTPUT);
  pinMode(G_LED, OUTPUT);
  digitalWrite(R_LED, LOW);
  digitalWrite(G_LED, LOW);

  Serial.begin(9600);
  while (!Serial);

  Serial.println();
  Serial.println("DRAM TESTER ");
  
  for (int i = 0; i < BUS_SIZE; i++)
    pinMode(a_bus[i], OUTPUT);

  digitalWrite(WE, HIGH);
  digitalWrite(RAS, HIGH);
  digitalWrite(CAS, HIGH);

  pinMode(CAS, OUTPUT);
  pinMode(RAS, OUTPUT);
  pinMode(WE, OUTPUT);
  pinMode(DI, OUTPUT);

  pinMode(M_TYPE, INPUT_PULLUP);
  pinMode(DO, INPUT_PULLUP);

  bus_size = BUS_SIZE - 1;
  uint16_t test_addr = 0xF0;

  //if (digitalRead(M_TYPE)) {
  //  writeAddress(test_addr, test_addr, 0);
  //  writeAddress(test_addr | (1 << 8), test_addr | (1 << 8), 1);
  //  if (readAddress(test_addr, test_addr) == 0)
  //    bus_size = BUS_SIZE;
 // }
  readDRAMType();

  Serial.println(bus_size == BUS_SIZE ? "256Kx1" : "64Kx1");

  //readSpeed();
  //Serial.flush();

  for (int i = 0; i < 8; i++) {
    digitalWrite(RAS, LOW);
    digitalWrite(RAS, HIGH);
  }

  digitalWrite(R_LED, HIGH);
  digitalWrite(G_LED, HIGH);
}

void loop()
 {

  if(true) // false to use the menu
  {
    Serial.println();
    testFill();
    testCheckerboard();
    testRetention();
    //testInversion();
    //testWalkingBits();
    //testMATSpp();
    //testMarchC();
    //testMarchA();
//    testAddressAliasing();
    //int t = bus_size == BUS_SIZE ? 262144:64535;
    //testRandomAccess(t);
    ok();
    while (1);
  }
  else
  {
    Serial.println();
    readDRAMType();
    Serial.println();
    Serial.println("=== DRAM TEST MENU ===");
    Serial.println();
    Serial.println("B - Run BASIC tests");
    Serial.println("A - Run ALL tests");
    Serial.println("F - Run FAST integrity test");
    Serial.println("X - Run Integrity test");
    Serial.println();
    Serial.println("Q - Quick Test - Fill & RND Access");
    Serial.println();
    Serial.println("--- Individual Tests ---");
    Serial.println("1 - Fill test (0/1)");
    Serial.println("2 - Checkerboard");
    Serial.println("3 - Retention");
    Serial.println("4 - Inversion");
    Serial.println("5 - Walking Bits");
    Serial.println("6 - MATS++");
    Serial.println("7 - March C");
    Serial.println("8 - March A");
    Serial.println("9 - Random access");
    Serial.println("0 - Address Aliasing");
    Serial.println("------------------------");
    Serial.println();
    Serial.println("Enter option:");

    while (!Serial.available());
    char option = Serial.read();
    Serial.println(); Serial.flush();
      
    readDRAMType();

    err_cnt = 0;

    switch (option) {
      case 'A': case 'a':
        testFill();
        testCheckerboard();
        testRetention();
        testInversion();
        //testAddressAliasing();
        testWalkingBits();
        //testMarchX();
        testMATSpp();
        testMarchC();
        testMarchA();
        //testIntegrityExhaustive();
        break;
      case 'B': case 'b':
        Serial.println();
        testFill();
        testCheckerboard();
        testRetention();
        break;
      case 'F': case 'f':
        testIntegrityFast();
        break;
      case 'X': case 'x':
        testIntegrityFull();
        break;
      case 'Q': case 'q':
        Serial.println();
        testFill();
      {
        int t = bus_size == BUS_SIZE ? 262144:64535;
        testRandomAccess(t); 
      }
        break;
      case '1': testFill(); break;
      case '2': testCheckerboard(); break;
      case '3': testRetention(); break;
      case '4': testInversion(); break;
      case '5': testWalkingBits(); break;
      case '6': testMATSpp(); break;
      case '7': testMarchC(); break;
      case '8': testMarchA(); break;
      case '9': 
      {
                int t = bus_size == BUS_SIZE ? 262144:64535;
                testRandomAccess(t); 
      }
      break;
      case '0': testAddressAliasing(); break;
      default:
        Serial.println("Invalid option. Please try again.");
        break;
    }

    ok();
    Serial.println("Press any key to return to menu...");
    while (!Serial.available());
    while (Serial.available()) Serial.read();
  }
}
