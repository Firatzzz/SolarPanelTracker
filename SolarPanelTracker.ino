#include <Servo.h>
#include <LiquidCrystal.h>

#define voltage_input A5  
LiquidCrystal lcd(12, 11, 9, 8, 7, 6);

Servo horizontal;
Servo vertical;
int servoh = 90;
int servov = 90;
int time_ = 10;
int tol = 1;

void setup() {
    lcd.begin(16, 2);
    lcd.setCursor(0, 0);
    lcd.print("=====START=====");
    delay(1000);
    lcd.clear();
    Serial.begin(9600);

    horizontal.attach(4);
    vertical.attach(3);
    pinMode(A0, INPUT);
    pinMode(A1, INPUT);
    pinMode(A2, INPUT);
    pinMode(A3, INPUT);
}

void loop() {
    int down = (analogRead(A3) / tol);
    int up = (analogRead(A2) / tol);
    int right = (analogRead(A1) / tol);
    int left = (analogRead(A0) / tol);

    Serial.print("\nright=");
    Serial.print(right);
    Serial.print(" left=");
    Serial.print(left);
    Serial.print(" up=");
    Serial.print(up);
    Serial.print(" down=");
    Serial.print(down);

    if (up < down) {
        servov = ++servov;
        if (servov > 180) {
            servov = 180;
        }
    } else if (up > down) {
        servov = --servov;
        if (servov < 0) {
            servov = 0;
        }
    }

    vertical.write(servov);

    if (left < right) {
        servoh = --servoh;
        if (servoh < 0) {
            servoh = 0;
        }
    } else if (left > right) {
        servoh = ++servoh;
        if (servoh > 180) {
            servoh = 180;
        }
    }

    horizontal.write(servoh);
    delay(time_);
    read();
}

void read() {
    int read = analogRead(voltage_input);
    float voltage = read * (5.0 / 1024.0) * 4.6;

    Serial.print("\nvoltage = ");
    Serial.println(voltage, 3);
    Serial.print("A5 = ");
    Serial.print(read);
    Serial.println(" ");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(" VOLTAGE : ");
    lcd.setCursor(6, 1);
    lcd.print(voltage);
    delay(3000);  // Delay 3 detik
    
    checkLightLevels();
}

void checkLightLevels() {
    int sensors[4] = {analogRead(A0), analogRead(A1), analogRead(A2), analogRead(A3)};
    String lightLevels[4];

    for (int i = 0; i < 4; i++) {
        if (sensors[i] > 900) {
            lightLevels[i] = "Gelap";
        } else if (sensors[i] > 500) {
            lightLevels[i] = "Redup";
        } else if (sensors[i] > 100) {
            lightLevels[i] = "Terang";
        } else {
            lightLevels[i] = "Sangat Terang";  // Any value <= 100
        }
    }

    for (int i = 0; i < 4; i++) {
        Serial.print("Sensor ");
        Serial.print(i);
        Serial.print(": ");
        Serial.println(lightLevels[i]);
        lcd.setCursor(0, i % 2);  // Print on either first or second row
        lcd.print("S");
        lcd.print(i);
        lcd.print(": ");
        lcd.print(lightLevels[i]);
        if (i % 2 == 1) {
            delay(3000);  // Delay to allow reading the values before updating the screen
            lcd.clear();
        }
    }
}
