#include <Keypad.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h> 

// --- Configuration du clavier matriciel ---
#define ROWS  2 
#define COLS  2 

char keyMap[ROWS][COLS] = {
  {'1','2'},
  {'3','4'},
};

uint8_t rowPins[ROWS] = {5, 21}; 
uint8_t colPins[COLS] = {18, 19};  

Keypad keypad = Keypad(makeKeymap(keyMap), rowPins, colPins, ROWS, COLS );

// --- Configuration LED indicateur ---
#define LED_PIN 2  // LED intégrée sur la plupart des ESP32
#define LED_BLINK_INTERVAL 500  // Intervalle de clignotement en ms

// --- Configuration BLE ---
#define SERVICE_UUID        "fabb9cc5-6e51-410e-815f-ce3407eb71f5" // UUID de votre service personnalisé
#define CHARACTERISTIC_UUID "036881af-a2d0-4bef-8d4a-de290627c11f" // UUID de la caractéristique pour les données des touches

// --- Configuration des informations du fabricant ---
#define MANUFACTURER_ID     0x02E5  // ID du fabricant (exemple: 0x02E5 pour Espressif)
#define DEVICE_NAME         "LeXo Keyboard"
#define DEVICE_MODEL        "LeXo-KB-001"
#define FIRMWARE_VERSION    "1.0.0"
#define HARDWARE_VERSION    "1.0"

// Données du fabricant (peut inclure des informations personnalisées)
uint8_t manufacturerData[] = {
  0x4C, 0x65, 0x58, 0x6F,  // "LeXo" en ASCII
  0x01, 0x00,              // Version du firmware (1.0)
  0x4B, 0x42, 0x2D, 0x30, 0x30, 0x31  // "KB-001" en ASCII
};

BLEServer* pServer = NULL;             
BLECharacteristic* pCharacteristic = NULL;

bool deviceConnected = false;           
bool oldDeviceConnected = false;        

// Variables pour la gestion de la LED
unsigned long lastLedToggle = 0;
bool ledState = false;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("Client BLE connecté !");
      // Allumer la LED en continu quand connecté
      digitalWrite(LED_PIN, HIGH);
      ledState = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("Client BLE déconnecté !");
      // Éteindre la LED quand déconnecté
      digitalWrite(LED_PIN, LOW);
      ledState = false;
      BLEDevice::startAdvertising();
    }
};

// Fonction pour gérer l'état de la LED
void updateLedStatus() {
  if (deviceConnected) {
    // LED allumée en continu quand connecté
    digitalWrite(LED_PIN, HIGH);
  } else {
    // LED clignote quand en attente de connexion
    unsigned long currentTime = millis();
    if (currentTime - lastLedToggle >= LED_BLINK_INTERVAL) {
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
      lastLedToggle = currentTime;
    }
  }
}

