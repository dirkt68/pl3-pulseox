/*------------------------------- LIBRARIES ---------------------------------*/
#include "WiFi.h" // WiFi library
#include "SPI.h"  // SPI library
#include "Wire.h" // i2c library

#include <display_code.h>

// libraries for SPO2 sensor
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include "heartRate.h"

// libraries for temp. sensor
#include "Protocentral_MAX30205.h"

// wifi manager for dynamic access
#include "WifiManager.h"

// firebase stuff
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

/*------------------------------- DEFINES ---------------------------------*/
#define DEBUG // when available, print debug information to the screen
#define SCREEN_CONN // when no screen attached, disable screen function calls

#define BUTTON 39        // pin connected to wifi connection button
#define WIFI_TIMER 2000 // every 30 seconds, send wifi info

// define stuff to log in to firebase
#define API_KEY "AIzaSyDrjThqfnejA6Lc12Lwnbxfnrqdf2X1TZ0"
#define DB_URL "https://project-lab-3-45cf8-default-rtdb.firebaseio.com"

#define I2C_CUSTOM_SCL 19
#define I2C_CUSTOM_SDA 21


/*------------------------------- GLOBALS ---------------------------------*/
bool wifi_enabled = true;
bool firebase_enabled = false;

TFT_eSPI tft = TFT_eSPI();

MAX30205 tempSensor;
MAX30105 pulseOxSensor;

/* PULSE OX VARIABLES */
const uint8_t POBufferSize = 100;
const uint8_t RATE_SIZE = 4;

uint32_t irLEDBuf[POBufferSize];
uint32_t redLEDBuf[POBufferSize];

int32_t spo2;
int8_t validSPO2;

int32_t heartRate;
int8_t validHeartRate;

/* TEMPERATURE VARIABLES */
double temperature;

/* FIREBASE VARIABLES */
FirebaseData FBDO;
FirebaseAuth auth;
FirebaseConfig config;

/* TIMING VARIABLES */
uint64_t firebaseTimer = 0;

/*------------------------------- INTERRUPT SERVICE ROUTINE ---------------------------------*/
/* interrupt handler for the button */
void IRAM_ATTR ISR()
{
    // only connect to wifi if not already connected
    if (WiFi.status() != WL_CONNECTED)
    {
        wifi_enabled = true;
    }
}

/*------------------------------- HELPER FUNCTIONS ---------------------------------*/
/* convert temperature from celsius to fahrenheit */
double temp_CtoF(double tempC)
{
    return (tempC * 1.8) + 32;
}

/* setup wifi when triggered */
void wifi_setup()
{
    wifi_enabled = false;

#ifdef SCREEN_CONN
    tft.setSwapBytes(true);
    tft.fillScreen(TFT_WHITE);
    tft.pushImage(60, 60, 120, 120, BWiFi120);
#endif

    // connect to the wifi using Wifi Manager
    WiFiManager wm;
    wm.setClass("invert");
    bool result = wm.autoConnect("LifeMTR");

#ifdef DEBUG
    if (!result)
    {
        Serial.println("Couldn't connect to wifi");
    }
#endif

    if (Firebase.signUp(&config, &auth, "", ""))
    {
        firebase_enabled = true;
    }

    Firebase.begin(&config, &auth);

#ifdef SCREEN_CONN
    tft.fillScreen(TFT_WHITE);
#endif
}

