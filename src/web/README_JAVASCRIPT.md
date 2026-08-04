# JavaScript Files — MrJ-LayoutFX WebUI

## Overview

All JavaScript files in the project now follow a standardized header format consistent with the rest of the project.

---

## Standard header format

```javascript
/**
 * @file <filename>.js
 * @brief <short description>
 *
 * <optional detailed description>
 *
 * @project MrJ-LayoutFX
 * @repo https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @license AGPL-3.0-or-later — Copyright (c) 2026 HO44 PROJECT
 */
```

---

## List of standardized JavaScript files

There is no `app.js` file: `build_webui.py` concatenates the `app-*.js`
modules below, in `APP_MODULES` order, directly into the final bundle —
there is no separate "main controller".

### **1. app-pure.js**
Pure helpers, no DOM or fetch (input data → output data). The only
module tested by the native Node tests (`test/web/`) — a CommonJS guard at
the bottom of the file exports the functions for tests without affecting the
browser bundle.

### **2. app-core.js**
Cockpit view core: globals, device card renderers, filter, grid, polling.

### **3. app-wizard.js**
Setup wizard and configuration utilities.
- First-boot wizard
- Configuration reset
- Code export

### **4. app-config.js**
Configuration management and device state actions.
- Config file upload/download (client-side validation via `validate_config.js`, generated — see [build-pipeline.md](../../doc/contributing/build-pipeline.md))
- Device state management
- Debug data loading

### **5. app-boards.js**
I2C scanner and board management.
- I2C device detection
- Board listing
- Bus configuration

### **6. app-dcc.js**
Diagnostics tab: live DCC activity indicators (polls `/api/dcc-status`).
- One indicator per message category (bus/speed/func/accessory/signal)
- Distinguishes a "dead bus" from a "bus alive but ignored by this decoder"

### **7. app-about.js**
About panel with system information.
- Firmware version
- Hardware info
- Memory usage
- Project links

### **8. app-device-editor.js**
Modal for editing individual devices.
- Device creation/editing
- Pin selection
- Type picker
- Parameter configuration

### **9. app-board-editor.js**
Modal for editing boards (I2C/SPI expansion), the bus editor, and the boot sequence.
- PCA9685, MCP23017, 74HC595
- Board configuration

### **10. icons.js**
SVG definitions for all device types.
- Railway signals
- Lamps (gas, electric, oil, etc.)
- Beacons
- Traffic lights
- Effects (storm, campfire, etc.)

### **11. i18n.js**
Internationalization (fr/de/es/en).
- Device type translations
- UI labels

---

## Code architecture

### **Concatenation order (`APP_MODULES` in `build_webui.py`)**

```
style.css + i18n.js + icons.js (markers %%STYLE%%/%%I18N%%/%%ICONS%%)
  └─ app-pure.js         (pure helpers, tested in Node)
  └─ app-core.js         (cockpit view)
  └─ app-wizard.js        (setup wizard)
  └─ app-config.js        (config management)
  └─ app-boards.js         (I2C scanner)
  └─ app-dcc.js             (DCC diagnostics)
  └─ app-about.js            (about panel)
  └─ app-device-editor.js     (device modal)
  └─ app-board-editor.js       (board/bus modal)
  [+ validate_config.js, generated, if present — see build-pipeline.md]
```

### **Build pipeline**

All JS files are:
1. Concatenated by `build_webui.py`
2. Minified with `rjsmin` (comments stripped)
3. Injected into `webui.html`
4. Compressed with gzip
5. Converted to a C++ array (`webui_html_gz[]`)

---

## Code conventions

### **Prefixed globals**
```javascript
var _dbgDevs = [];      // runtime device list
var _dbgCfg = null;     // loaded config
var _boardTypes = null; // board catalogue
var _deviceTypes = null;// device catalogue
```

### **Named functions**
```javascript
function openDevEditor() { ... }
function scanI2c() { ... }
function uploadConfig() { ... }
```

### **Comments**
- File header: standardized JSDoc format
- Inline comments: explanatory only (stripped on minification)
- No TODO/FIXME comments in production

### **String formatting**
```javascript
// OK
var msg = 'Device created';

// Avoid (less readable after minification)
var msg = "Device created";
```

---

## Adding a new JavaScript file

1. **Create the file** in `src/web/`
2. **Add the standardized header**:
   ```javascript
   /**
    * @file app-newfeature.js
    * @brief Short description of the feature
    *
    * Detailed description (optional)
    *
    * @project MrJ-LayoutFX
    * @repo https://github.com/HO44-PROJECT/MrJ-LayoutFX
    * @license AGPL-3.0-or-later — Copyright (c) 2026 HO44 PROJECT
    */
   ```

3. **Add it to the build**:
   - Update `tools/build_webui.py` if needed
   - Concatenation order: dependencies first

4. **Test**:
   ```bash
   python tools/build_webui.py
   pio run -e <env> --target upload
   ```

---

## Maintenance

### **Header check**

```bash
# List all JS files
find src/web -name "*.js"

# Check that all have the standard header
grep -L "@project MrJ-LayoutFX" src/web/*.js
```

### **Automatic formatting** (if configured)

```bash
# Prettier (if installed)
prettier --write src/web/*.js

# ESLint (if installed)
eslint --fix src/web/*.js
```

---

## References

- **Build script**: `tools/build_webui.py`
- **HTML template**: `src/web/webui.html`
- **WebUI server**: `src/api/WebUI.cpp`
- **API documentation**: see `src/api/README.md`

---

**Standardization date**: 2026-06-02
**Version**: 1.0
**Project**: MrJ-LayoutFX
