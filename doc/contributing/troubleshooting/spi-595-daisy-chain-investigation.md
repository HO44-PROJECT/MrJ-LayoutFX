# Investigation — 74HC595 SPI : le 2ᵉ registre (sorties 9-16) ne se pilote pas depuis l'UI

[Docs](../../README.md) / [Contributing](../README.md) / [Troubleshooting](README.md) / SPI 74HC595 daisy-chain investigation

> Journal de débogage complet (breadboard + carte MrJ). Écrit après une longue
> session ; conservé pour ne pas refaire les mêmes tests. **Ce n'est pas une doc
> structurelle** — c'est un compte-rendu d'enquête, avec ce qui a été testé,
> écarté, et conclu.

## ✅ RÉSOLU — panne matérielle du breadboard, firmware hors de cause

La ré-exécution du POC **à la fin de l'enquête** a tranché : `poc_k2000_SPI`,
reflashé sur le même breadboard, montre **lui aussi** des LEDs 9-16 **faiblardes et
aléatoires** — et après un reset électrique, **tout est parfait ~1 seconde puis les
9+ se dégradent**. Signature purement **électrique** (contact/alim qui lâche sous
charge), impossible pour un bug logiciel.

**Le fait fondateur de l'enquête — « le POC allume les 16 LEDs sur ce breadboard » —
était PÉRIMÉ** : vrai à l'époque, faux au moment de l'enquête (câblage dégradé entre
temps). Toute la contradiction « POC OK / firmware KO » reposait sur cette baseline
jamais re-vérifiée. **Leçon n°1 : quand les faits se contredisent, re-vérifier la
baseline AVANT de suspecter le code.**

