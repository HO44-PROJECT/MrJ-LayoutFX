# Library Data Files — MrJ-RailwayFX

## Vue d'ensemble

Ce répertoire contient les **fichiers de données structurelles de la bibliothèque** — catalogues de types de devices, boards, buses et chips I2C connus.

Ces fichiers définissent le **vocabulaire** et les **métadonnées** utilisés par le framework.

---

## Fichiers

### **1. device_types.json**
Catalogue de tous les types de devices supportés par la bibliothèque.

**Contenu :**
- Types de devices (Led, GasLamp, signaux DB, traffic lights, etc.)
- Paramètres par type (nombre de pins, style de constructeur)
- États possibles (pour signaux et servos)
- Catégories (light, signal, traffic, servo, etc.)

**Utilisé par :**
- WebUI : sélecteur de type de device
- ConfigManager : validation des devices dans config.json
- DeviceFactory : création des instances de devices
- API : endpoint `/api/device-types`

**Schéma :** `../schemas/device_types.schema.json`

---

### **2. board_types.json**
Catalogue de tous les types de boards supportés (MCU + expansion boards).

**Contenu :**
- Boards ESP32 (DevKit, Mini, custom PCB)
- Expansion boards I2C (PCA9685, MCP23017, OLED)
- Expansion boards SPI (74HC595)
- Pinout physique (labels, GPIO, capabilities)
- Buses pré-câblés (I2C, SPI, UART, DCC)

**Utilisé par :**
- WebUI : éditeur de boards (diagramme DIP)
- ConfigManager : validation des boards dans config.json
- API : endpoint `/api/board-types`

**Schéma :** `../schemas/board_types.schema.json`

---

### **3. bus_types.json**
Catalogue des types de bus de communication supportés.

**Contenu :**
- Types de bus (SPI, UART, I2C, DCC)
- Paramètres configurables par type
- Validation des pins (GPIO ranges)

**Utilisé par :**
- WebUI : configuration des buses
- ConfigManager : validation des buses dans config.json
- BusRegistry : initialisation des buses
- API : endpoint `/api/bus-types`

**Schéma :** `../schemas/bus_types.schema.json`

---

### **4. i2c_known.json**
Catalogue des chips I2C reconnus avec leurs adresses standard.

**Contenu :**
- Mapping adresse I2C (7-bit décimal) → nom du chip
- Exemple : `"64": "PCA9685"` (0x40 en hexa)

**Utilisé par :**
- WebUI : I2C scanner (affichage des noms de chips)
- API : endpoint `/api/i2c-known`

**Schéma :** `../schemas/i2c_known.schema.json`

---

## Emplacement et distribution

### **Source de vérité :**
```
lib/MrJ-RailwayFX.local/data/  ← Ici (partie de la bibliothèque)
```

### **Déployé sur ESP32 (LittleFS) :**
Ces fichiers doivent être uploadés sur l'ESP32 via **PlatformIO → Upload Filesystem Image**.

Le répertoire `data/` du **projet racine** doit contenir des copies ou liens symboliques vers ces fichiers pour que l'upload LittleFS fonctionne.

**Chemins LittleFS sur ESP32 :**
```
/board_types.json   → servi par /api/board-types
/device_types.json  → servi par /api/device-types
/bus_types.json     → servi par /api/bus-types
/i2c_known.json     → servi par /api/i2c-known
```

---

## Modification des fichiers

### **Format**

Tous les fichiers suivent le format JSON avec validation par schéma :

```json
{
  "$schema": "../schemas/<type>.schema.json",
  "<key>": {
    // ... définition ...
  }
}
```

### **Validation**

**Dans VSCode :**
- Ouvrir un fichier → validation automatique
- Erreurs affichées en temps réel
- Autocomplétion disponible

**En ligne de commande :**
```bash
# Installer ajv-cli
npm install -g ajv-cli

# Valider un fichier
ajv validate -s ../schemas/device_types.schema.json -d device_types.json
```

### **Workflow de modification**

1. **Modifier** le fichier dans `lib/MrJ-RailwayFX.local/data/`
2. **Valider** avec VSCode ou ajv-cli
3. **Copier** vers le `data/` du projet (si pas de lien symbolique)
4. **Uploader** sur ESP32 via PlatformIO
5. **Tester** dans la WebUI

---

## Ajout d'un nouveau type

### **Nouveau device type :**

1. **Ajouter l'entrée** dans `device_types.json` :
   ```json
   "MyNewDevice": {
     "category": "light",
     "wires": 1,
     "ctor_style": "single_pin"
   }
   ```

2. **Créer la classe C++** dans `lib/src/devices/` ou `lib/src/led_fx/`

3. **Ajouter au DeviceFactory** dans `src/config/DeviceFactory.cpp`

4. **Ajouter l'icône SVG** dans `src/web/icons.js`

5. **Uploader** les fichiers data sur ESP32

### **Nouveau board type :**

1. **Ajouter l'entrée** dans `board_types.json` avec pinout complet

2. **Tester** dans la WebUI (éditeur de boards)

3. **Uploader** sur ESP32

---

## Structure complète du projet

```
lib/MrJ-RailwayFX.local/
├── data/                       ← Données structurelles (ici)
│   ├── board_types.json
│   ├── device_types.json
│   ├── bus_types.json
│   ├── i2c_known.json
│   └── README.md
├── schemas/                    ← Validation JSON Schema
│   ├── board_types.schema.json
│   ├── device_types.schema.json
│   ├── bus_types.schema.json
│   └── i2c_known.schema.json
├── include/
├── src/
└── ...

examples/                       ← Configurations d'exemple
├── config_esp32mini.json
├── config_2_ext.json
└── config_spi_sample.json

data/                           ← Config utilisateur + upload LittleFS
├── board_types.json            → lien/copie vers lib/data/
├── device_types.json           → lien/copie vers lib/data/
├── bus_types.json              → lien/copie vers lib/data/
├── i2c_known.json              → lien/copie vers lib/data/
└── config.json                 ← Config utilisateur
```

---

## API Endpoints

Ces fichiers sont servis par l'API REST du framework :

| Endpoint | Fichier | Description |
|----------|---------|-------------|
| `GET /api/device-types` | `device_types.json` | Catalogue devices |
| `GET /api/board-types` | `board_types.json` | Catalogue boards |
| `GET /api/bus-types` | `bus_types.json` | Catalogue buses |
| `GET /api/i2c-known` | `i2c_known.json` | Chips I2C connus |

**Code source :** `lib/src/api/DeviceStatusApi.cpp`

---

## Références

- **Schémas JSON** : `../schemas/`
- **API Server** : `../src/api/`
- **DeviceFactory** : `../src/config/DeviceFactory.cpp`
- **WebUI** : `../src/web/`

---

**Version** : 1.0  
**Date** : 2026-06-02  
**Projet** : MrJ-ArduinoRailwayFX  
**Licence** : MIT License — Copyright (c) 2026 HO44 PROJECT
