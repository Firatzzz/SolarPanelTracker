#include <Servo.h>
#include <LiquidCrystal.h>

// Definisi pin LDR dan input tegangan
#define LDR_UP_PIN A2
#define LDR_DOWN_PIN A3
#define LDR_LEFT_PIN A0
#define LDR_RIGHT_PIN A1
#define VOLTAGE_INPUT_PIN A5

// Inisialisasi LCD
LiquidCrystal lcd(12, 11, 9, 8, 7, 6);

// Objek Servo
Servo servoHorizontal;
Servo servoVertical;

// Posisi awal servo (tengah) dan variabel terkait
int servoHPos = 90;
int servoVPos = 90;
int servoHLimitMin = 0;    // Batas minimum servo horizontal
int servoHLimitMax = 180;  // Batas maksimum servo horizontal
int servoVLimitMin = 0;    // Batas minimum servo vertikal
int servoVLimitMax = 180;  // Batas maksimum servo vertikal

// Waktu tunda dalam loop utama (ms)
int loopDelayTime = 50; // Meningkatkan sedikit dari 10ms untuk stabilitas
int tolerance = 1; // Variabel 'tol' dari kode asli, saat ini tidak banyak berpengaruh jika 1

// === Parameter Logika Fuzzy ===

// Output Singletons untuk Perubahan Sudut Vertikal (delta_sv)
// Nilai positif berarti gerakan ke atas (mengurangi servoVPos), negatif berarti ke bawah (menambah servoVPos)
const float SV_NAIK_CEPAT = 5.0;
const float SV_NAIK_LAMBAT = 2.0;
const float SV_TIDAK_UBAH = 0.0;
const float SV_TURUN_LAMBAT = -2.0;
const float SV_TURUN_CEPAT = -5.0;

// Output Singletons untuk Perubahan Sudut Horizontal (delta_sh)
// Nilai positif berarti gerakan ke kanan (mengurangi servoHPos), negatif berarti ke kiri (menambah servoHPos)
const float SH_KANAN_CEPAT = 5.0;
const float SH_KANAN_LAMBAT = 2.0;
const float SH_TIDAK_UBAH = 0.0;
const float SH_KIRI_LAMBAT = -2.0;
const float SH_KIRI_CEPAT = -5.0;


// Fungsi Keanggotaan Triangular
// x: nilai input crisp
// a, b, c: poin dari segitiga (a: kaki kiri, b: puncak, c: kaki kanan)
float triangularMF(float x, float a, float b, float c) {
    if (x <= a || x >= c) {
        return 0.0;
    } else if (x == b) {
        return 1.0;
    } else if (x > a && x < b) {
        return (x - a) / (b - a);
    } else { // x > b && x < c
        return (c - x) / (c - b);
    }
}

void setup() {
    lcd.begin(16, 2);
    lcd.setCursor(0, 0);
    lcd.print("Fuzzy Tracker");
    delay(1000);
    lcd.clear();
    Serial.begin(9600);

    servoHorizontal.attach(4);
    servoVertical.attach(3);

    // Set servo ke posisi tengah awal
    servoHorizontal.write(servoHPos);
    servoVertical.write(servoVPos);

    pinMode(LDR_UP_PIN, INPUT);
    pinMode(LDR_DOWN_PIN, INPUT);
    pinMode(LDR_LEFT_PIN, INPUT);
    pinMode(LDR_RIGHT_PIN, INPUT);
    pinMode(VOLTAGE_INPUT_PIN, INPUT);

    Serial.println("Sistem Fuzzy Solar Tracker Dimulai");
}

