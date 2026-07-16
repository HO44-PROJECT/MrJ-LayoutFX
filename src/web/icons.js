/**
 * @file icons.js
 * @brief SVG icon definitions for railway devices (signals, lamps, beacons, etc.).
 *
 * Contains inline SVG paths for all device types displayed in the WebUI.
 * Used for device cards and type selection.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

var S = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-linecap="round" stroke-linejoin="round" stroke-width="1.5">';
var E = '</svg>';
// Mini LED SVG for the raw-GPIO test sub-button
var LED_ICO = S + '<circle cx="12" cy="11" r="4"/>'
  + '<path d="M12 1v2M12 18v2M3 11H1M23 11h-2M5.6 4.6l1.4 1.4M15 15.4l1.4 1.4M5.6 17.4l1.4-1.4M15 6.6l1.4-1.4"/>'
  + '<line x1="9" y1="21" x2="15" y2="21"/><line x1="12" y1="15" x2="12" y2="21"/>'
  + E;
var ICONS = {
  'Beacon': S
    + '<circle cx="12" cy="10" r="3.5"/>'
    + '<path d="M12 3v2M12 15v2M5 10h2M17 10h2M8.5 6.5l-1.5-1.5M15.5 6.5l1.5-1.5M8.5 13.5l-1.5 1.5M15.5 13.5l1.5 1.5"/>'
    + '<line x1="8" y1="21" x2="16" y2="21"/>'
    + E,

  'DoubleBeacon': S
    + '<circle cx="7" cy="9" r="2.5"/>'
    + '<path d="M7 2v2M7 14v2M1 9H3M11 9h2M3.5 5.5l1.5 1.5M8.5 12.5l1.5 1.5M3.5 12.5l1.5-1.5M8.5 5.5l1.5-1.5"/>'
    + '<circle cx="17" cy="9" r="2.5"/>'
    + '<path d="M17 2v2M17 14v2M13 9h2M21 9h2M13.5 5.5l1.5 1.5M18.5 12.5l1.5 1.5M13.5 12.5l1.5-1.5M18.5 5.5l1.5-1.5"/>'
    + '<line x1="5" y1="20" x2="19" y2="20"/>'
    + E,

  'GasLamp': S
    // mat central
    + '<line x1="12" y1="22" x2="12" y2="12" stroke="currentColor" stroke-width="2" stroke-linecap="round"/>'
    // bras gauche : arc montant et arrondi
    + '<path d="M12,12 C10,11 8,9 7,7.5" stroke="currentColor" stroke-width="1.5" fill="none" stroke-linecap="round"/>'
    // bras central (optionnel, vertical)
    + '<path d="M12,12 C12,11 12,9 12,7.5" stroke="currentColor" stroke-width="1.5" fill="none" stroke-linecap="round"/>'
    // bras droit : arc montant et arrondi
    + '<path d="M12,12 C14,11 16,9 17,7.5" stroke="currentColor" stroke-width="1.5" fill="none" stroke-linecap="round"/>'
    // lampes
    + '<circle cx="7" cy="7.5" r="1.5" fill="currentColor"/>'
    + '<circle cx="12" cy="7.5" r="1.5" fill="currentColor"/>'
    + '<circle cx="17" cy="7.5" r="1.5" fill="currentColor"/>'
    // socle du lampadaire
    + '<rect x="10" y="22" width="4" height="1.5" fill="currentColor"/>'
    + '<path d="M10,23.5 L14,23.5 L13,25 L11,25 Z" fill="currentColor"/>'
    + E,

  'GasLampDefect': S
    // mat central
    + '<line x1="12" y1="22" x2="12" y2="12" stroke="currentColor" stroke-width="2" stroke-linecap="round"/>'
    // bras gauche : arc montant et arrondi
    + '<path d="M12,12 C10,11 8,9 7,7.5" stroke="currentColor" stroke-width="1.5" fill="none" stroke-linecap="round"/>'
    // bras central : arc dessiné mais SANS lampe (ampoule manquante = signe du défaut)
    + '<path d="M12,12 C12,11 12,9 12,7.5" stroke="currentColor" stroke-width="1.5" fill="none" stroke-linecap="round" stroke-dasharray="1.5 1.5"/>'
    // bras droit : arc montant et arrondi
    + '<path d="M12,12 C14,11 16,9 17,7.5" stroke="currentColor" stroke-width="1.5" fill="none" stroke-linecap="round"/>'
    // lampes (seulement gauche et droite — la centrale manque)
    + '<circle cx="7" cy="7.5" r="1.5" fill="currentColor"/>'
    + '<circle cx="17" cy="7.5" r="1.5" fill="currentColor"/>'
    // socle du lampadaire
    + '<rect x="10" y="22" width="4" height="1.5" fill="currentColor"/>'
    + '<path d="M10,23.5 L14,23.5 L13,25 L11,25 Z" fill="currentColor"/>'
    + E,

  'ElectricLamp': S
    + '<path d="M10 21h4M11 21v-2M13 21v-2"/>'
    + '<path d="M12 3a6 6 0 0 1 4 10.5V17a1 1 0 0 1-1 1H9a1 1 0 0 1-1-1v-3.5A6 6 0 0 1 12 3Z"/>'
    + '<line x1="10" y1="13" x2="10" y2="15"/>'
    + E,

  'CampFire': S
    + '<path d="M12 2C11 5 9 8 11 11C11.5 12.5 12 13 12 13C12 13 12.5 12.5 13 11C15 8 13 5 12 2Z"/>'
    + '<path d="M9.5 13C8.5 15 9.5 17.5 12 18C14.5 17.5 15.5 15 14.5 13"/>'
    + '<line x1="5" y1="22" x2="19" y2="22"/>'
    + '<line x1="6" y1="18" x2="9.5" y2="22"/>'
    + '<line x1="18" y1="18" x2="14.5" y2="22"/>'
    + E,

  'Torch': S
    + '<line x1="6" y1="22" x2="14" y2="13"/>'
    + '<line x1="12" y1="14" x2="16" y2="12"/>'
    + '<path d="M13 13 Q10 9 13 5 Q15 8 16 6 Q18 10 16 13Z"/>'
    + E,

  'TurnSignal': S
    + '<path d="M4 12L13 3V8H19V16H13V21Z"/>'
    + E,

  'Storm': S
    + '<path d="M20 9a8 8 0 0 0-15.3-2.2A4.5 4.5 0 0 0 5 16h13a3 3 0 0 0 2-5.2"/>'
    + '<polyline points="12,11 10,16 14,16 12,21"/>'
    + E,

  'SolderLamp': S
    + '<path d="M4,21 L4,8 Q4,3 12,3 Q20,3 20,8 L20,21 Z"/>'
    + '<rect x="6" y="10" width="12" height="5" rx="1"/>'
    + E,

  'DefectLamp': S
    + '<path d="M10 21h4M11 21v-2M13 21v-2"/>'
    + '<path d="M12 3a6 6 0 0 1 4 10.5V17a1 1 0 0 1-1 1H9a1 1 0 0 1-1-1v-3.5A6 6 0 0 1 12 3Z"/>'
    + '<line x1="9.5" y1="7.5" x2="14.5" y2="14.5"/>'
    + '<line x1="14.5" y1="7.5" x2="9.5" y2="14.5"/>'
    + E,

  'NeonSign': S
    + '<path d="M2 16V8l4 5 4-5v8"/>'
    + '<path d="M12 16v-6M12 13q2.5-2 5 0"/>'
    + '<path d="M19 8h3M21 8v6q0 2-2.5 2H18"/>'
    + E,

  'OilLamp': S
    // Pied
    + '<rect x="10" y="17" width="4" height="2" rx="1"/>'

    // Réservoir (plus stylé qu’un ellipse)
    + '<path d="M4,14 Q12,9 20,14 Q18,17 12,17 Q6,17 4,14 Z"/>'

    // Bec
    + '<path d="M4,14 Q5,10 4,7 Q2.5,10 4,14"/>'

    // Flamme
    + '<path d="M4,9 Q5,7 4,6 Q3,7 4,9 Z"/>'

    // Poignée / bouton
    + '<circle cx="21" cy="13" r="1.5"/>'

    + E,

  'SignalFlare': S
    + '<path d="M9 9L10 5L11 8L12 2L13 7L14 4L15 9Z" fill="currentColor" stroke="none"/>'
    + '<rect x="10" y="9" width="4" height="12" rx="1"/>'
    + E,

  'TrainHeadLamp': S
    + '<rect x="10" y="1" width="4" height="5" rx="1"/>'
    + '<circle cx="12" cy="13" r="6"/>'
    + '<circle cx="12" cy="10.5" r="2" fill="currentColor" stroke="none"/>'
    + '<line x1="2" y1="18" x2="22" y2="18"/>'
    + '<circle cx="5.5" cy="21" r="2.5"/>'
    + '<circle cx="18.5" cy="21" r="2.5"/>'
    + E,

  'RailwayCrossingLights': S
    + '<line x1="12" y1="2" x2="12" y2="22"/>'
    + '<line x1="3" y1="7" x2="21" y2="17"/>'
    + '<line x1="21" y1="7" x2="3" y2="17"/>'
    + '<circle cx="3" cy="7" r="2.5" fill="currentColor" stroke="none"/>'
    + '<circle cx="21" cy="17" r="2.5" fill="currentColor" stroke="none"/>'
    + E,

  'StaticLow': S
    + '<line x1="4" y1="10" x2="20" y2="10"/>'
    + '<line x1="7" y1="14" x2="17" y2="14"/>'
    + '<line x1="10" y1="18" x2="14" y2="18"/>'
    + E,

  'MrJDBBlocSignal': S
    // Mât
    + '<line x1="12" y1="22" x2="12" y2="21"/>'
    // Boîtier commun aux 3 signaux : x=8 w=8 h=20 rx=2
    + '<rect x="8" y="1" width="8" height="20" rx="2"/>'
    // 2 LEDs côte à côte dans la moitié basse
    + '<circle cx="10" cy="15" r="1.5" fill="currentColor" stroke="none"/>'
    + '<circle cx="14" cy="15" r="1.5" fill="currentColor" stroke="none"/>'
    + E,

  'MrJDBEntrySignal': S
    // Mât
    + '<line x1="12" y1="22" x2="12" y2="21"/>'
    // Boîtier commun aux 3 signaux : x=8 w=8 h=20 rx=2
    + '<rect x="8" y="1" width="8" height="20" rx="2"/>'
    // 1 LED en haut à droite
    + '<circle cx="14" cy="6" r="1.5" fill="currentColor" stroke="none"/>'
    // 2 LEDs en bas côte à côte
    + '<circle cx="10" cy="15" r="1.5" fill="currentColor" stroke="none"/>'
    + '<circle cx="14" cy="15" r="1.5" fill="currentColor" stroke="none"/>'
    + E,

  'MrJDBExitSignal': S
    // Mât
    + '<line x1="12" y1="22" x2="12" y2="21"/>'
    // Boîtier commun aux 3 signaux : x=8 w=8 h=20 rx=2
    + '<rect x="8" y="1" width="8" height="20" rx="2"/>'
    // Rangée 1 : 1 LED gauche
    + '<circle cx="10" cy="4.5" r="1.5" fill="currentColor" stroke="none"/>'
    // Rangée 2 : 2 LEDs gauche + droite
    + '<circle cx="10" cy="8" r="1.5" fill="currentColor" stroke="none"/>'
    + '<circle cx="14" cy="8" r="1.5" fill="currentColor" stroke="none"/>'
    // Rangée 3 : 1 petite LED droite
    + '<circle cx="14" cy="11.5" r="1.1" fill="currentColor" stroke="none"/>'
    // Rangée 4 : 1 petite LED gauche
    + '<circle cx="10" cy="15" r="1.1" fill="currentColor" stroke="none"/>'
    // Rangée 5 : 1 LED gauche
    + '<circle cx="10" cy="18.5" r="1.5" fill="currentColor" stroke="none"/>'
    + E,

  'TrafficLight3ph': S
    + '<rect x="7" y="2" width="10" height="17" rx="2"/>'
    + '<circle cx="12" cy="6" r="2"/>'
    + '<circle cx="12" cy="11" r="2"/>'
    + '<circle cx="12" cy="16" r="2"/>'
    + '<line x1="12" y1="19" x2="12" y2="22"/>'
    + E,

  'TrafficLight4ph': S
    + '<rect x="7" y="2" width="10" height="17" rx="2"/>'
    + '<circle cx="12" cy="6" r="2"/>'
    + '<circle cx="12" cy="11" r="2"/>'
    + '<circle cx="12" cy="16" r="2"/>'
    + '<line x1="12" y1="19" x2="12" y2="22"/>'
    + E,

  'DfAudio': S
    + '<polygon points="11,5 6,9 2,9 2,15 6,15 11,19"/>'
    + '<path d="M15.5 8.5a5 5 0 0 1 0 7"/>'
    + '<path d="M19 5a10 10 0 0 1 0 14"/>'
    + E,

  'SerialServo': S
    // corps du servo : rectangle arrondi
    + '<rect x="5" y="7" width="14" height="10" rx="2" ry="2" stroke="currentColor" stroke-width="2" fill="none"/>'
    // axe central
    + '<circle cx="15" cy="12" r="2" fill="currentColor"/>'
    // bras du servo ressorti et légèrement décalé pour donner du relief
    + '<line x1="15" y1="12" x2="24" y2="6" stroke="currentColor" stroke-width="2" stroke-linecap="round"/>'
    + E,

  'PCA9685Servo': S
    // corps du servo positionnel : rectangle arrondi
    + '<rect x="3" y="8" width="14" height="8" rx="2" fill="none"/>'
    // axe
    + '<circle cx="14" cy="12" r="2" fill="currentColor"/>'
    // bras en position intermédiaire (~45°)
    + '<line x1="14" y1="12" x2="19" y2="7"/>'
    // arc de course (butée basse → butée haute)
    + '<path d="M21 12 A7 7 0 0 0 14 5"/>'
    + E,

  'PCA9685Motor': S
    + '<rect x="3" y="8" width="14" height="8" rx="2" fill="none"/>'
    + '<circle cx="14" cy="12" r="2" fill="currentColor"/>'
    + '<circle cx="14" cy="12" r="7"/>'
    + '<polyline points="12 3 14 5 12 7"/>'
    + E,

  'Led': S
    + '<circle cx="12" cy="11" r="4"/>'
    + '<path d="M12 1v2M12 18v2M3 11H1M23 11h-2M5.6 4.6l1.4 1.4M15 15.4l1.4 1.4M5.6 17.4l1.4-1.4M15 6.6l1.4-1.4"/>'
    + '<line x1="9" y1="21" x2="15" y2="21"/>'
    + '<line x1="12" y1="15" x2="12" y2="21"/>'
    + E,

  '_': S
    + '<circle cx="12" cy="12" r="9"/>'
    + '<path d="M9.5 9.5a3 3 0 1 1 3 3V14"/>'
    + '<circle cx="12" cy="17" r=".7" fill="currentColor" stroke="none"/>'
    + E
};