Suspects matériels (chip 2, dans l'ordre) : contact **VCC/GND** sous charge,
**OE (pin 13)** mal relié à GND (sorties faiblardes = OE flottant classique),
**RCLK (pin 12)**, cascade **Q7'→SER**, absence de **100 nF** de découplage,
**résistances série des LEDs** (surcharge des drivers → échauffement en quelques
secondes).

### Chronologie de la résolution (observations finales)

1. **Re-flush périodique forcé à 200 Hz** dans le firmware (patch TEMP) : 9-16
   **toujours 100 % mortes**. Déduction clé : à 200 Hz d'un buffer *statique*, UN
   seul latch réussi aurait suffi à les allumer durablement → le taux de réussite
   des transferts vers le registre 2 était **zéro**. Échec **déterministe**, pas
   marginal — incompatible avec « le POC marche » *si le câblage est le même*.
   → C'est ce qui a imposé de re-vérifier la baseline.
2. **Re-flash du POC `poc_k2000_SPI`, le jour même, sur le même breadboard** :
   les LEDs **9+ sont faiblardes et aléatoires**. La baseline était périmée.
3. **Reset électrique** : tout est **parfait ~1 seconde** (les 16), puis les 9+
   se dégradent.
4. **Sous le POC** : **3 chenilles parfaites sur les 9+, la 4ᵉ déconne** —
   dégradation cumulative sous charge.

(2)+(3)+(4) = signature **électrique/thermique** (contact ou surcharge qui lâche
quand le courant s'établit/chauffe). Aucun logiciel ne produit une panne qui
« marche à froid puis se dégrade en quelques secondes ».

## TL;DR (à lire en premier)

- **Symptôme** : sur une carte fille à **2× 74HC595 (16 sorties)** en daisy-chain SPI,
  seules les **8 premières sorties (registre 1, proche du MOSI)** se pilotent depuis
  le firmware principal / l'UI. Les **8 suivantes (registre 2, cascadé via Q7'→SER)**
  restent éteintes, ou s'allument **aléatoirement** quand beaucoup d'effets tournent.
- **Fait n°1 (périmé !)** : le POC **`poc_k2000_SPI`** allumait **les 16 LEDs** sur
  ce breadboard. → On en a déduit « hardware bon » pendant toute l'enquête.
  **La ré-vérification finale a montré que ce n'était plus vrai** (voir ✅ ci-dessus).
- **Fait n°2** : le **même firmware principal** pilote **les 16 sorties** sur
  le **second ESP32 (carte MrJ)**. → Ce **n'est pas un bug firmware**.
- **Conclusion finale** : panne matérielle du breadboard (chip 2), révélée par le
  one-shot du firmware et masquée par le rafraîchissement continu du POC tant que la
  dégradation était partielle (voir la section ★ cadence).
- **Reco** : utiliser la vraie carte (qui marche). Fiabiliser le breadboard →
  **hardware** (voir plus bas), pas plus de firmware.

## Le montage

| | |
|---|---|
| MCU | ESP32 DevKitC (VSPI) |
| Bus | `spi_master_only` — MOSI **23**, SCLK **18**, LATCH **5**, MISO override **19** |
| Carte fille | `HC595x2_biface` / `HC595x2_uniface` = **2× 595 = 16 sorties**, `pin_count` structurel = 16 |
| Daisy-chain | 595#1 `Q7'` (pin 9) → 595#2 `SER/DS` (pin 14). SCLK + LATCH partagés. |
| Deux exemplaires | **(1) breadboard** (fils volants) — échoue ; **(2) carte MrJ** (pistes propres) — marche |

Le firmware modélise **une carte de 16 bits = 2 octets** (`totalBytes = 2`).
Mapping (`Spi595Bus::setPin`) : ordre d'octet inversé →
- **wiring 1-8** → `buf[1]` (LSByte) → registre **1** (proche MOSI, dernier octet transmis)
- **wiring 9-16** → `buf[0]` (MSByte) → registre **2** (lointain, **premier** octet transmis)

## Ce que le firmware fait de correct (vérifié)

- `pins=16` au boot (log `DeviceFactory: board[...] pins=16`) et
  `Spi595Bus: init ok — totalBytes=2` → dimensionnement **bon**.
- Le debug ajouté dans `flush` a montré `buf[0]=0x01` quand l'effet sur wiring 9 est ON
  → **le bon bit du bon octet est mis** (mapping correct).
- Puisque le registre **1** marche, les **16 bits sont bien décalés** (le 2ᵉ octet doit
  traverser le 1ᵉ registre pour l'atteindre) → donc `buf[0]` **est** poussé dans le
  registre 2. Le bit y arrive, mais la sortie ne s'allume pas de façon fiable.

## Tout ce qui a été testé (et le résultat)

| Test | Résultat | Conclusion |
|---|---|---|
| `pin_count` 8 vs 16 (config) | à 8 → chaîne à 1 octet (attendu) ; à 16 → `totalBytes=2` | pas la cause finale (le boot montre bien 16/2) |
| Méthode `flush` : boucle `transfer(octet)` | 9-16 morts | — |
| Méthode `flush` : `writeBytes` (burst continu) | 9-16 morts | ce n'est pas « boucle vs continu » |
| Méthode `flush` : `transfer16` (= **exactement** le POC) | 9-16 morts | le flush n'est **pas** la différence |
| Horloge 10 MHz → **1 MHz** | aucun changement | **pas** l'intégrité de signal côté horloge (plus lent aurait aidé si c'était ça) |
| **DCC coupé** (bus retiré) | aucun changement | pas les interruptions DCC |
| **OLED débranché** | aucun changement (rapporté) | pas l'OLED seul (à reconfirmer proprement) |
| **Self-test au boot** = code EXACT du POC (`transfer16(0x0100)`) injecté dans le firmware | registre 2 muet | même appel que le POC, **résultat différent** → différence dans l'**environnement**, pas dans l'appel SPI |
| Beaucoup d'effets sur 9-16 | s'allument **aléatoirement** | le registre 2 « tient » seulement s'il est **rafraîchi très souvent** |
| POC `poc_k2000_SPI` sur le même breadboard (souvenir, **jamais re-vérifié pendant l'enquête**) | « 16 LEDs OK » | ⚠️ baseline **périmée** — a orienté toute l'enquête vers le logiciel |
| Firmware principal sur ESP32 #2 (carte MrJ) | **16 sorties OK** | firmware bon |
| **Re-flush périodique forcé 200 Hz** (patch TEMP, buffer statique) | 9-16 **toujours mortes** | taux de réussite des transferts vers reg2 = **zéro** → échec **déterministe** → la baseline POC devait être fausse |
| **POC re-flashé LE JOUR MÊME** sur le breadboard | 9+ **faiblardes et aléatoires** | la baseline était périmée → **panne matérielle confirmée** |
| Reset électrique | parfait ~1 s (les 16), puis 9+ se dégradent | signature électrique (contact/alim sous charge) |
| Chenilles POC observées dans la durée | 3 parfaites sur les 9+, la **4ᵉ déconne** | dégradation cumulative sous charge → thermique/contact |

## Ce qui a été formellement écarté

- **Bug de firmware** (le même firmware marche sur la carte #2 ; les octets envoyés
  sont prouvés corrects ; le POC échoue aussi une fois re-vérifié).
- **Mapping octet/bit** (`buf[0]`/`buf[1]` prouvés corrects par le debug).
- **Méthode de transfert SPI** (boucle / writeBytes / transfer16 : toutes échouent).
- **Vitesse d'horloge** (1 MHz n'aide pas ; « plus lent » échoue là où « plus rapide »
  marchait pour le POC → incohérent avec un problème de vitesse).
- **DCC** (coupé, sans effet), **OLED** (débranché, sans effet).
- ~~Panne hardware~~ — écartée **à tort** pendant toute l'enquête, sur la foi de la
  baseline POC périmée. C'est finalement **la cause réelle**.

## ★ L'explication trouvée après coup : l'asymétrie de CADENCE

Relecture complète du POC après la session : **le POC et le firmware ne font pas le
même métier**, et c'est ce qui rend le « POC OK / firmware KO » compatible.

- **Le POC est un moteur POV** : `K2000Coroutine` envoie 3 trames par 7 ms par
  instance, ×2 instances → **~850 retransmissions complètes de la chaîne par
  seconde, en continu**. Il re-décale ET re-latche tout en permanence ; il ne
  demande **jamais** au 595 de *retenir* un état latché.
- **Le firmware est événementiel (one-shot)** : `_dirty` → 1 flush par changement
  d'état, puis plus rien pendant des centaines de ms ; le 595 doit **tenir**.

Conséquence : « le POC allume les 16 LEDs » ne prouve **pas** qu'un transfert
*unique* atteint et latche fiablement le registre 2. Avec un contact marginal
(fil **RCLK du chip 2** ou cascade **Q7'→SER**, faux contact breadboard), le POC
masque tout — même un RCLK flottant afficherait l'animation (latchs parasites sur
un flux répété 850×/s). Le firmware en one-shot, lui, échoue en permanence.

Cette lecture explique **tous** les faits du tableau ci-dessus, y compris les deux
que toutes les théories logicielles n'expliquaient pas : le self-test `transfer16`
muet (one-shots) et le « 9-16 s'allument aléatoirement quand beaucoup d'effets
tournent » (plus de flushs = on se rapproche de la cadence POC → captures
aléatoires).

**Test décisif (logiciel)** : forcer un re-flush périodique (~5 ms) dans
`Spi595Bus::flush` → si les 9-16 reprennent vie (même en scintillant), la chaîne
n'est pas fiable en one-shot → contact matériel prouvé sans oscillo.
**Suspect n°1** : le fil RCLK (latch, pin 12) du chip 2 — un RCLK flottant donne
exactement « jamais de mise à jour en one-shot + captures aléatoires sous trafic ».

## Hypothèse « interférences » (historique — DÉPASSÉE par la résolution)

> Conservée pour mémoire : c'était l'hypothèse de travail **avant** la re-vérification
> du POC. Elle attribuait la panne à l'environnement du firmware (WiFi, I²C voisin du
> MOSI, conso) sur un câblage marginal. La ré-exécution du POC a montré que le
> câblage était **dégradé tout court** — pas besoin du firmware pour échouer. Seule
> la partie « la carte MrJ tolère / le breadboard non » reste vraie.

