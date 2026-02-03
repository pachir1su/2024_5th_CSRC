#include <Wire.h>
#include <TM1637Display.h>
#include <LiquidCrystal_I2C.h>
#include "MAX30105.h"
#include "heartRate.h"

// 핀 정의
#define BUTTON_START 2
#define BUTTON_STOP 3
#define BUTTON_RESET 4
#define BUZZER_PIN 23
#define MAIN_LEVER_PIN 22
#define FAN_LEVER_PIN 30
#define LED_RED 8
#define LED_GREEN 9
#define LED_PIN 10
#define FAN_MOTOR_INA 11
#define FAN_MOTOR_INB 12
#define FAN_LED_RED 34
#define FAN_LED_GREEN 35
#define FND_CLK 26
#define FND_DIO 27
#define LIGHT_SENSOR_PIN A0

// FND 및 심박수, LCD 설정
TM1637Display display(FND_CLK, FND_DIO);
MAX30105 particleSensor;
LiquidCrystal_I2C lcd(0x27, 16, 2);

unsigned long startTime = 0;
unsigned long elapsedTime = 0;
bool isTiming = false;
int beatAvg;

// 버튼 상태 확인 변수
bool startPressed = false;
bool stopPressed = false;
bool resetPressed = false;

void setup() {
  Serial.begin(9600);
  
  // FND 디스플레이 설정
  display.setBrightness(0x0f);

  // LCD 설정
  lcd.begin(16, 2);
  lcd.backlight();
  lcd.print("Heart Rate:");

  // 심박수 센서 초기화
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 sensor not found.");
    while (1);
  }
  particleSensor.setup();
  particleSensor.setPulseAmplitudeRed(0x1F);
  particleSensor.setPulseAmplitudeIR(0x1F);

  // 핀 설정
  pinMode(BUTTON_START, INPUT_PULLUP);
  pinMode(BUTTON_STOP, INPUT_PULLUP);
  pinMode(BUTTON_RESET, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(MAIN_LEVER_PIN, INPUT_PULLUP);
  pinMode(FAN_LEVER_PIN, INPUT_PULLUP);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(FAN_MOTOR_INA, OUTPUT);
  pinMode(FAN_MOTOR_INB, OUTPUT);
  pinMode(FAN_LED_RED, OUTPUT);
  pinMode(FAN_LED_GREEN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  // 초기 FND 디스플레이 표시
  display.showNumberDecEx(0, 0b01000000, true);
}

void loop() {
  // 메인 레버 상태 확인
  bool mainLeverOn = digitalRead(MAIN_LEVER_PIN) == HIGH;
  bool fanLeverOn = digitalRead(FAN_LEVER_PIN) == HIGH;

  // 메인 레버에 따른 전체 시스템 제어
  if (!mainLeverOn) {
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_RED, HIGH);
    display.showNumberDecEx(0, 0b01000000, true); // FND에 0 표시
    digitalWrite(FAN_MOTOR_INA, LOW); // 팬 모터 OFF
    digitalWrite(FAN_MOTOR_INB, LOW);
    digitalWrite(FAN_LED_RED, HIGH);  // 팬 꺼짐 상태로 R LED 켜기
    digitalWrite(FAN_LED_GREEN, LOW); // G LED 끄기
    return; // 메인 레버가 OFF일 경우 모든 기능 정지
  } else {
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_RED, LOW);
  }

  // 팬 모터 및 LED 제어
  if (fanLeverOn) {
    // 팬 모터 정방향 회전 및 G LED 켜기
    digitalWrite(FAN_MOTOR_INA, HIGH);
    digitalWrite(FAN_MOTOR_INB, LOW);
    digitalWrite(FAN_LED_GREEN, HIGH); // 팬 작동 상태로 G LED 켜기
    digitalWrite(FAN_LED_RED, LOW);    // R LED 끄기
  } else {
    // 팬 모터 정지 및 R LED 켜기
    digitalWrite(FAN_MOTOR_INA, LOW);
    digitalWrite(FAN_MOTOR_INB, LOW);
    digitalWrite(FAN_LED_RED, HIGH);   // 팬 꺼짐 상태로 R LED 켜기
    digitalWrite(FAN_LED_GREEN, LOW);  // G LED 끄기
  }

  // 버튼 입력 확인 (부저 삐 소리 추가)
  if (digitalRead(BUTTON_START) == LOW && !startPressed) {
    startPressed = true;
    isTiming = true;
    startTime = millis();
    tone(BUZZER_PIN, 1000, 100); // 부저 삐 소리
  }
  if (digitalRead(BUTTON_START) == HIGH && startPressed) {
    startPressed = false;
  }

  if (digitalRead(BUTTON_STOP) == LOW && !stopPressed) {
    stopPressed = true;
    isTiming = false;
    elapsedTime += millis() - startTime;
    startTime = millis(); // startTime 초기화
    tone(BUZZER_PIN, 1000, 100); // 부저 삐 소리
  }
  if (digitalRead(BUTTON_STOP) == HIGH && stopPressed) {
    stopPressed = false;
  }

  if (digitalRead(BUTTON_RESET) == LOW && !resetPressed) {
    resetPressed = true;
    isTiming = false;
    elapsedTime = 0;
    display.showNumberDecEx(0, 0b01000000, true); // FND에 0 표시
    tone(BUZZER_PIN, 1000, 100); // 부저 삐 소리
  }
  if (digitalRead(BUTTON_RESET) == HIGH && resetPressed) {
    resetPressed = false;
  }

  // 스탑워치 동작 및 FND 출력
  if (isTiming) {
    unsigned long currentTime = millis();
    unsigned long totalElapsedTime = (elapsedTime + (currentTime - startTime)) / 1000;
    int minutes = totalElapsedTime / 60;
    int seconds = totalElapsedTime % 60;
    display.showNumberDecEx(minutes * 100 + seconds, 0b01000000, true); // 분:초 형식
  } else {
    int minutes = elapsedTime / 60000;
    int seconds = (elapsedTime / 1000) % 60;
    display.showNumberDecEx(minutes * 100 + seconds, 0b01000000, true); // 멈췄을 때 경과 시간 유지
  }

  // 심박수 측정 및 LCD 출력
  long irValue = particleSensor.getIR();
  if (checkForBeat(irValue)) {
    long delta = millis() - startTime;
    startTime = millis();
    beatAvg = 60 / (delta / 1000.0);

    if (beatAvg > 10 && beatAvg < 180) {
      lcd.setCursor(0, 1);
      lcd.print("BPM: ");
      lcd.print(beatAvg);
      lcd.print("   ");
    }
  }

  // 조도 센서로 LED 밝기 조절
  int lightLevel = analogRead(LIGHT_SENSOR_PIN);
  int ledBrightness = map(lightLevel, 0, 1023, 0, 255);
  analogWrite(LED_PIN, ledBrightness);

  delay(100);
}