void loop() {
    // 1. Baca Nilai Sensor LDR
    int ldrUp = analogRead(LDR_UP_PIN) / tolerance;
    int ldrDown = analogRead(LDR_DOWN_PIN) / tolerance;
    int ldrLeft = analogRead(LDR_LEFT_PIN) / tolerance;
    int ldrRight = analogRead(LDR_RIGHT_PIN) / tolerance;

    // 2. Hitung Perbedaan (Error)
    // ErrV > 0 berarti atas lebih terang
    // ErrV < 0 berarti bawah lebih terang
    int errorVertical = ldrUp - ldrDown;

    // ErrH > 0 berarti kanan lebih terang
    // ErrH < 0 berarti kiri lebih terang
    int errorHorizontal = ldrRight - ldrLeft;

    Serial.print("LDRs: U="); Serial.print(ldrUp);
    Serial.print(" D="); Serial.print(ldrDown);
    Serial.print(" L="); Serial.print(ldrLeft);
    Serial.print(" R="); Serial.print(ldrRight);
    Serial.print(" | ErrV="); Serial.print(errorVertical);
    Serial.print(" ErrH="); Serial.print(errorHorizontal);

    // === Logika Fuzzy untuk Gerakan Vertikal ===
    float fuzzy_AST, fuzzy_AT, fuzzy_SV, fuzzy_BT, fuzzy_BST;

    // Fuzzifikasi untuk Error_Vertical (ErrV)
    // Rentang nilai perlu di-tuning. Contoh:
    // AST (Atas Sangat Terang): ErrV positif besar
    fuzzy_AST = triangularMF(errorVertical, 200, 500, 800); // Puncak di 500, rentang 200-800
    // AT (Atas Terang): ErrV positif sedang
    fuzzy_AT = triangularMF(errorVertical, 50, 250, 450);   // Puncak di 250, rentang 50-450
    // SV (Seimbang Vertikal): ErrV dekat nol
    fuzzy_SV = triangularMF(errorVertical, -100, 0, 100);   // Puncak di 0, rentang -100-100
    // BT (Bawah Terang): ErrV negatif sedang
    fuzzy_BT = triangularMF(errorVertical, -450, -250, -50); // Puncak di -250, rentang -450-(-50)
    // BST (Bawah Sangat Terang): ErrV negatif besar
    fuzzy_BST = triangularMF(errorVertical, -800, -500, -200); // Puncak di -500, rentang -800-(-200)

    // Inferensi dan Defuzzifikasi (Weighted Average Sederhana)
    // Aturan:
    // 1. IF ErrV is AST THEN delta_sv is SV_NAIK_CEPAT
    // 2. IF ErrV is AT  THEN delta_sv is SV_NAIK_LAMBAT
    // 3. IF ErrV is SV  THEN delta_sv is SV_TIDAK_UBAH
    // 4. IF ErrV is BT  THEN delta_sv is SV_TURUN_LAMBAT
    // 5. IF ErrV is BST THEN delta_sv is SV_TURUN_CEPAT
    float sumWeightedOutputsV = (fuzzy_AST * SV_NAIK_CEPAT) +
                               (fuzzy_AT * SV_NAIK_LAMBAT) +
                               (fuzzy_SV * SV_TIDAK_UBAH) +
                               (fuzzy_BT * SV_TURUN_LAMBAT) +
                               (fuzzy_BST * SV_TURUN_CEPAT);
    float sumFiringStrengthsV = fuzzy_AST + fuzzy_AT + fuzzy_SV + fuzzy_BT + fuzzy_BST;
    
    float deltaServoVertical = 0;
    if (sumFiringStrengthsV > 0) { // Hindari pembagian dengan nol
        deltaServoVertical = sumWeightedOutputsV / sumFiringStrengthsV;
    }

    // === Logika Fuzzy untuk Gerakan Horizontal ===
    float fuzzy_KAST, fuzzy_KAT, fuzzy_SH, fuzzy_KIT, fuzzy_KIST;

    // Fuzzifikasi untuk Error_Horizontal (ErrH)
    // KAST (Kanan Sangat Terang)
    fuzzy_KAST = triangularMF(errorHorizontal, 200, 500, 800);
    // KAT (Kanan Terang)
    fuzzy_KAT = triangularMF(errorHorizontal, 50, 250, 450);
    // SH (Seimbang Horizontal)
    fuzzy_SH = triangularMF(errorHorizontal, -100, 0, 100);
    // KIT (Kiri Terang)
    fuzzy_KIT = triangularMF(errorHorizontal, -450, -250, -50);
    // KIST (Kiri Sangat Terang)
    fuzzy_KIST = triangularMF(errorHorizontal, -800, -500, -200);

    // Inferensi dan Defuzzifikasi
    // Aturan:
    // 1. IF ErrH is KAST THEN delta_sh is SH_KANAN_CEPAT
    // 2. IF ErrH is KAT  THEN delta_sh is SH_KANAN_LAMBAT
    // 3. IF ErrH is SH   THEN delta_sh is SH_TIDAK_UBAH
    // 4. IF ErrH is KIT  THEN delta_sh is SH_KIRI_LAMBAT
    // 5. IF ErrH is KIST THEN delta_sh is SH_KIRI_CEPAT
    float sumWeightedOutputsH = (fuzzy_KAST * SH_KANAN_CEPAT) +
                               (fuzzy_KAT * SH_KANAN_LAMBAT) +
                               (fuzzy_SH * SH_TIDAK_UBAH) +
                               (fuzzy_KIT * SH_KIRI_LAMBAT) +
                               (fuzzy_KIST * SH_KIRI_CEPAT);
    float sumFiringStrengthsH = fuzzy_KAST + fuzzy_KAT + fuzzy_SH + fuzzy_KIT + fuzzy_KIST;

    float deltaServoHorizontal = 0;
    if (sumFiringStrengthsH > 0) { // Hindari pembagian dengan nol
        deltaServoHorizontal = sumWeightedOutputsH / sumFiringStrengthsH;
    }
    
    Serial.print(" | dSV="); Serial.print(deltaServoVertical);
    Serial.print(" dSH="); Serial.println(deltaServoHorizontal);

    // 3. Update Posisi Servo
    // Berdasarkan logika kode asli Anda:
    // Jika atas lebih terang (ErrV > 0), deltaServoVertical positif, servov dikurangi (gerak ke atas)
    // Jika bawah lebih terang (ErrV < 0), deltaServoVertical negatif, servov ditambah (gerak ke bawah)
    servoVPos -= (int)round(deltaServoVertical);

    // Jika kanan lebih terang (ErrH > 0), deltaServoHorizontal positif, servoh dikurangi (gerak ke kanan)
    // Jika kiri lebih terang (ErrH < 0), deltaServoHorizontal negatif, servoh ditambah (gerak ke kiri)
    servoHPos -= (int)round(deltaServoHorizontal);


    // Batasi pergerakan servo
    if (servoVPos > servoVLimitMax) servoVPos = servoVLimitMax;
    if (servoVPos < servoVLimitMin) servoVPos = servoVLimitMin;
    if (servoHPos > servoHLimitMax) servoHPos = servoHLimitMax;
    if (servoHPos < servoHLimitMin) servoHPos = servoHLimitMin;

    servoVertical.write(servoVPos);
    servoHorizontal.write(servoHPos);
    
    // Panggil fungsi read() dan checkLightLevels() dari kode asli Anda
    readAndDisplay(); // Menggabungkan read() dan checkLightLevels() untuk efisiensi

    delay(loopDelayTime);
}