## Checklist matérielle (réparation du breadboard — chip 2 en priorité)

Signature observée : « parfait à froid ~1 s / 3 chenilles, puis 9+ faiblardes » →
contact ou surcharge qui lâche quand le courant s'établit/chauffe. Dans l'ordre :

1. **Enfoncer fermement les 2 puces** dans le breadboard (puce à moitié insérée =
   exactement ce symptôme).
2. **Refaire les fils VCC (pin 16) / GND (pin 8) du chip 2** — courts, directs sur
   les rails. Suspect n°1 d'une panne qui apparaît sous charge.
3. **OE (pin 13) → GND bien franc** — un OE flottant = sorties *faiblardes*
   (tri-state intermittent), le symptôme observé mot pour mot.
4. **SRCLR/MR (pin 10) → VCC** ferme.
5. Fil **Q7' (pin 9, chip 1) → SER (pin 14, chip 2)** : court, contact franc.
6. **RCLK (pin 12)** et **SRCLK (pin 11)** du chip 2 : re-vérifier les deux
   jumpers partagés.
7. **100 nF céramique entre VCC et GND de CHAQUE 595**, au plus près des pattes.
8. **Une résistance série par LED (~220-470 Ω)** sur les 9-16 — des LEDs sans
   résistance surchargent les drivers du 595 (échauffement en quelques secondes →
   « 3 chenilles puis déconne »).
