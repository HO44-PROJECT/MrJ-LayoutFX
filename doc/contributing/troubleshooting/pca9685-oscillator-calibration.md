# Calibration de l'oscillateur PCA9685

[Docs](../../README.md) / [Contributing](../README.md) / [Troubleshooting](README.md) / PCA9685 oscillator calibration

## Symptôme

Tous les servos/moteurs continus d'une même carte PCA9685 ont un point neutre différent
de 1500 µs (par exemple 1700 µs). Le problème touche **tous les canaux** du chip.

## Cause

Le PCA9685 embarque un oscillateur interne nominalement à **25 MHz**, mais sa tolérance
réelle peut atteindre ±10 %. Si la bibliothèque Adafruit calcule les durées µs en supposant
25 MHz alors que le chip tourne à ~22 MHz, toutes les impulsions sont allongées
proportionnellement.

Exemple : oscillateur à 22 059 000 Hz → ce que le code croit être 1500 µs sort réellement
à **1700 µs** sur la broche.

## Mesure

1. Flasher le firmware avec `neutral_us` à 1500 (valeur par défaut).
2. Connecter un servo continu ou utiliser un oscilloscope sur un canal.
3. Observer la largeur d'impulsion au repos (neutre réel).
4. Calculer la fréquence d'oscillateur réelle :

```
oscillator_hz = 25 000 000 × (1500 / neutre_mesuré)
```

Exemple : neutre mesuré = 1700 µs → `25 000 000 × (1500 / 1700) ≈ 22 059 000 Hz`

## Configuration

Dans l'éditeur de board (WebUI → Configuration → Cartes & extensions → modifier la carte
PCA9685), renseigner le champ **Oscillateur PCA9685 (Hz)** avec la valeur calculée.

Ou directement dans `config.json` :

```json
{
  "boards": [
    {
      "id": "pca1",
      "type": "PCA9685",
      "bus": "i2c0",
      "oscillator_hz": 22059000
    }
  ]
}
```

La valeur est omise du JSON si elle vaut 25 000 000 (valeur par défaut).

## Effet

Le firmware appelle `setOscillatorFrequency(oscillator_hz)` avant `setPWMFreq(50)` lors
de l'initialisation du driver. Tous les canaux du board sont corrigés automatiquement.
`neutral_us` peut rester à 1500 sur chaque device.

## Notes

- La correction s'applique à la fois aux `PCA9685Servo` et aux `PCA9685Motor`.
- Si deux cartes PCA9685 ont des oscillateurs différents, chaque board a sa propre valeur.
- Une mesure à l'oscilloscope est plus précise que la méthode par servo continu.
