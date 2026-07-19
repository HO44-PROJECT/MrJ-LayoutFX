# JSON Schemas — MrJ-LayoutFX

## Vue d'ensemble

Ce répertoire contient les schémas JSON pour valider les fichiers de configuration et de données du projet MrJ-LayoutFX.

---

## Schémas disponibles

### **1. config.schema.json**
Schéma de validation pour les fichiers de configuration principaux (`config.json`).

**Valide :**
- Structure des devices
- Configuration des boards (I2C, SPI)
- Paramètres des buses
- Mapping DCC

**Fichiers validés :**
- `data/config.json`
- `data/config_*.json`

---

### **2. device_types.schema.json**
Schéma de validation pour le catalogue de types de devices (`device_types.json`).

**Définit :**
- Types de devices disponibles (Led, GasLamp, signals, etc.)
- Paramètres par type
- Valeurs par défaut
- Contraintes de validation

**Fichiers validés :**
- `data/device_types.json`

---

### **3. board_types.schema.json**
Schéma de validation pour le catalogue de types de boards (`board_types.json`).

**Définit :**
- Types de boards d'extension (PCA9685, MCP23017, etc.)
- Nombre de pins/channels
- Protocoles supportés (I2C, SPI)
- Adresses I2C par défaut

**Fichiers validés :**
- `data/board_types.json`

---

### **4. bus_types.schema.json**
Schéma de validation pour le catalogue de types de bus (`bus_types.json`).

**Définit :**
- Types de bus disponibles (I2C, SPI)
- Paramètres de configuration
- Modes d'adressage

**Fichiers validés :**
- `data/bus_types.json`

---

### **5. i2c_known.schema.json**
Schéma de validation pour le catalogue de devices I2C connus (`i2c_known.json`).

**Définit :**
- Liste des chips I2C reconnus
- Adresses I2C standard
- Noms commerciaux
- Usages typiques

**Fichiers validés :**
- `data/i2c_known.json`

---

## Utilisation dans VSCode

Les schémas sont automatiquement appliqués via `.vscode/settings.json` :

```json
{
  "json.schemas": [
    {
      "fileMatch": ["data/config.json", "**/config.json"],
      "url": "./lib/MrJ-RailwayFX.local/schemas/config.schema.json"
    },
    // ... autres schémas
  ]
}
```

Lors de l'édition d'un fichier JSON correspondant, VSCode :
- ✅ Valide la syntaxe en temps réel
- ✅ Propose l'autocomplétion
- ✅ Affiche les erreurs de validation
- ✅ Fournit la documentation inline (descriptions)

---

## Validation manuelle

### **Avec VSCode**
Ouvrir un fichier JSON → les erreurs s'affichent automatiquement dans l'éditeur.

### **Avec JSON Schema Validator (CLI)**

```bash
# Installer le validateur
npm install -g ajv-cli

# Valider un fichier
ajv validate -s schemas/config.schema.json -d data/config.json
```

### **Avec Python**

```python
import json
import jsonschema

# Charger le schéma
with open('lib/MrJ-RailwayFX.local/schemas/config.schema.json') as f:
    schema = json.load(f)

# Charger les données
with open('data/config.json') as f:
    data = json.load(f)

# Valider
try:
    jsonschema.validate(data, schema)
    print("✅ Validation OK")
except jsonschema.ValidationError as e:
    print(f"❌ Erreur: {e.message}")
```

---

## Modification des schémas

### **Format standard**

Tous les schémas suivent le standard **JSON Schema Draft 2020-12** :

```json
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "$id": "https://github.com/HO44-PROJECT/MrJ-LayoutFX/schemas/config.schema.json",
  "title": "MrJ-LayoutFX Configuration",
  "description": "...",
  "type": "object",
  "properties": { ... },
  "required": [ ... ]
}
```

### **Bonnes pratiques**

1. **Toujours incrémenter `$id`** si changement majeur
2. **Ajouter des `description`** pour chaque propriété (aide VSCode)
3. **Utiliser `examples`** pour les cas d'usage courants
4. **Définir `additionalProperties: false`** pour détecter les typos
5. **Tester avec des fichiers réels** après modification

### **Après modification**

1. Valider le schéma lui-même :
   ```bash
   ajv compile -s schemas/config.schema.json
   ```

2. Tester avec les fichiers existants :
   ```bash
   ajv validate -s schemas/config.schema.json -d data/config.json
   ```

3. Vérifier dans VSCode (ouvrir un fichier → pas d'erreur rouge)

---

## Structure du projet

```
lib/MrJ-RailwayFX.local/
├── schemas/                    ← Schémas JSON (ici)
│   ├── config.schema.json
│   ├── device_types.schema.json
│   ├── board_types.schema.json
│   ├── bus_types.schema.json
│   ├── i2c_known.schema.json
│   └── README.md               ← Ce fichier
├── include/
├── src/
└── ...

data/                            ← Fichiers validés par les schémas
├── config.json
├── device_types.json
├── board_types.json
├── bus_types.json
└── i2c_known.json
```

---

## Dépendances

### **VSCode Extensions (recommandées)**
- **JSON Language Features** (built-in) — validation automatique
- **JSON Schema Validator** (optionnel) — validation avancée

### **Outils CLI (optionnels)**
```bash
npm install -g ajv-cli         # Validateur JSON Schema
npm install -g prettier        # Formatter JSON
```

---

## Références

- **JSON Schema Spec** : https://json-schema.org/
- **VSCode JSON** : https://code.visualstudio.com/docs/languages/json
- **AJV Validator** : https://ajv.js.org/

---

**Date de migration** : 2026-06-02  
**Version** : 1.0  
**Projet** : MrJ-LayoutFX  
**Licence** : AGPL-3.0-or-later — Copyright (c) 2026 HO44 PROJECT
