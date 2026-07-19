# ✅ résolu — le journal DCC diagnostics n'affichait aucune trace lors de bascules rapides

**Symptôme**: sur l'onglet Diagnostics WebUI (#76), des bascules ON/OFF rapides
d'un signal ou d'un accessoire (dans la fenêtre d'allumage ~1500ms de la pastille)
ne produisaient **aucune ligne** dans le journal détaillé, alors que la pastille
elle-même s'allumait et comptait correctement, et que des bascules espacées dans
le temps produisaient bien une ligne. Confirmé reproductible sur un signal JMRI
(adresse 31, aspects Hp0=0/Hp1=1 sur le même bouton de commande).

## Ce qui a été écarté

- **Pas un bug NmraDcc** : lecture du code vendored (`NmraDcc.cpp`) confirme que
  `notifyDccSigOutputState(Addr, State)` reçoit `State = pDccMsg->Data[2]` sans
  filtrage — la valeur 0 n'est jamais avalée par la librairie.
- **Pas un bug de câblage/bus** : le moniteur série (`LFX_DCC_AUDIT_ENABLED`)
  montre `notifyDccSigOutputState` appelé correctement pour `State=0` et
  `State=1`, avec un flux de paquets constant (`[DCC] N/M packets in last 2s`).
- **Pas un bug de dédoublonnage** : le compteur `repeatCount` fonctionnait
  correctement sur les événements qui *apparaissaient* dans le journal.

## Cause retenue

Le tampon circulaire du journal (`logDccEvent` dans `DccDrivable.h`) était un
**pool unique de 32 emplacements partagé entre les 5 catégories** de messages
(raw/speed/func/accessory/signal). Une centrale/throttle répète chaque commande
accessoire/signal ~10-15 fois pour la fiabilité (comportement DCC normal) — mais
pendant cette même fenêtre, le trafic Speed/Func (qui varie à presque chaque
répétition en pleine vitesse, cf. commentaire de code existant sur le
dédoublonnage) continue d'écrire dans le **même** pool partagé. Au débit observé
(~130 paquets/s), le seul trafic Speed/Func pouvait faire tourner le pool de 32
emplacements en moins de 0.5s — plus vite que le polling WebUI (1s) — si bien
qu'un événement Signal/Accessoire rare (déjà correctement dédoublonné en une
seule entrée) pouvait être évincé avant même que le navigateur ne l'ait récupéré.

Pas un bug de décodage, pas un bug de dédoublonnage : une course à l'éviction
entre catégories sur une ressource partagée.

## Correctif

Partitionnement du tampon en un ring buffer **par catégorie**
(`dccLog[DCC_MSG_KIND_COUNT][DCC_LOG_CAPACITY]`) — le trafic Speed/Func ne peut
plus structurellement évincer une entrée Accessoire/Signal, quel que soit le
débit du bus ou la cadence de polling. Une fusion par timestamp côté API
(`DeviceStatusApi.cpp`) reconstruit une vue chronologique unique pour l'onglet
"tout".

## Leçon

Face à un événement rare qui disparaît dans un flux à haut débit, soupçonner une
**ressource de capacité fixe partagée entre catégories de traffic très
inégales** avant de soupçonner le décodage lui-même — surtout quand le
symptôme dépend de la *cadence* (bascules rapides échouent, bascules espacées
réussissent) plutôt que de la *valeur* elle-même.

## Voir aussi

- Issue [#76](https://github.com/HO44-PROJECT/MrJ-RailwayFX-backlog/issues/76) — historique complet des itérations (v1 à v5).
- [`usage.md`](../../user/usage.md#diagnostics) — documentation utilisateur de l'onglet Diagnostics DCC.