void readAndDisplay() {
    // Baca tegangan
    int adcRead = analogRead(VOLTAGE_INPUT_PIN);
    float voltage = adcRead * (5.0 / 1024.0) * 4.6; // Faktor kalibrasi 4.6 dari kode asli

    Serial.print("Voltage = "); Serial.print(voltage, 3);
    Serial.print("V (A5 ADC: "); Serial.print(adcRead); Serial.println(")");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("VOLT: ");
    lcd.print(voltage, 2); // Tampilkan 2 desimal di LCD
    
    lcd.setCursor(0,1);
    lcd.print("H:"); lcd.print(servoHPos);
    lcd.print(" V:"); lcd.print(servoVPos);

    delay(1000); // Tunda untuk pembacaan LCD sebelum menampilkan level cahaya

    // Cek Level Cahaya (fungsi checkLightLevels dari kode asli)
    int sensors[4] = {
        analogRead(LDR_LEFT_PIN),  // A0
        analogRead(LDR_RIGHT_PIN), // A1
        analogRead(LDR_UP_PIN),    // A2
        analogRead(LDR_DOWN_PIN)   // A3
    };
    String lightLevels[4];
    String sensorNames[4] = {"L", "R", "U", "D"}; // Kiri, Kanan, Atas, Bawah

    for (int i = 0; i < 4; i++) {
        if (sensors[i] > 800) { // Menyesuaikan threshold sedikit dari kode asli
            lightLevels[i] = "Gelap";
        } else if (sensors[i] > 400) {
            lightLevels[i] = "Redup";
        } else if (sensors[i] > 100) {
            lightLevels[i] = "Terang";
        } else {
            lightLevels[i] = "S.Terang"; // Sangat Terang
        }
    }
    
    lcd.clear();
    for (int i = 0; i < 4; i++) {
        Serial.print("Sensor "); Serial.print(sensorNames[i]);
        Serial.print(" (A"); Serial.print(i); Serial.print("): ");
        Serial.print(sensors[i]); Serial.print(" -> "); Serial.println(lightLevels[i]);

        // Tampilkan 2 sensor per baris di LCD
        if (i < 2) { // Sensor L dan R di baris 0
            lcd.setCursor(i * 8, 0); // 0,0 atau 8,0
        } else { // Sensor U dan D di baris 1
            lcd.setCursor((i-2) * 8, 1); // 0,1 atau 8,1
        }
        lcd.print(sensorNames[i]); lcd.print(":"); lcd.print(lightLevels[i]);
    }
    delay(2000); // Tunda untuk pembacaan LCD
}
