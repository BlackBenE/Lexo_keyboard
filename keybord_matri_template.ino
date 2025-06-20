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

// --- Configuration BLE ---
#define SERVICE_UUID        "fabb9cc5-6e51-410e-815f-ce3407eb71f5" // UUID de votre service personnalisé
#define CHARACTERISTIC_UUID "036881af-a2d0-4bef-8d4a-de290627c11f" // UUID de la caractéristique pour les données des touches

BLEServer* pServer = NULL;             
BLECharacteristic* pCharacteristic = NULL;

bool deviceConnected = false;           
bool oldDeviceConnected = false;        

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("Client BLE connecté !");
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("Client BLE déconnecté !");
      BLEDevice::startAdvertising();
    }
};

void setup() {
  Serial.begin(115200); 
  Serial.println("Démarrage du serveur BLE pour le clavier...");

  BLEDevice::init("MyLeXoKeyboard");

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

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID); // Annonce que notre service est disponible
  pAdvertising->setScanResponse(true);        // Permet l'envoi d'une réponse de scan plus détaillée
  pAdvertising->setMinPreferred(0x06);        // Aide à une connexion plus rapide pour certains clients
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("Advertising BLE démarré. En attente d'un client pour se connecter...");
}

void loop() {
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
