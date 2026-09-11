/*
======================================================================
 ARDUINO MEGA 2560 + L298N + BLUETOOTH HC-06
 Robot điều khiển qua UART3 – Code chính thức của TranDangKhoaAutomation
======================================================================

📌 TÍNH NĂNG:
 - Điều khiển xe 2 động cơ DC qua L298N
 - Nhận lệnh từ HC-06 qua Serial3 (9600 bps)
 - Hỗ trợ các lệnh:
      F B L R  → Tiến / lùi / quay trái / quay phải
      G I H J  → Rẽ mềm / đi chéo (hệ số diagFactor)
      + -      → Tăng / giảm tốc
      0..9     → Chọn mức tốc độ (0 = min, 9 = max)
      S        → Dừng tự do
      P        → Phanh cứng

📌 TỐC ĐỘ:
 - Dùng PWM 0–255
 - currentSpeed: tốc độ hiện tại
 - diagFactor: hệ số chỉnh tốc bánh khi rẽ mềm

📌 PHẦN CỨNG DÙNG:
 - Arduino Mega 2560
 - L298N (driver 2 kênh)
 - Bluetooth HC-06 (UART)
 - 2 động cơ DC

======================================================================
 SƠ ĐỒ NỐI DÂY (ASCII – dễ hình dung)
======================================================================

        +--------------------------+
        |      ARDUINO MEGA       |
        +--------------------------+
            (TX3=14) -----> HC-06 RX
            (RX3=15) <----- HC-06 TX
            5V  -------> HC-06 VCC
            GND -------> HC-06 GND

   L298N DRIVER
   +--------------------------------------+
   | ENA | IN1 | IN2 | IN3 | IN4 | ENB   |
   +--------------------------------------+
      |     |     |     |     |     |
      |     |     |     |     |     +------> Mega D7 (PWM)
      |     |     |     |     +------------> Mega D6
      |     |     |     +-------------------> Mega D5
      |     |     +--------------------------> Mega D4
      |     +-------------------------------> Mega D3
      +-------------------------------------> Mega D2 (PWM)

   MOTOR TRÁI  ←→  OUT1, OUT2 (L298N)
   MOTOR PHẢI  ←→  OUT3, OUT4 (L298N)

======================================================================
 LƯU Ý NGUỒN ĐIỆN CHO L298N
======================================================================
 - Động cơ DC phải dùng nguồn riêng 6V–12V
 - GND động cơ phải nối chung GND Arduino
 - KHÔNG dùng 5V của Arduino cấp motor!

======================================================================
 BẮT ĐẦU CODE CHÍNH
======================================================================
*/

#include <Arduino.h>

// ================== KHAI BÁO CHÂN L298N ==================
#define ENA 2   // PWM motor trái
#define IN1 3
#define IN2 4

#define IN3 5
#define IN4 6
#define ENB 7   // PWM motor phải

// ================== THAM SỐ TỐC ĐỘ ==================
int currentSpeed = 150;          // tốc độ mặc định
const int SPEED_STEP = 20;
const int SPEED_MIN  = 60;
const int SPEED_MAX  = 255;

// Hệ số tốc bánh khi rẽ mềm
float diagFactor = 0.5;          

// ================== KHỞI TẠO MOTOR ==================
void initMotorPins() {
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);

  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
}

// ================== HÀM ĐIỀU KHIỂN 1 MOTOR ==================
void setOneMotor(uint8_t side, int speed) {
  if (speed > 255)  speed = 255;
  if (speed < -255) speed = -255;

  uint8_t pwm = abs(speed);

  if (side == 0) { // Motor trái
    if (speed > 0) {
      digitalWrite(IN1, HIGH);
      digitalWrite(IN2, LOW);
    } else if (speed < 0) {
      digitalWrite(IN1, LOW);
      digitalWrite(IN2, HIGH);
    } else {
      digitalWrite(IN1, LOW);
      digitalWrite(IN2, LOW);
    }
    analogWrite(ENA, pwm);
  } else {         // Motor phải
    if (speed > 0) {
      digitalWrite(IN3, HIGH);
      digitalWrite(IN4, LOW);
    } else if (speed < 0) {
      digitalWrite(IN3, LOW);
      digitalWrite(IN4, HIGH);
    } else {
      digitalWrite(IN3, LOW);
      digitalWrite(IN4, LOW);
    }
    analogWrite(ENB, pwm);
  }
}

// ================== HÀM ĐIỀU KHIỂN CẶP MOTOR ==================
void setMotor(int leftSpeed, int rightSpeed) {
  setOneMotor(0, leftSpeed);
  setOneMotor(1, rightSpeed);
}

