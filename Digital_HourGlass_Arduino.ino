#include <MPU6050_tockn.h>
#include "LedControl.h"
#include "Delay.h"

#define  MATRIX_A  1
#define MATRIX_B  0

MPU6050 mpu6050(Wire);

// Values are 260/330/400
#define ACC_THRESHOLD_LOW -25
#define ACC_THRESHOLD_HIGH 25

// Matrix
#define PIN_DATAIN 5
#define PIN_CLK 4
#define PIN_LOAD 6

// Accelerometer
#define PIN_X  mpu6050.getAngleX()
#define PIN_Y  mpu6050.getAngleY()

// Rotary Encoder
#define PIN_ENC_1 3
#define PIN_ENC_2 2
#define PIN_ENC_BUTTON 7

#define PIN_BUZZER 14

// This takes into account how the matrixes are mounted
#define ROTATION_OFFSET 90

// in milliseconds
#define DEBOUNCE_THRESHOLD 500

#define DELAY_FRAME 100

#define DEBUG_OUTPUT 1

#define MODE_HOURGLASS 1
#define MODE_SETMINUTES 2
#define MODE_SETHOURS 0

byte delayHours = 0;
byte delayMinutes = 1;
int mode = 3;
int gravity;
LedControl lc = LedControl(PIN_DATAIN, PIN_CLK, PIN_LOAD, 2);
NonBlockDelay d;
int resetCounter = 0;
bool alarmWentOff = false;
unsigned long timer = 0  ;
/**
   Get delay between particle drops (in seconds)
*/
long getDelayDrop() {
  // since we have exactly 60 particles we don't have to multiply by 60 and then divide by the number of particles again :)
  return delayMinutes + delayHours * 60;
}

coord getDown(int x, int y) {
  coord xy;
  xy.x = x - 1;
  xy.y = y + 1;
  return xy;
}
coord getLeft(int x, int y) {
  coord xy;
  xy.x = x - 1;
  xy.y = y;
  return xy;
}
coord getRight(int x, int y) {
  coord xy;
  xy.x = x;
  xy.y = y + 1;
  return xy;
}



bool canGoLeft(int addr, int x, int y) {
  if (x == 0) return false; // not available
  return !lc.getXY(addr, getLeft(x, y)); // you can go there if this is empty
}
bool canGoRight(int addr, int x, int y) {
  if (y == 7) return false; // not available
  return !lc.getXY(addr, getRight(x, y)); // you can go there if this is empty
}
bool canGoDown(int addr, int x, int y) {
  if (y == 7) return false; // not available
  if (x == 0) return false; // not available
  if (!canGoLeft(addr, x, y)) return false;
  if (!canGoRight(addr, x, y)) return false;
  return !lc.getXY(addr, getDown(x, y)); // you can go there if this is empty
}



void goDown(int addr, int x, int y) {
  lc.setXY(addr, x, y, false);
  lc.setXY(addr, getDown(x, y), true);
}
void goLeft(int addr, int x, int y) {
  lc.setXY(addr, x, y, false);
  lc.setXY(addr, getLeft(x, y), true);
}
void goRight(int addr, int x, int y) {
  lc.setXY(addr, x, y, false);
  lc.setXY(addr, getRight(x, y), true);
}


int countParticles(int addr) {
  int c = 0;
  for (byte y = 0; y < 8; y++) {
    for (byte x = 0; x < 8; x++) {
      if (lc.getXY(addr, x, y)) {
        c++;
      }
    }
  }
  return c;
}


bool moveParticle(int addr, int x, int y) {
  if (!lc.getXY(addr, x, y)) {
    return false;
  }

  bool can_GoLeft = canGoLeft(addr, x, y);
  bool can_GoRight = canGoRight(addr, x, y);

  if (!can_GoLeft && !can_GoRight) {
    return false; // we're stuck
  }

  bool can_GoDown = canGoDown(addr, x, y);

  if (can_GoDown) {
    goDown(addr, x, y);
  } else if (can_GoLeft && !can_GoRight) {
    goLeft(addr, x, y);
  } else if (can_GoRight && !can_GoLeft) {
    goRight(addr, x, y);
  } else if (random(2) == 1) { // we can go left and right, but not down
    goLeft(addr, x, y);
  } else {
    goRight(addr, x, y);
  }
  return true;
}



void fill(int addr, int maxcount) {
  int n = 8;
  byte x, y;
  int count = 0;
  for (byte slice = 0; slice < 2 * n - 1; ++slice) {
    byte z = slice < n ? 0 : slice - n + 1;
    for (byte j = z; j <= slice - z; ++j) {
      y = 7 - j;
      x = (slice - j);
      lc.setXY(addr, x, y, (++count <= maxcount));
    }
  }
}



/**
   Detect orientation using the accelerometer

       | up | right | left | down |
   --------------------------------
   400 |    |       | y    | x    |
   330 | y  | x     | x    | y    |
   260 | x  | y     |      |      |
*/
int getGravity() {
  int x = mpu6050.getAngleX();
  int y = mpu6050.getAngleY();

  if (y < ACC_THRESHOLD_LOW)  {
    return 90;
  }
  if (x > ACC_THRESHOLD_HIGH) {
    return 0;
  }
  if (y > ACC_THRESHOLD_HIGH) {
    return 270;
  }
  if (x < ACC_THRESHOLD_LOW)  {
    return 180;
  }
  else {
    return false;
  }
}


int getTopMatrix() {
  return (getGravity() == 90) ? MATRIX_A : MATRIX_B;
}
int getBottomMatrix() {
  return (getGravity() != 90) ? MATRIX_A : MATRIX_B;
}