9. Vérifier que les **rails d'alim du breadboard ne sont pas coupés au milieu**
   (classique : rails segmentés, la moitié lointaine mal alimentée).
10. Accessoirement : résistance série ~100 Ω sur SCLK/MOSI (réflexions), éloigner
    MOSI (23) des lignes I²C (21/22).

Validation après réparation : reflasher le POC → les 16 doivent rester **parfaites
dans la durée** (pas seulement 1 s) ; puis le firmware principal → effets sur
wiring 9-16 pilotables depuis l'UI **sans aucun changement de code**.

---

## Bugs firmware **réels** découverts en chemin (indépendants du non-pilotage 595)

Ces points sont de vrais défauts, tous **corrigés et fermés** depuis : **#56**
(resize hot-reload), **#54** (pin_count éditeur), **#52** (indicateur actif),
**#53** (upload). La réparation matérielle du breadboard est suivie par **#57**,
également fermée. Conservé ci-dessous pour l'historique de l'enquête.

1. **La chaîne SPI n'est pas redimensionnée au hot-reload.** (→ #56)
   `BusRegistry::activateSpi()` est idempotent (`if (_spiReady) return;`) et
   `BusRegistry::reset()` **préserve** `_spiReady`. Donc changer `pin_count` via l'UI
   (hot-reload) ne rappelle **jamais** `Spi595Bus::init()` → `totalBytes` reste figé à
   sa valeur du **premier boot**. Il faut un **reboot complet**. → *piège majeur pendant
   l'enquête* (« j'ai mis 16, rien ne change »).

2. **Éditeur de board : champ `pin_count` périmé pour les boards SPI.**
   `beUpdateFields` (app-board-editor.js) ne gère pas le cas « board SPI **sans**
   `pin_count` » → le champ garde la **valeur précédente** (un 8 résiduel), et Save écrit
   alors `pin_count: 8` dans la config → chaîne tronquée à 8 sorties.

3. **La config affichée « active » ne reflète pas la config qui tourne.**
   Le point vert « actif » = dernier **fichier activé** (`config_source.txt`). Mais les
   éditions WebUI écrivent **directement dans `config.json`**, qui **diverge** alors du
   fichier source. D'où : `config__19_.json` marqué actif (sans SPI) alors que la carte
   SPI est à l'écran (= `config.json`). **`config.json` = la vérité.**

4. **L'upload de config « ne s'applique pas ».**
   Rapporté : uploader un JSON n'a pas d'effet visible. À investiguer (écrit-il un
   nouveau fichier sans l'activer ? ne reboote-t-il pas ?).

## Références code

- `lib/.../src/spi/Spi595Bus.cpp` — `init` (calcul `totalBytes`), `setPin` (mapping
  wiring→byteIdx, ordre inversé), `flush` (boucle `transfer`, gating `_dirty`).
- `lib/.../src/bus/BusRegistry.cpp` — `activateSpi` (idempotent), `reset`
  (`_spiReady` préservé), `regSpiCard`, `_spiCardPinCounts` (la **vraie** table d'init).
- `configurations/poc_k2000_SPI/Spi595.cpp` — `send595` (référence qui **marche** :
  `transfer16`), `spi595Init`.
- `lib/.../src/web/app-board-editor.js` — `beUpdateFields` (bug champ `pin_count`),
  `saveBoardEditor` (écrit `entry.pin_count`).
