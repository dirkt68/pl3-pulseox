#include <Arduino.h>
#include <WiFiManager.h>
#include <WiFi.h>

#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

#define DEBUG

// define stuff to log in to firebase
#define API_KEY "AIzaSyDrjThqfnejA6Lc12Lwnbxfnrqdf2X1TZ0"
#define DB_URL "https://console.firebase.google.com/project/project-lab-3-45cf8/database/project-lab-3-45cf8-default-rtdb/data/~2F"

/*------------------------------- GLOBALS ---------------------------------*/
bool wifi_enabled = false;
bool firebase_enabled = false;
FirebaseData FBDO;
FirebaseAuth auth;
FirebaseConfig config;


void wifi_setup() {
    wifi_enabled = false;

    #ifdef SCREEN_CONN
        //TODO: DISPLAY WIFI SYMBOL
    #endif

    Serial.println("Starting WiFi");

    // connect to the wifi using Wifi Manager
    WiFiManager wm;
    wm.resetSettings();
    wm.setClass("invert");
    Serial.println("wm active");
    bool result = wm.autoConnect("LifeMTR");

    #ifdef DEBUG
        if (!result){
            Serial.println("Couldn't connect to wifi");
        }
    #endif

    Serial.println("activating Firebase");

    if (Firebase.signUp(&config, &auth, "", "")) {
        firebase_enabled = true;
    }

    Serial.println("begin Firebase"); 
    Firebase.begin(&config, &auth);

    #ifdef SCREEN_CONN
        //TODO: TURN OFF WIFI SYMBOL
    #endif
}

void setup() {
    // put your setup code here, to run once:
    Serial.begin(115200);
    wifi_setup();


}

void loop() {
    Serial.println("Testing");
    delay(1);

    Firebase.RTDB.setIntAsync(&FBDO, "mainData/heart_rate", 10);
    Firebase.RTDB.setIntAsync(&FBDO, "mainData/spo2", 20);
    Firebase.RTDB.setFloatAsync(&FBDO, "mainData/body_temp", 30);
}