// ================== HÀM DI CHUYỂN CƠ BẢN ==================
void stopFree() {
  setMotor(0, 0);
}

void brakeHard() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, HIGH);
  analogWrite(ENA, 0);

  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, HIGH);
  analogWrite(ENB, 0);
}

void goForward(uint8_t speed)  { setMotor(speed, speed); }
void goBackward(uint8_t speed) { setMotor(-speed, -speed); }
void spinLeft(uint8_t speed)   { setMotor(speed, -speed); }
void spinRight(uint8_t speed)  { setMotor(-speed, speed); }

// ================== RẼ MỀM / ĐI CHÉO (G, I, H, J) ==================
void forwardLeft(uint8_t speed) {
  int outer = speed;
  int inner = speed * diagFactor;
  setMotor(outer, inner);
}

void forwardRight(uint8_t speed) {
  int outer = speed;
  int inner = speed * diagFactor;
  setMotor(inner, outer);
}

void backwardLeft(uint8_t speed) {
  int outer = -speed;
  int inner = -(speed * diagFactor);
  setMotor(outer, inner);
}

void backwardRight(uint8_t speed) {
  int outer = -speed;
  int inner = -(speed * diagFactor);
  setMotor(inner, outer);
}

// ================== XỬ LÝ LỆNH BLUETOOTH ==================
void handleCommand(char cmd) {
  // --------- PHÍM SỐ 0..9: CHỌN MỨC TỐC ĐỘ ----------
  if (cmd >= '0' && cmd <= '9') {
    int level = cmd - '0'; // 0..9
    // map 0..9 -> SPEED_MIN..SPEED_MAX
    currentSpeed = SPEED_MIN + (int)((SPEED_MAX - SPEED_MIN) * (float)level / 9.0f);

    Serial.print(F("[CMD] SPEED LEVEL "));
    Serial.print(level);
    Serial.print(F(" -> "));
    Serial.println(currentSpeed);
    return; // đã xử lý xong, không vào switch nữa
  }

  // --------- CÁC LỆNH CHỮ CÁI ----------
  switch (cmd) {
    case 'F': case 'f':
      goForward(currentSpeed);
      Serial.println(F("[CMD] FORWARD"));
      break;

    case 'B': case 'b':
      goBackward(currentSpeed);
      Serial.println(F("[CMD] BACKWARD"));
      break;

    case 'L': case 'l':
      spinLeft(currentSpeed);
      Serial.println(F("[CMD] SPIN LEFT"));
      break;

    case 'R': case 'r':
      spinRight(currentSpeed);
      Serial.println(F("[CMD] SPIN RIGHT"));
      break;

    case 'S': case 's':
      stopFree();
      Serial.println(F("[CMD] STOP FREE"));
      break;

    case 'P': case 'p':
      brakeHard();
      Serial.println(F("[CMD] BRAKE HARD"));
      break;

    case '+':
      currentSpeed += SPEED_STEP;
      if (currentSpeed > SPEED_MAX) currentSpeed = SPEED_MAX;
      Serial.print(F("[CMD] SPEED UP -> "));
      Serial.println(currentSpeed);
      break;

    case '-':
      currentSpeed -= SPEED_STEP;
      if (currentSpeed < SPEED_MIN) currentSpeed = SPEED_MIN;
      Serial.print(F("[CMD] SPEED DOWN -> "));
      Serial.println(currentSpeed);
      break;

    // ====== G I H J ======
    case 'G': case 'g':
      forwardLeft(currentSpeed);
      Serial.println(F("[CMD] FORWARD-LEFT (G)"));
      break;

    case 'I': case 'i':
      forwardRight(currentSpeed);
      Serial.println(F("[CMD] FORWARD-RIGHT (I)"));
      break;

    case 'H': case 'h':
      backwardLeft(currentSpeed);
      Serial.println(F("[CMD] BACKWARD-LEFT (H)"));
      break;

    case 'J': case 'j':
      backwardRight(currentSpeed);
      Serial.println(F("[CMD] BACKWARD-RIGHT (J)"));
      break;

    default:
      Serial.print(F("[CMD] UNKNOWN: "));
      Serial.println(cmd);
      break;
  }
}

// ================== SETUP & LOOP ==================
void setup() {
  Serial.begin(115200);   
  Serial3.begin(9600);    

  initMotorPins();

  Serial.println(F("=== ArduinoMega + L298N + HC06 READY ==="));
  Serial.print(F("Current speed = ")); Serial.println(currentSpeed);
  Serial.print(F("diagFactor = ")); Serial.println(diagFactor);
}

void loop() {
  if (Serial3.available()) {
    char c = Serial3.read();
    handleCommand(c);
  }
}