void setup() {
  Serial.begin(115200); 
  Serial.println("Démarrage du serveur BLE pour le clavier...");

  // Configuration de la LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // Éteindre la LED au démarrage

  // Initialisation du dispositif BLE avec le nom personnalisé
  BLEDevice::init(DEVICE_NAME);

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks()); 

  BLEService *pService = pServer->createService(SERVICE_UUID);

  // 4. Création de la caractéristique BLE pour l'envoi des données des touches.
  //   - CHARACTERISTIC_UUID: L'identifiant unique de cette caractéristique.
  //   - PROPERTY_READ: Permet aux clients de lire la valeur actuelle.
  //   - PROPERTY_NOTIFY: C'est la propriété clé qui permet à l'ESP32 d'envoyer
  //                      des mises à jour (notifications) au client abonné.
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );

  pCharacteristic->addDescriptor(new BLE2902());

  pService->start();

  // Ajout du service d'information sur le dispositif (Device Information Service)
  BLEService *pDISService = pServer->createService("180A"); // UUID standard pour DIS
  
  // Caractéristique pour le nom du fabricant
  BLECharacteristic *pManufacturerName = pDISService->createCharacteristic(
    "2A29", // UUID standard pour Manufacturer Name String
    BLECharacteristic::PROPERTY_READ
  );
  pManufacturerName->setValue("LeXo");
  
  // Caractéristique pour le numéro de modèle
  BLECharacteristic *pModelNumber = pDISService->createCharacteristic(
    "2A24", // UUID standard pour Model Number String
    BLECharacteristic::PROPERTY_READ
  );
  pModelNumber->setValue(DEVICE_MODEL);
  
  // Caractéristique pour le numéro de série
  BLECharacteristic *pSerialNumber = pDISService->createCharacteristic(
    "2A25", // UUID standard pour Serial Number String
    BLECharacteristic::PROPERTY_READ
  );
  pSerialNumber->setValue("SN001");
  
  // Caractéristique pour la version du firmware
  BLECharacteristic *pFirmwareRevision = pDISService->createCharacteristic(
    "2A26", // UUID standard pour Firmware Revision String
    BLECharacteristic::PROPERTY_READ
  );
  pFirmwareRevision->setValue(FIRMWARE_VERSION);
  
  // Caractéristique pour la version du hardware
  BLECharacteristic *pHardwareRevision = pDISService->createCharacteristic(
    "2A27", // UUID standard pour Hardware Revision String
    BLECharacteristic::PROPERTY_READ
  );
  pHardwareRevision->setValue(HARDWARE_VERSION);
  
  // Caractéristique pour le nom du logiciel
  BLECharacteristic *pSoftwareRevision = pDISService->createCharacteristic(
    "2A28", // UUID standard pour Software Revision String
    BLECharacteristic::PROPERTY_READ
  );
  pSoftwareRevision->setValue("LeXo Keyboard Firmware v1.0");
  
  pDISService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  
  // Ajout des données du fabricant
  pAdvertising->addManufacturerData(MANUFACTURER_ID, manufacturerData, sizeof(manufacturerData));
  
  // Configuration de l'apparence du dispositif (clavier)
  // 0x03C0 = HID Keyboard (selon la spécification GAP)
  pAdvertising->setAppearance(0x03C0);
  
  // Ajout du nom complet du dispositif dans les données d'advertising
  pAdvertising->setName(DEVICE_NAME);
  
  pAdvertising->addServiceUUID(SERVICE_UUID); // Annonce que notre service est disponible
  pAdvertising->setScanResponse(true);        // Permet l'envoi d'une réponse de scan plus détaillée
  pAdvertising->setMinPreferred(0x06);        // Aide à une connexion plus rapide pour certains clients
  pAdvertising->setMinPreferred(0x12);
  
  // Configuration des paramètres d'advertising pour une meilleure visibilité
  pAdvertising->setAdvertisementType(BLE_GAP_CONN_MODE_NON);
  
  BLEDevice::startAdvertising();

  Serial.println("Advertising BLE démarré. En attente d'un client pour se connecter...");
  Serial.println("LED clignote = En attente de connexion");
  Serial.println("LED allumée = Client connecté");
  Serial.print("Nom du dispositif: ");
  Serial.println(DEVICE_NAME);
  Serial.print("Modèle: ");
  Serial.println(DEVICE_MODEL);
  Serial.print("Version firmware: ");
  Serial.println(FIRMWARE_VERSION);
}

void loop() {
  // Mise à jour de l'état de la LED
  updateLedStatus();
  
  char key = keypad.getKey(); 

  if (key) { 
    Serial.print("Touche pressée détectée : ");
    Serial.println(key);

    if (deviceConnected) { 
      String message = String(key); // Convertit le caractère en une chaîne de caractères
      pCharacteristic->setValue(message.c_str()); // Définit la valeur de la caractéristique
      pCharacteristic->notify(); // Envoie une notification au client abonné
      Serial.print("Notification BLE envoyée : ");
      Serial.println(message);
    } else {
      Serial.println("Aucun client BLE connecté pour envoyer la touche.");
    }
  }


  if (!deviceConnected && oldDeviceConnected) {
    delay(500); 
    pServer->startAdvertising(); 
    Serial.println("Redémarrage de l'advertising.");
    oldDeviceConnected = deviceConnected;
  }
  if (deviceConnected && !oldDeviceConnected) {
    Serial.println("Appareil BLE connecté pour la première fois (ou après reconnexion).");
    oldDeviceConnected = deviceConnected;
  }

  delay(50); 
}