void resetTime() {
  for (byte i = 0; i < 2; i++) {
    lc.clearDisplay(i);
  }
  fill(getTopMatrix(), 60);
  d.Delay(getDelayDrop() * 1000);
}



/**
   Traverse matrix and check if particles need to be moved
*/

bool updateMatrix() {
  int n = 8;
  bool somethingMoved = false;
  byte x, y;
  bool direction;
  for (byte slice = 0; slice < 2 * n - 1; ++slice) {
    direction = (random(2) == 1); // randomize if we scan from left to right or from right to left, so the grain doesn't always fall the same direction
    byte z = slice < n ? 0 : slice - n + 1;
    for (byte j = z; j <= slice - z; ++j) {
      y = direction ? (7 - j) : (7 - (slice - j));
      x = direction ? (slice - j) : j;

      if (moveParticle(MATRIX_B, x, y)) {
        somethingMoved = true;
      };
      if (moveParticle(MATRIX_A, x, y)) {
        somethingMoved = true;
      }
    }
  }
  return somethingMoved;
}



/**
   Let a particle go from one matrix to the other
*/
boolean dropParticle() {
  if (d.Timeout()) {
    d.Delay(getDelayDrop() * 1000);
    if (gravity == 0 || gravity == 180) {
      if ((lc.getRawXY(MATRIX_A, 0, 0) && !lc.getRawXY(MATRIX_B, 7, 7)) ||
          (!lc.getRawXY(MATRIX_A, 0, 0) && lc.getRawXY(MATRIX_B, 7, 7))
         ) {
        // for (byte d=0; d<8; d++) { lc.invertXY(0, 0, 7); delay(50); }
        lc.invertRawXY(MATRIX_A, 0, 0);
        lc.invertRawXY(MATRIX_B, 7, 7);
        tone(PIN_BUZZER, 440, 10);
        return true;
      }
    }
  }
  return false;
}

void resetCheck() {
  int z = analogRead(A0);
  if (z > ACC_THRESHOLD_HIGH || z < ACC_THRESHOLD_LOW) {
    resetCounter++;

  } else {
    resetCounter = 0;
  }
  if (resetCounter > 20) {
    resetTime();
    resetCounter = 0;
  }
}



void displayLetter(char letter, int matrix) {
  // Serial.print("Letter: ");
  // Serial.println(letter);
  lc.clearDisplay(matrix);
  lc.setXY(matrix, 1, 4, true);
  lc.setXY(matrix, 2, 3, true);
  lc.setXY(matrix, 3, 2, true);
  lc.setXY(matrix, 4, 1, true);

  lc.setXY(matrix, 3, 6, true);
  lc.setXY(matrix, 4, 5, true);
  lc.setXY(matrix, 5, 4, true);
  lc.setXY(matrix, 6, 3, true);

  if (letter == 'M') {
    lc.setXY(matrix, 4, 2, true);
    lc.setXY(matrix, 4, 3, true);
    lc.setXY(matrix, 5, 3, true);
  }
  if (letter == 'H') {
    lc.setXY(matrix, 3, 3, true);
    lc.setXY(matrix, 4, 4, true);
  }
}



void renderSetMinutes() {
  fill(getTopMatrix(), delayMinutes);
  displayLetter('M', getBottomMatrix());
}

void renderSetHours() {
  fill(getTopMatrix(), delayHours);
  displayLetter('H', getBottomMatrix());
}

/**
   Setup
*/
void setup() {
  mpu6050.calcGyroOffsets(true);
  Serial.begin(9600);
  mpu6050.begin();
  timer = millis();
  randomSeed(analogRead(A0));

  // init displays
  for (byte i = 0; i < 2; i++) {
    lc.shutdown(i, false);
    lc.setIntensity(i, 0);
  }

  resetTime();
}

String x = " ";
int flag = 0 ;
/**
   Main loop
*/
void loop() {


  if (Serial.available()) {
    x = Serial.readString();
    if (x == "start") {
      flag = 1 ;
      Serial.println(x);
    } else if (x == "Pause") {
      flag = 0;
      Serial.println(x);
    } else if (x == "restart") {
      resetTime();
      flag = 1;
    }else if(x.substring(0,5) == "Speed" ){
       delayMinutes = x.substring(5).toInt() ;
    }

  }

  if (flag) {
    mpu6050.update();
    delay(DELAY_FRAME);
    
    // update the driver's rotation setting. For the rest of the code we pretend "down" is still 0,0 and "up" is 7,7
    gravity = getGravity();
    lc.setRotation((ROTATION_OFFSET + gravity) % 360);
 
    // handle special modes
    if (mode == MODE_SETMINUTES) {
      renderSetMinutes(); return;
    } else if (mode == MODE_SETHOURS) {
      renderSetHours(); return;
    }

    // resetCheck(); // reset now happens when pushing a button
    bool moved = updateMatrix();
    bool dropped = dropParticle();

    // alarm when everything is in the bottom part
    if (!moved && !dropped && !alarmWentOff && (countParticles(getTopMatrix()) == 0)) {
      alarmWentOff = true;
      Serial.println("finished");
    }
    // reset alarm flag next time a particle was dropped
    if (dropped) {
      alarmWentOff = false;
      Serial.println(String(((countParticles(0)*delayMinutes)/60)/(60) )+":"+
        String((countParticles(0)*delayMinutes)/60)+":"+
        String(countParticles(0)*delayMinutes%60)
      );

    }
  }


}