/*------------------------------- INITIAL SETUP ---------------------------------*/
void setup()
{
    // sleep to allow all devices to turn on
    sleep(1);
    Wire.begin(I2C_CUSTOM_SDA, I2C_CUSTOM_SCL, I2C_SPEED_FAST);
#ifdef DEBUG
    // while testing, use the serial port to print info
    Serial.begin(115200);
#endif

/*------------------------------- FIREBASE SETUP ---------------------------------*/
    config.api_key = API_KEY;
    config.database_url = DB_URL;
    config.token_status_callback = tokenStatusCallback;

/*------------------------------- SCREEN SETUP ---------------------------------*/
#ifdef SCREEN_CONN
    tft.begin();
    tft.setRotation(2);
    tft.setSwapBytes(true);
    tft.fillScreen(TFT_WHITE);
    tft.pushImage(60, 60, 120, 120, LifeMTR);
#endif

/*------------------------------- SENSOR SETUP ---------------------------------*/
    // initialize temp sensor
    tempSensor.begin();

    // initialize pulse ox sensor
    pulseOxSensor.begin(Wire, I2C_SPEED_FAST);
    // recommended values to run the device at
    uint8_t ledBrightness = 60;
    uint8_t sampleAverage = 4;
    uint8_t ledMode = 3;
    uint8_t sampleRate = 100;
    uint16_t pulseWidth = 411;
    uint16_t adcRange = 4096;
    pulseOxSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);

    // perform a full pulse ox reading
    for (uint8_t i = 0; i < POBufferSize; i++)
    {
        // wait for a reading
        while (!pulseOxSensor.available())
        {
            pulseOxSensor.check();
        }

        // when a reading is received, save it to the buffers
        redLEDBuf[i] = pulseOxSensor.getRed();
        irLEDBuf[i] = pulseOxSensor.getIR();
        // advance the queue
        pulseOxSensor.nextSample();
    }

    // take extraneous reading
    maxim_heart_rate_and_oxygen_saturation(irLEDBuf,
                                           POBufferSize,
                                           redLEDBuf,
                                           &spo2,
                                           &validSPO2,
                                           &heartRate,
                                           &validHeartRate);

    // setup wifi button to trigger interrupt
    pinMode(BUTTON, INPUT_PULLUP);
    attachInterrupt(BUTTON, ISR, FALLING);

#ifdef SCREEN_CONN
    tft.fillScreen(TFT_WHITE);
#endif
}

/*------------------------------- MAIN CODE LOOP ---------------------------------*/
void loop()
{
    /* DO PULSE OX MEASUREMENT */
    // keep the latest 75 samples
    for (uint8_t i = 25; i < POBufferSize; i++)
    {
        irLEDBuf[i - 25] = irLEDBuf[i];
        redLEDBuf[i - 25] = redLEDBuf[i];
    }

    // take a new 25 samples
    for (uint8_t i = 75; i < POBufferSize; i++)
    {
        while (!pulseOxSensor.available())
        {
            pulseOxSensor.check();
        }

        // when a reading is received, save it to the buffers
        redLEDBuf[i] = pulseOxSensor.getRed();
        irLEDBuf[i] = pulseOxSensor.getIR();
        // advance the queue
        pulseOxSensor.nextSample();
    }

    // take reading for oximeter, ignore heart rate
    maxim_heart_rate_and_oxygen_saturation(irLEDBuf,
                                           POBufferSize,
                                           redLEDBuf,
                                           &spo2,
                                           &validSPO2,
                                           &heartRate,
                                           &validHeartRate);

    // take reading for temperature
    temperature = temp_CtoF(tempSensor.getTemperature());

#ifdef DEBUG
    Serial.print(("Heart Rate True -> "));
    Serial.println((heartRate));

    Serial.print(("SPO2 -> "));
    Serial.println((spo2));

    Serial.print(("Temp. -> "));
    Serial.println(temperature);
#endif

#ifdef SCREEN_CONN
    tft.setSwapBytes(false);
    tft.setTextSize(3);
    tft.fillScreen(TFT_WHITE);

    char hrB[10];
    itoa(heartRate, hrB, 10);

    char oxiB[10];
    itoa(spo2, oxiB, 10);

    char tempB[10];
    dtostrf(temperature, 4, 2, tempB);

    tft.setCursor(65, 85);
    tft.setTextColor(TFT_RED);
    tft.print("HR:");
    tft.setCursor(115, 85);
    tft.print(hrB);

    tft.setCursor(65, 115);
    tft.setTextColor(TFT_BLUE);
    tft.print("OX:");
    tft.setCursor(115, 115);
    tft.print(oxiB);

    tft.setCursor(65, 145);
    tft.setTextColor(TFT_BLACK);
    tft.print("TF:");
    tft.setCursor(115, 145);
    tft.print(tempB);
#endif

    if (wifi_enabled && WiFi.status() != WL_CONNECTED)
    {
        wifi_setup();
    }
    else if (WiFi.status() == WL_CONNECTED && millis() - firebaseTimer > WIFI_TIMER && Firebase.ready() && firebase_enabled)
    {

#ifdef DEBUG
        Serial.println("Sending to Firebase");
#endif

        firebaseTimer = millis();

        Firebase.RTDB.setIntAsync(&FBDO, "mainData/heart_rate", heartRate);
        sleep(.5);
        Firebase.RTDB.setIntAsync(&FBDO, "mainData/spo2", spo2);
        sleep(.5);
        Firebase.RTDB.setFloatAsync(&FBDO, "mainData/body_temp", temperature);
    }
}