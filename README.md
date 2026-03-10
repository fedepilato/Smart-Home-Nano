# Smart Home IoT System

Questo progetto implementa un sistema IoT composto da un nodo edge (Arduino) e un pannello di controllo (Python). Il sistema monitora l'ambiente (temperatura, presenza, rumore) e controlla vari attuatori (ventola, LED, display LCD) utilizzando i protocolli REST e MQTT (formato SenML).

## Architettura del Progetto

Il progetto è diviso in due macro-componenti:

1. **Codice Arduino (Nodo Edge):** - Raccoglie dati da sensori di temperatura, microfono (PDM) e sensore di movimento (PIR).
   - Implementa logica locale (es. accensione luce tramite rilevamento del battito di mani).
   - Regola dinamicamente le soglie di riscaldamento e illuminazione in base alla presenza di persone.
   - Comunica i dati e riceve comandi tramite MQTT.
   - Mostra informazioni di stato su un display LCD I2C.

2. **Client Python (Pannello di Controllo):**
   - Si registra tramite API REST per ottenere le credenziali del broker MQTT.
   - Fornisce un'interfaccia a riga di comando (CLI) interattiva.
   - Permette di iscriversi ai topic per monitorare in tempo reale i sensori.
   - Invia comandi di attuazione per accendere/spegnere i LED, regolare la velocità della ventola o stampare messaggi sul display LCD.

## 🛠️ Requisiti Hardware
* Scheda compatibile con `WiFiNINA` e microfono integrato (es. Arduino Nano RP2040 Connect).
* Sensore di temperatura (analogico).
* Sensore di presenza PIR.
* Ventola (guidata tramite PWM) e LED.
* Display LCD con modulo I2C (PCF8574).

## 💻 Installazione e Configurazione

### Configurazione Arduino
1. Apri lo sketch `.ino` con l'IDE di Arduino.
2. Assicurati di avere installato le seguenti librerie tramite il Library Manager:
   - `WiFiNINA`
   - `PubSubClient`
   - `ArduinoJson`
   - `ArduinoHttpClient`
   - `PDM`
   - `LiquidCrystal_PCF8574`
3. **Gestione Credenziali:** Rinomina il file `secrets_template.h` (se presente) in `secrets.h` e inserisci le credenziali della tua rete Wi-Fi:
   ```cpp
   #define USERID "IL_TUO_SSID"
   #define PASSWORD "LA_TUA_PASSWORD"