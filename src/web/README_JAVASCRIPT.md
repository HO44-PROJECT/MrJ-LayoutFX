# JavaScript Files — MrJ-LayoutFX WebUI

## Vue d'ensemble

Tous les fichiers JavaScript du projet suivent maintenant un format d'en-tête standardisé conforme au reste du projet.

---

## Format d'en-tête standard

```javascript
/**
 * @file <filename>.js
 * @brief <description courte>
 *
 * <description détaillée facultative>
 *
 * @project MrJ-LayoutFX
 * @repo https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */
```

---

## Liste des fichiers JavaScript standardisés

### **1. app.js**
Main SPA controller, navigation, et architecture globale.
- Cockpit view (device cards + polling)
- Config view (boards, buses, files)
- About view (system info)

### **2. app-core.js**
Cockpit view core : globals, device card renderers, filter, grid, polling.

### **3. app-config.js**
Configuration management et device state actions.
- Config file upload/download
- Device state management
- Debug data loading

### **4. app-device-editor.js**
Modal d'édition de devices individuels.
- Création/modification devices
- Pin selection
- Type picker
- Parameter configuration

### **5. app-board-editor.js**
Modal d'édition de boards (I2C/SPI expansion).
- PCA9685, MCP23017, 74HC595
- Board configuration

### **6. app-boards.js**
I2C scanner et board management.
- I2C device detection
- Board listing
- Bus configuration

### **7. app-wizard.js**
Setup wizard et utilitaires de configuration.
- First-boot wizard
- Configuration reset
- Code export

### **8. app-about.js**
Panel About avec informations système.
- Firmware version
- Hardware info
- Memory usage
- Project links

### **9. icons.js**
Définitions SVG pour tous les types de devices.
- Railway signals
- Lamps (gas, electric, oil, etc.)
- Beacons
- Traffic lights
- Effects (storm, campfire, etc.)

### **10. i18n.js**
Internationalisation (French tooltips).
- Device type translations
- UI labels
- French/English support

---

## Architecture du code

### **Dépendances entre fichiers**

```
app.js (main controller)
  ├─ app-core.js (cockpit view)
  ├─ app-config.js (config management)
  ├─ app-device-editor.js (device modal)
  ├─ app-board-editor.js (board modal)
  ├─ app-boards.js (I2C scanner)
  ├─ app-wizard.js (setup wizard)
  ├─ app-about.js (about panel)
  ├─ icons.js (SVG definitions)
  └─ i18n.js (translations)
```

### **Build pipeline**

Tous les fichiers JS sont :
1. Concaténés par `build_webui.py`
2. Minifiés avec `rjsmin` (commentaires supprimés)
3. Injectés dans `webui.html`
4. Compressés en gzip
5. Convertis en tableau C++ (`webui_html_gz[]`)

---

## Conventions de code

### **Globals prefixés**
```javascript
var _dbgDevs = [];      // runtime device list
var _dbgCfg = null;     // loaded config
var _boardTypes = null; // board catalogue
var _deviceTypes = null;// device catalogue
```

### **Fonctions nommées**
```javascript
function openDevEditor() { ... }
function scanI2c() { ... }
function uploadConfig() { ... }
```

### **Commentaires**
- En-tête de fichier : format JSDoc standardisé
- Commentaires inline : explicatifs uniquement (minifiés à la build)
- Pas de commentaires TODO/FIXME en production

### **Format des chaînes**
```javascript
// OK
var msg = 'Device created';

// Éviter (moins lisible après minification)
var msg = "Device created";
```

---

## Ajout d'un nouveau fichier JavaScript

1. **Créer le fichier** dans `src/web/`
2. **Ajouter l'en-tête standardisé** :
   ```javascript
   /**
    * @file app-newfeature.js
    * @brief Description courte de la fonctionnalité
    *
    * Description détaillée (optionnelle)
    *
    * @project MrJ-LayoutFX
    * @repo https://github.com/HO44-PROJECT/MrJ-LayoutFX
    * @license MIT License — Copyright (c) 2026 HO44 PROJECT
    */
   ```

3. **Ajouter au build** :
   - Modifier `tools/build_webui.py` si nécessaire
   - Ordre de concaténation : dépendances en premier

4. **Tester** :
   ```bash
   python tools/build_webui.py
   pio run -e <env> --target upload
   ```

---

## Maintenance

### **Vérification des en-têtes**

```bash
# Lister tous les fichiers JS
find src/web -name "*.js"

# Vérifier que tous ont l'en-tête standard
grep -L "@project MrJ-LayoutFX" src/web/*.js
```

### **Format automatique** (si configuré)

```bash
# Prettier (si installé)
prettier --write src/web/*.js

# ESLint (si installé)
eslint --fix src/web/*.js
```

---

## Références

- **Build script** : `tools/build_webui.py`
- **HTML template** : `src/web/webui.html`
- **WebUI server** : `src/api/WebUI.cpp`
- **Documentation API** : voir `src/api/README.md`

---

**Date de standardisation** : 2026-06-02  
**Version** : 1.0  
**Projet** : MrJ-LayoutFX
