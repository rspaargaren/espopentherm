#include <arduino.h>
#include <Ticker.h>
#include <bitset>

volatile int interrupts;

const int OT_IN_PIN = 4; //3 //Arduino UNO
const int OT_OUT_PIN = 5; //2 //Arduino UNO
unsigned long bit_value = 0b0;
std::bitset<68> bit_recept;
bool bit_shift;
bool left_bit = true;
const unsigned int bitPeriod = 1000; //1020 //microseconds, 1ms -10%+15%
#define START   0xF0    // start pattern
#define STOP    0x01 //0x0F    // stop pattern



//P MGS-TYPE SPARE DATA-ID  DATA-VALUE
//0 000      0000  00000000 00000000 00000000
unsigned long requests[] = {
  0x1,
  //0x4,
  //0x300, //0 get status
  //0x600,
  //0x900
  //0x90014000, //1 set CH temp //64C
  //0x80190000, //25 Boiler water temperature
};

void setIdleState() {
  digitalWrite(OT_OUT_PIN, HIGH);
}

void setActiveState() {
  digitalWrite(OT_OUT_PIN, LOW);
}

void activateBoiler() {
  setIdleState();
  delay(1000);
}

void sendBit(bool high) {
  if (high) Serial1.write(0b01000000);
  else Serial1.write(0b10000000);
}

void writeBit(int pos,bool high) {
Serial.printf ("Send Bit: %i at Pos: %i" , high, pos);
  if (high) { // bit 1 = 10 manchester
    bit_recept[pos] = true;
    bit_recept[pos+1] = false;
  } else { // bit 0 = 01 manchester
    bit_recept[pos] = false;
    bit_recept[pos+1] = true;
  }
Serial.printf(" Manchester code: %i", bit_recept.test(pos));
Serial.print(bit_recept[pos+1] );
Serial.println();
}


void sendFrame(unsigned long request) {
  int j = 2;
  //sendBit(HIGH); //start bit
  bit_recept = 0b0; // reset bit recept
  writeBit(0,true); // start bit = 1
  for (int i = 31; i >= 0; i--) {    
    writeBit(j,bitRead(request,i));
    j = j+2;
  }
  writeBit(66,true); // stop bit = 1
  Serial.println("The bit recept is:");
  for (int i = 0; i < 68; i++) {
    Serial.print(bit_recept[i]);
  }
  bit_value = request;
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);
  timer1_write(2500);
  //Serial1.write(0b11110000);
  //Serial1.write(0b00001111);
  //Serial1.write(0b00001111);
  //Serial1.write(0b11110000);
  //Serial1.write(0b00001111);
  //Serial1.write(START);
  //Serial1.write(START);
  //for (int i = 31; i >= 0; i--) {
  //  sendBit(bitRead(request, i));
  //}
  //sendBit(HIGH); //stop bit  
  //Serial1.write(STOP);
  //setIdleState();
  //setActiveState(); 
}

void printBinary(unsigned long val) {
  for (int i = 31; i >= 0; i--) {
    Serial.print(bitRead(val, i));
  }
}

bool waitForResponse() {
  unsigned long time_stamp = micros();
  while (digitalRead(OT_IN_PIN) != HIGH) { //start bit
    if (micros() - time_stamp >= 1000000) {
      Serial.println("Response timeout");
      return false;
    }
  }
  delayMicroseconds(bitPeriod * 1.25); //wait for first bit
  return true;
}

unsigned long readResponse() {
  unsigned long response = 0;
  for (int i = 0; i < 32; i++) {
    response = (response << 1) | digitalRead(OT_IN_PIN);
    delayMicroseconds(bitPeriod);
  }
  Serial.println("Response: ");
  printBinary(response);
  Serial.print(" / ");
  Serial.print(response, HEX);
  Serial.println();

  if ((response >> 16 & 0xFF) == 25) {
    Serial.print("t=");
    Serial.print(response >> 8 & 0xFF);
    Serial.println("");
  }
  return response;
}

unsigned long sendRequest(unsigned long request) {
  Serial.println();
  Serial.print("Request:  ");
  printBinary(request);
  Serial.print(" / ");
  Serial.print(request, HEX);
  Serial.println();
  sendFrame(request);

  if (!waitForResponse()) return 0;

  return readResponse();
}

// ISR to Fire when Timer is triggered
void ICACHE_RAM_ATTR onTime() {
  bool bit_value;
  if (interrupts >= 68) {
    interrupts = 0;
    timer1_disable();
    digitalWrite(OT_OUT_PIN, HIGH);
  } else {
    bit_value = bit_recept.test(interrupts);
    digitalWrite(OT_OUT_PIN, bit_value);
    interrupts++;
  }
}

void setup() {
  pinMode(OT_IN_PIN, INPUT);
  pinMode(OT_OUT_PIN, OUTPUT);
  Serial.begin(115200);
  Serial.println("Start");
  Serial1.begin(10000);
  timer1_attachInterrupt(onTime);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);
  activateBoiler();
  timer1_write(2500000);
}




void loop() {
  for (int index = 0; index < (sizeof(requests) / sizeof(unsigned long)); index++) {
    sendRequest(requests[index]);
    delay(5000);
  }
}
