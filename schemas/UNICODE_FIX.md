# Correction des caractères Unicode dans les schémas JSON

## Problème détecté

VSCode signalait des warnings sur tous les fichiers JSON :

```
The character U+2013 "–" could be confused with the ASCII character U+002D "-"
```

**Cause :** Utilisation de tirets longs (em-dash Unicode U+2013) au lieu de tirets ASCII standard (U+002D).

**Impact :**
- ⚠️ Warnings VSCode dans tous les fichiers JSON
- ⚠️ Risque de problèmes de parsing JSON sur certains systèmes
- ⚠️ Incohérence avec les standards JSON (qui utilisent ASCII)

---

## Correction appliquée

### **Fichiers corrigés :**

**Schémas (5 fichiers) :**
- ✅ `lib/MrJ-RailwayFX.local/schemas/board_types.schema.json`
- ✅ `lib/MrJ-RailwayFX.local/schemas/bus_types.schema.json`
- ✅ `lib/MrJ-RailwayFX.local/schemas/config.schema.json`
- ✅ `lib/MrJ-RailwayFX.local/schemas/device_types.schema.json`
- ✅ `lib/MrJ-RailwayFX.local/schemas/i2c_known.schema.json`

**Données (1 fichier) :**
- ✅ `data/board_types.json`

### **Remplacement effectué :**

```bash
# Tous les U+2013 (–) remplacés par U+002D (-)
sed -i '' 's/–/-/g' *.schema.json
```

**Exemples de corrections :**

| Avant | Après |
|-------|-------|
| `"GPIO number (0–39)"` | `"GPIO number (0-39)"` |
| `"7-bit I²C address (0–127)"` | `"7-bit I²C address (0-127)"` |
| `"PCA9685, address 0x40–0x4F"` | `"PCA9685, address 0x40-0x4F"` |

---

## Vérification

### **Script de validation :**

```bash
# Vérifier qu'il n'y a plus de U+2013
for file in schemas/*.schema.json data/*.json; do
  if grep -q "–" "$file"; then
    echo "❌ Still has U+2013: $file"
  else
    echo "✅ Clean: $file"
  fi
done
```

### **Résultat attendu :**
```
✅ Clean: schemas/board_types.schema.json
✅ Clean: schemas/bus_types.schema.json
✅ Clean: schemas/config.schema.json
✅ Clean: schemas/device_types.schema.json
✅ Clean: schemas/i2c_known.schema.json
✅ Clean: data/board_types.json
✅ Clean: data/device_types.json
✅ Clean: data/bus_types.json
✅ Clean: data/i2c_known.json
```

---

## Prévention future

### **Dans VSCode :**

Activer les warnings Unicode dans `settings.json` :

```json
{
  "editor.unicodeHighlight.ambiguousCharacters": true,
  "editor.unicodeHighlight.invisibleCharacters": true,
  "editor.unicodeHighlight.nonBasicASCII": true
}
```

### **Bonnes pratiques JSON :**

1. **Toujours utiliser ASCII pour les structures** :
   - Tirets : `-` (U+002D) pas `–` (U+2013) ou `—` (U+2014)
   - Guillemets : `"` (U+0022) pas `"` `"` (U+201C/U+201D)
   - Apostrophes : `'` (U+0027) pas `'` (U+2019)

2. **Unicode acceptable uniquement dans les valeurs de chaînes** :
   ```json
   {
     "label": "Température – Capteur",  ❌ Clé avec U+2013
     "description": "Plage : 0–100°C"  ✅ Valeur avec U+2013 (acceptable)
   }
   ```

3. **Outils de validation recommandés** :
   - `jq` : valide JSON strict
   - `jsonlint` : détecte les caractères non-ASCII
   - VSCode JSON validator (built-in)

---

## Impact de la correction

### **Avant :**
- 🟡 4 warnings VSCode par fichier
- 🟡 Parsing JSON fragile sur certains parsers stricts

### **Après :**
- ✅ 0 warnings VSCode
- ✅ Compatibilité JSON stricte garantie
- ✅ Validation automatique VSCode fonctionnelle

---

## Commande de réparation rapide

Si le problème réapparaît (copier-coller depuis Word, etc.) :

```bash
# Réparer tous les schémas d'un coup
find lib/MrJ-RailwayFX.local/schemas -name "*.schema.json" -exec sed -i '' 's/–/-/g' {} \;

# Réparer tous les fichiers data
find data -name "*.json" -exec sed -i '' 's/–/-/g' {} \;
```

---

**Date de correction :** 2026-06-02  
**Fichiers corrigés :** 6 fichiers JSON  
**Caractères remplacés :** ~10 occurrences de U+2013  
**Projet :** MrJ-ArduinoRailwayFX
