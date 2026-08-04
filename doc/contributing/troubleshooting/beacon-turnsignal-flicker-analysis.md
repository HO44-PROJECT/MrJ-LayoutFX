# Analyse : TurnSignal scintille quand Beacon est actif

[Docs](../../README.md) / [Contributing](../README.md) / [Troubleshooting](README.md) / Beacon/TurnSignal flicker analysis

## 🔍 EN COURS — correctifs logiciels validés, résidu matériel non résolu (issue #139)

Sur la carte SPI `spi1` (HC595x2_uniface, 16 sorties), `signalflare01`
(TurnSignal, wiring 16) présentait un scintillement erratique. Ce document
retrace le diagnostic complet (logiciel et hardware) et les correctifs
appliqués (§1-10, résolus). Un problème résiduel, purement matériel et non
résolu, subsiste dans un cas précis — voir §11 et
[issue #139](https://github.com/HO44-PROJECT/MrJ-LayoutFX-backlog/issues/139).

---

## 1. Architecture d'exécution

### MrJFX::loop()

```cpp
static void loop() {
    ace_routine::CoroutineScheduler::loop();  // UNE coroutine traitée
#ifdef MRJFX_SPI_CARDS_ENABLED
    Spi595Bus::flush();                        // flush SPI après chaque coroutine
#endif
}
```

**Point critique** : `flush()` est appelé **N fois** par tour complet du scheduler (une fois par coroutine), pas une seule fois. Avec 16+ coroutines, le buffer SPI est envoyé 16+ fois par round.

### CoroutineScheduler::loop()

```cpp
void runCoroutine() {
    if (*mCurrent == nullptr) {
        mCurrent = T_COROUTINE::getRoot();
        if (*mCurrent == nullptr) return;
    }
    switch ((*mCurrent)->getStatus()) {
        case T_COROUTINE::kStatusYielding:
        case T_COROUTINE::kStatusDelaying:
            (*mCurrent)->runCoroutine();  // appelé même en délai !
            break;
        default:
            break;
    }
    mCurrent = (*mCurrent)->getNext();
}
```

**Découverte clé** : Le scheduler traite **UNE SEULE coroutine par appel** à `CoroutineScheduler::loop()`, puis avance le curseur. Il n'y a **pas de boucle interne** sur toutes les coroutines. Les coroutines en état `kStatusDelaying` **continuent d'être appelées** à chaque pass — la vérification d'expiration se fait à l'intérieur de la coroutine elle-même (via la macro `COROUTINE_DELAY`).

### Implémentation des coroutines AceRoutine

AceRoutine utilise des **computed gotos** (`__label__` + `&&jumpLabel`), **pas** un `switch(__LINE__)`. Les macros `COROUTINE_YIELD_INTERNAL()` sauvegardent l'adresse de retour et `COROUTINE_BEGIN()` jump dessus à la reprise.

---

## 2. Macros de délai AceRoutine — comparaison

| Macro | Mécanisme | Status | Coût par pass | Limite |
|---|---|---|---|---|
| `COROUTINE_DELAY(ms)` | `setDelayMillis()` + `isDelayExpired()` → arithmétique uint16_t | `kStatusDelaying` | ~3 cycles | 32 767 ms |
| `COROUTINE_DELAY_MICROS(µs)` | `setDelayMicros()` + `isDelayExpired()` → arithmétique uint16_t | `kStatusDelaying` | ~3 cycles | 32 767 µs |
| `COROUTINE_DELAY_MILLIS(t, ms)` | `COROUTINE_AWAIT(millis()-t >= ms)` → lecture hardware timer | `kStatusYielding` | ~20+ cycles | illimité |

```cpp
// COROUTINE_DELAY — efficace
#define COROUTINE_DELAY(delayMillis) \
  do { \
    this->setDelayMillis(delayMillis); \
    this->setDelaying(); \
    do { COROUTINE_YIELD_INTERNAL(); } while (!this->isDelayExpired()); \
    this->setRunning(); \
  } while (false)

// COROUTINE_DELAY_MILLIS — coûteux (lecture millis() à chaque pass)
#define COROUTINE_DELAY_MILLIS(timerStart, delay_millis) \
  do { \
    timerStart = millis(); \
    COROUTINE_AWAIT(millis() - timerStart >= delay_millis); \
  } while (0)
```

**Note** : `kStatusYielding` et `kStatusDelaying` sont traités **de la même façon** par le scheduler (les deux appellent `runCoroutine()`). La différence est uniquement dans le coût de la vérification interne.

---

## 3. PWM logiciel sur SPI — mécanique

### setPin() vs flush()

```cpp
// setPin() : mise à jour du buffer seulement — pas de transfert SPI
void setPin(uint8_t globalBit, bool value) {
    uint8_t byteIdx = _totalBytes - 1 - (globalBit / 8);
    uint8_t bitIdx  = globalBit % 8;
    if (value) _buf[byteIdx] |=  (1 << bitIdx);
    else       _buf[byteIdx] &= ~(1 << bitIdx);
}

// flush() : transfert SPI complet
void flush() {
    SPI.beginTransaction(...);
    digitalWrite(_latchPin, LOW);
    for (int i = _totalBytes - 1; i >= 0; i--)
        SPI.transfer(_buf[i]);
    digitalWrite(_latchPin, HIGH);
    SPI.endTransaction();
}
```

**Pour les pins SPI** : le changement d'état LED n'est effectif qu'au prochain `flush()`, pas au `setPin()`. La PWM logicielle via `COROUTINE_DELAY_MICROS` + `setPin()` fonctionne uniquement parce que `flush()` est appelé après chaque coroutine dans `MrJFX::loop()`.

### simulatePWM_raw (TurnSignal)

```
outputActive(pin)          → setPin(bit, HIGH)
COROUTINE_DELAY_MICROS(ON_TIME)
outputInactive(pin)        → setPin(bit, LOW)
COROUTINE_DELAY_MICROS(OFF_TIME)
```

Le LED est physiquement ON pendant `ON_TIME + flush_overhead` et OFF pendant `OFF_TIME + flush_overhead`. La régularité de la PWM dépend de la **stabilité du temps de round** du scheduler.

---

## 4. Mapping buffer SPI (HC595x2_uniface, 16 pins)

Pour `_totalBytes = 2` :

| Wiring (1-based) | globalBit (0-based) | byteIdx | bitIdx | Byte dans buffer |
|---|---|---|---|---|
| 1 | 0 | 1 | 0 | `_buf[1]` bit 0 |
| 8 | 7 | 1 | 7 | `_buf[1]` bit 7 |
| 9 | 8 | 0 | 0 | `_buf[0]` bit 0 ← **Beacon** |
| 16 | 15 | 0 | 7 | `_buf[0]` bit 7 ← **TurnSignal** |

Les pins 9 et 16 sont dans le **même octet** (`_buf[0]`) mais des bits différents — les opérations `|=`/`&=` sont indépendantes. Pas d'interférence logicielle entre eux dans le buffer.

Les pins 9 et 16 sont sur le **même 74HC595 physique** (second chip). En cas de problème matériel résiduel, voir section 8.

---

## 5. Analyse de la cause racine

### Timing du scheduler

- ~16 coroutines actives sur `spi1`
- Temps par coroutine (état `kStatusDelaying` avec `isDelayExpired()`) : ~4 µs
- **Temps de round nominal** : ~16 × 4 µs ≈ **64 µs**
- TurnSignal est appelé environ toutes les 64 µs

### Impact de COROUTINE_DELAY_MILLIS sur le round

Quand Beacon utilisait `COROUTINE_DELAY_MILLIS(timerStart, 80)` pour ses délais de 80 ms :
- Pendant 80 ms, **à chaque pass du scheduler**, Beacon appelait `millis()` (lecture du hardware timer, ~20+ cycles ESP32)
- Overhead ajouté par Beacon au round : ~20+ cycles × (1/240MHz) ≈ **+0,08 µs par pass**
- Le round passait de ~64 µs à ~64,08 µs

Cet overhead semble minime mais il est **non-uniforme** : il varie selon les autres activités ESP32 (WiFi sur Core 0, interruptions), introduisant du **jitter** dans le timing du scheduler.

### Impact sur TurnSignal à faible luminosité

À faible luminosité, `ON_TIME` peut être aussi court que **196 µs** — seulement **3 rounds** du scheduler. Un jitter de quelques µs sur le round représente une variation relative importante du rapport cyclique, causant le scintillement visible.

### Pourquoi seulement avec Beacon actif ?

Sans Beacon actif, `COROUTINE_DELAY_MILLIS` n'est pas en cours d'exécution — pas de lecture `millis()` répétée → round stable → TurnSignal régulier.

Avec Beacon actif, Beacon passe 80% de son temps en attente (délais 80ms, 100ms, 600ms) → lecture `millis()` à **chaque** pass pendant ces périodes → jitter → scintillement.

---

## 6. Correctif appliqué

### Beacon.cpp — avant

```cpp
// dans Beacon.h private section :
uint32_t timerStart;

// dans Beacon.cpp :
outputActive(_pin);
COROUTINE_DELAY_MILLIS(timerStart, BEACON_FLASH_ON_DURATION_1);
outputInactive(_pin);
COROUTINE_DELAY_MILLIS(timerStart, BEACON_FLASH_OFF_DURATION_1);
outputActive(_pin);
COROUTINE_DELAY_MILLIS(timerStart, BEACON_FLASH_ON_DURATION_2);
outputInactive(_pin);
COROUTINE_DELAY_MILLIS(timerStart, BEACON_FLASH_OFF_DURATION_2);
COROUTINE_DELAY_MILLIS(timerStart, BEACON_SHORT_PAUSE);
```

### Beacon.cpp — après

```cpp
// timerStart supprimé de Beacon.h

outputActive(_pin);
COROUTINE_DELAY(BEACON_FLASH_ON_DURATION_1);
outputInactive(_pin);
COROUTINE_DELAY(BEACON_FLASH_OFF_DURATION_1);
outputActive(_pin);
COROUTINE_DELAY(BEACON_FLASH_ON_DURATION_2);
outputInactive(_pin);
COROUTINE_DELAY(BEACON_FLASH_OFF_DURATION_2);
COROUTINE_DELAY(BEACON_SHORT_PAUSE);
```

Même correctif appliqué à **DoubleBeacon.cpp** (5 occurrences identiques) et `timerStart` supprimé de `DoubleBeacon.h`.

### Fichiers modifiés

| Fichier | Modification |
|---|---|
| `lib/MrJ-LayoutFX.local/src/led_fx/Beacon.cpp` | 5× `COROUTINE_DELAY_MILLIS` → `COROUTINE_DELAY` |
| `lib/MrJ-LayoutFX.local/src/led_fx/DoubleBeacon.cpp` | 5× `COROUTINE_DELAY_MILLIS` → `COROUTINE_DELAY` |
| `lib/MrJ-LayoutFX.local/include/led_fx/Beacon.h` | Suppression `uint32_t timerStart;` |
| `lib/MrJ-LayoutFX.local/include/led_fx/DoubleBeacon.h` | Suppression `uint32_t timerStart = 0;` |

### Build après correctif

```
pio run -e poc_ui --target clean
pio run -e poc_ui
→ SUCCESS  Flash: 30.1%  RAM: 8.0%
```

---

## 7. Limites de la PWM logicielle sur SPI

`COROUTINE_DELAY_MICROS` a une limite de **32 767 µs** (uint16_t interne). Pour les délais SPI, cela représente une limite à considérer si des effets lents sont implémentés via micros.

La précision de la PWM SPI est limitée par :
1. La granularité du scheduler (~64 µs par round avec 16 coroutines)
2. Le temps de transfert SPI par `flush()` (quelques µs)
3. Le jitter des autres coroutines (minimisé avec `COROUTINE_DELAY`)

**Paramètres TurnSignal à surveiller** : à très faible luminosité (ON_TIME < 200 µs), seuls 3 rounds se produisent entre HIGH et LOW — la précision est structurellement limitée indépendamment du correctif Beacon.

---

## 8. Fallback matériel (si correctif insuffisant)

Les pins 9 et 16 sont sur le **même 74HC595** (second chip de la chaîne spi1). Si l'activation de la LED beacon (forte luminosité, fort courant) provoque une chute de tension sur VCC ou un rebond de masse affectant le chip :

- **Symptôme** : scintillement résiduel même après correctif logiciel, corrélé à l'intensité du courant LED beacon
- **Diagnostic** : mesurer VCC du 74HC595 à l'oscilloscope pendant l'activation beacon
- **Correctif** : condensateur de découplage **100 nF** (céramique) entre VCC et GND du 74HC595, placé au plus près des broches du chip
- **Optionnel** : résistance série sur la ligne de données SPI (22-33 Ω) pour limiter les transitoires

---

## 9. Autres découvertes importantes

### Docstring incorrecte dans Spi595Bus

La documentation de `setPin()` indiquait incorrectement qu'il envoyait les données sur le bus SPI. En réalité, il met uniquement à jour le buffer interne. Le `flush()` est responsable du transfert.

### flush() appelé N fois par round

Avec 16 coroutines et un `flush()` après chaque coroutine dans `MrJFX::loop()`, le buffer est envoyé 16 fois par round complet. C'est intentionnel (permet la PWM SPI) mais entraîne une charge SPI importante. À surveiller si de nombreuses coroutines sont ajoutées.

### Cache PlatformIO

Les changements dans les fichiers inclus via `-include configurations/<env>/config.h` dans `build_flags` ne sont **pas automatiquement détectés** par PlatformIO. Après modification de `config.h`, toujours exécuter :

```bash
pio run -e <env> --target clean
pio run -e <env>
```

### COROUTINE_DELAY limite 32767 ms

`COROUTINE_DELAY(ms)` utilise un uint16_t en interne → maximum **32 767 ms** (~32 secondes). Pour des délais plus longs, utiliser `COROUTINE_DELAY_SECONDS(s)` (max 32767 s) ou enchaîner plusieurs délais.

---

## 10. Récapitulatif des bugs corrigés

| # | Fichier | Bug | Correctif |
|---|---|---|---|
| 1 | `Beacon.cpp` | `COROUTINE_DELAY_MILLIS` → overhead `millis()` à chaque pass | `COROUTINE_DELAY` |
| 2 | `DoubleBeacon.cpp` | Idem | `COROUTINE_DELAY` |
| 3 | `Beacon.h` / `DoubleBeacon.h` | `timerStart` inutile après correctif | Supprimé |
| 4 | `DoubleBeacon.cpp` / `.h` | `setState(RUN_FIRST/SECOND)` à chaque cycle → `oled.display()` bloquant | Remplacé par bool `_firstPinNext` |

---

## 11. Problème résiduel — wiring 8 spécifique (en cours)

### Symptôme

Après tous les correctifs logiciels, TurnSignal (wiring 16) scintille encore dans un cas précis : **tout effet activé sur wiring 8** (chip 1, Q7/QH).

### Tests de localisation effectués

| Test | Résultat |
|---|---|
| Beacon wiring 6 | OK |
| Beacon wiring 7 | OK |
| Beacon wiring 8 | **KO** |
| Beacon wiring 9 | OK |
| Beacon wiring 7 + 9 simultanés | OK |
| `oillamp01` GPIO 19 pendant wiring 8 actif | **stable** |
| QH' chip 1 (pin 9) relié à GND | aucun effet |
| Remplacement des 2 chips 74HC595 | aucun effet |
| Fréquence SPI 1 MHz au lieu de 10 MHz | aucun effet |

### Ce qui est éliminé

- **Logiciel** : tout le chemin code → buffer → flush est propre. GPIO 19 stable prouve que l'ESP32 et son alimentation ne sont pas affectés.
- **Chips** : deux chips neufs, même comportement.
- **Courant total** : wiring 7+9 simultanés (même courant que 8 seul + 9) → OK.
- **Fréquence SPI** : 1 MHz et 10 MHz → même résultat.
- **QH'** : court-circuité à GND → aucun effet.
- **MacBook Pro M4** : chassis aluminium causait du couplage capacitif parasite. Retiré. Le problème wiring 8 persiste sur surface neutre.

### Câblage 74HC595 (rappel)

```
ESP32 MOSI → Chip 1 DS (pin 14)   [wiring 1-8]
Chip 1 QH' (pin 9) → Chip 2 DS (pin 14)   [wiring 9-16]
Chip 2 QH' (pin 9) → non connecté
```

Wiring 8 = Chip 1 **Q7 (QH)** = même nœud interne que QH'. Tout signal parasite sur Q7 lors d'une transition se propage vers Chip 2 DS → peut corrompre le bit 7 de chip 2 (wiring 16 = TurnSignal).

### Hypothèse actuelle

Couplage physique sur le breadboard entre le fil de la LED wiring 8 (qui porte ~10-20 mA lors d'un flash) et les fils voisins (QH'→DS, ou directement le fil de wiring 16). La proximité sur le breadboard crée un couplage inductif ou capacitif.

### Piste à tester

Déplacer physiquement la LED de wiring 8 à l'autre extrémité du breadboard (même connexion électrique, mais fils plus éloignés des signaux SPI critiques). Si le problème disparaît → couplage entre fils physiques, solution : routage soigné des fils sur le PCB final.
