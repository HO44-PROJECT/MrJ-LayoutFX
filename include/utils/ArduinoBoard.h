#pragma once

// -----------------------------------------------------------------------------
// Définitions des macros pour les plateformes Arduino
//
// Ces macros sont définies automatiquement par l'environnement de compilation
// et peuvent être utilisées pour adapter le code à la carte cible.
// -----------------------------------------------------------------------------

#if defined(ARDUINO_AVR_NANO)
    // Arduino Nano
    // #define ARDUINO_BOARD_NAME "Arduino Nano"
    #define MAX_PIN_NUMBER 20 // Broches D0-D13 et A0-A5 (14 + 6)
#elif defined(ARDUINO_AVR_UNO)
    // Arduino Uno, Duemilanove, ou équivalents basés sur ATmega328P
    // #define ARDUINO_BOARD_NAME "Arduino Uno"
    #define MAX_PIN_NUMBER 20 // Broches D0-D13 et A0-A5 (14 + 6)
#elif defined(ARDUINO_AVR_MEGA2560)
    // Arduino Mega 2560
    // #define ARDUINO_BOARD_NAME "Arduino Mega 2560"
    #define MAX_PIN_NUMBER 70 // Broches D0-D53 et A0-A15 (54 + 16)
#elif defined(ARDUINO_SAM_DUE)
    // Arduino Due
    // #define ARDUINO_BOARD_NAME "Arduino Due"
    #define MAX_PIN_NUMBER 66 // Broches D0-D53 et A0-A11 (54 + 12)
#elif defined(ESP32)
    // Plateformes ESP32 (définies par l'environnement de compilation ESP-IDF ou Arduino-ESP32)
    // #define ARDUINO_BOARD_NAME "ESP32"
    #define MAX_PIN_NUMBER 40 // Le nombre exact de broches peut varier, mais cette valeur est une estimation sécuritaire
#elif defined(ESP8266)
    // Plateformes ESP8266
    // #define ARDUINO_BOARD_NAME "ESP8266"
    #define MAX_PIN_NUMBER 17 // Broches D0 à D16
#elif defined(TEENSYDUINO)
    // Plateformes Teensy
    // #define ARDUINO_BOARD_NAME "Teensy"
    // Le nombre de broches varie, il est donc préférable de le définir pour un modèle spécifique
    #define MAX_PIN_NUMBER 46 // Pour un Teensy 4.0 par exemple
#else
    // Si la plateforme n'est pas reconnue
    #warning "Plateforme Arduino non reconnue. La macro MAX_PIN_NUMBER n'a pas été définie."
    #define ARDUINO_BOARD_NAME "Unknown"
    #define MAX_PIN_NUMBER 0
#endif
