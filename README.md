# batterie_Pb.h

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Arduino Badge](https://img.shields.io/badge/framework-arduino-brightgreen?logo=arduino.svg)](https://www.arduino.cc/)
[![PlatformIO](https://img.shields.io/badge/platformio-All_µC-violet?logo=platformio)](https://platformio.org/)
[![Arduino Library Badge](https://www.ardu-badge.com/badge/batterie_Pb.svg)](https://github.com/Fo170?tab=repositories)

Bibliothèque C/C++ complète de gestion des batteries au plomb-acide pour systèmes embarqués (régulateurs solaires, chargeurs intelligents, onduleurs).

## 🚀 Fonctionnalités

- ✅ **Fichier header unique** – Pas de `.c` à compiler
- ✅ **Zero overhead** – Toutes les fonctions sont `static inline`
- ✅ **4 technologies supportées** : Sn-Pb liquide | Ca-Pb liquide | AGM | GEL
- ✅ **Universelle** – Constantes par élément, compatible 2V/6V/12V/24V/48V
- ✅ **Interpolation linéaire** sur tous les tableaux (DOD/SOC quelconques)
- ✅ **Compensation thermique** automatique sur **toutes** les tensions (-5 mV/°C/élément)
- ✅ **Machine à états** CC/CV/Float/Equalization avec **BULK et ABSORPTION distincts**
- ✅ **Comptage de Coulomb** avec réajustement OCV et comptage de cycles
- ✅ Compatible **C99** et **C++11+**
- ✅ Prêt pour **PlatformIO**, **Arduino IDE**, **STM32**, **AVR**, **ESP8266**, **ESP32**, **Raspberry Pi Pico** , ...

---

## 📊 Références & Origine des données

🙏 Remerciements
Anne Labouret & Michel Villoz pour leur ouvrage de référence

![livre](https://raw.githubusercontent.com/Fo170/batterie_Pb/main/Energie_solaire_photovoltaïque_tiny.png)
- 📖 **Labouret & Villoz**, *Énergie solaire photovoltaïque*, Dunod, ISBN **9782100499458** 

Les constantes électriques et les tableaux de tension proviennent principalement, pages 89 à 136 (chapitre accumulateurs et régulation)

📄 Les valeurs pour batteries **liquides** (Sn-Pb et Ca-Pb) sont fidèles au livre. Les valeurs **AGM** et **GEL** sont dérivées des datasheets industrielles (Yuasa, Victron, Exide) avec un ajustement de +0.01 à +0.02 V/élément.

- 🔗 [batterie-solaire.com](https://www.batterie-solaire.com)
- 🔗 [Wikipedia — Batterie au plomb](https://fr.wikipedia.org/wiki/Batterie_au_plomb) — phases CC/CV, floating, auto-décharge

## 📚 Datasheets officielles

| Fabricant | Lien | Types supportés |
|-----------|------|-----------------|
| **Yuasa** | 🔗 [Site officiel](https://www.yuasa.com/) | AGM, GEL, Open |
| **Victron Energy** | 🔗 [Lead Acid Batteries](https://www.victronenergy.com/batteries/gel-and-agm-batteries) | AGM, GEL |
| **Canadian Energy** | 🔗 [Batteries](https://www.cdnrg.com/products/batteries?locale=fr) | batteries plomb-acide inondées, mixtèques, AGM |

### Références normes
- 🔗 [IEEE 450-2010](https://standards.ieee.org/standard/450-2010.html) - Maintenance des batteries plomb-acide
- 🔗 [IEC 60896-21](https://webstore.iec.ch/publication/3895) - Batteries stationnaires VRLA

---

## Caractéristiques

- **Fichier header unique** (`batterie_Pb.h`) — aucun `.c` à linker
- Toutes les fonctions sont `static inline` — zéro overhead d'appel
- **4 technologies** supportées : Sn-Pb liquide, Ca-Pb liquide, AGM, GEL
- **Universelle** — toutes les constantes sont par élément (V/él), compatibles 2V à 48V
- **Interpolation linéaire** sur tous les tableaux (DOD/SOC quelconques)
- **Compensation thermique** automatique sur **toutes** les tensions (-5 mV/°C/élément)
- **Machine à états** CC/CV/Float/Equalization avec **BULK et ABSORPTION distincts** (Vision 2)
- **Comptage de Coulomb** avec réajustement périodique par OCV
- **Comptage de cycles** selon méthode IEC 61427 (80% DOD cumulé = 1 cycle)
- Compatible **C99** et **C++11+**

---

## 📦 Installation

### Arduino IDE

1. Téléchargez le zip depuis GitHub Releases

2. Dans l'IDE : Croquis > Inclure une bibliothèque > Ajouter la bibliothèque .ZIP

### Manuellement

```bash
git clone https://github.com/Fo170/batterie_Pb.git
cp batterie_Pb/src/batterie_Pb.h /chemin/vers/votre/projet/
```

### PlatformIO (recommandé)

```ini
lib_deps =
    fo170/batterie_Pb
```

Ou 

### via pio :
```bash
pio pkg install --library "fo170/batterie_Pb"
```

Ou

### Simplement

Copier simplement `batterie_Pb.h` dans votre projet :

```bash
cp batterie_Pb.h /chemin/vers/votre/projet/src/
```

Inclure dans votre code :

```c
#include "batterie_Pb.h"
```

Aucune étape de compilation/linkage supplémentaire n'est nécessaire.

---

## 💡 Utilisation rapide

```c
#include <stdio.h>
#include "batterie_Pb.h"

int main(void)
{
    bat_pb_t batterie;

    /* Initialisation : AGM 12V (6 éléments), 100 Ah, régulateur CC/CV */
    bat_pb_init(&batterie, BAT_TYPE_AGM, 6, 100.0f, BAT_REGULATOR_CC_CV);
    bat_pb_compute_all(&batterie);  /* Calcule seuils et courants */

    /* Mise à jour avec mesures (V, I, T) - appelé toutes les secondes */
    bat_pb_update_state(&batterie, 14.1f, 2.0f, 25.0f, 1.0f);

    /* Phase de charge actuelle */
    bat_charge_phase_t phase = bat_pb_determine_charge_phase(&batterie);
    printf("Phase : %s\n", bat_pb_phase_to_string(phase));

    /* Tension de délestage pour DOD = 45% avec compensation thermique */
    float V_off = bat_pb_get_disconnect_voltage(BAT_TYPE_AGM, 45.0f,
                                                 BAT_C_RATE_C20, 6,
                                                 25.0f, 25.0f);
    printf("Délestage à 45%% DOD : %.2f V\n", V_off);

    /* SOC estimé depuis tension au repos */
    float soc = bat_pb_estimate_soc_from_ocv(BAT_TYPE_AGM, 12.65f, 6);
    printf("SOC estimé : %.1f %%\n", soc);

    /* Comptage de Coulomb (appel régulier avec dt) */
    bat_pb_update_coulomb_counting(&batterie, 2.0f, 1.0f, false);

    return 0;
}
```

---

## Architecture

### Structures de données

| Structure | Contenu |
|-----------|---------|
| `bat_pb_config_t` | Paramètres fixes (type, nb éléments, capacité, mode régulateur) |
| `bat_pb_thresholds_t` | Seuils de tension calculés [V] |
| `bat_pb_currents_t` | Courants calculés [A] |
| `bat_pb_state_t` | État dynamique (mesures, SOC, phase, compteurs, timer) |
| `bat_pb_t` | Agrégat des 4 structures ci-dessus |

### Énumérations

```c
bat_pb_type_t        /* SN_PB_LIQUIDE, CA_PB_LIQUIDE, AGM, GEL */
bat_soc_level_t      /* CRITICAL, VERY_LOW, LOW, MEDIUM, HIGH, FULL */
bat_charge_phase_t   /* IDLE, BULK, ABSORPTION, FLOATING, EQUALIZATION */
bat_regulator_mode_t /* ON_OFF, CC_CV, MPPT */
bat_c_rate_t         /* C100, C50, C20, C10, C5 */
```

---

## 🔋 Constantes universelles par élément

Toutes les tensions sont définies par élément (V/él) et converties automatiquement :

```c
#define BAT_CELL_CUTOFF_EMPTY       1.75f   /* Coupure décharge */
#define BAT_CELL_CUTOFF_FULL_FLOAT  2.30f   /* Coupure charge */
#define BAT_CELL_OCV_EMPTY          1.75f   /* OCV vide (0% SOC) */
#define BAT_CELL_OCV_FULL           2.15f   /* OCV pleine (100% SOC) */
#define BAT_CELL_BULK_THRESHOLD     2.30f   /* Transition BULK → ABSORPTION */
#define BAT_CELL_ABSORPTION         2.35f   /* Tension absorption */
#define BAT_CELL_FLOATING           2.25f   /* Tension floating */
#define BAT_CELL_GAS_THRESHOLD      2.34f   /* Tension de gazage */
#define BAT_CELL_BOOST              2.50f   /* Tension boost */
#define BAT_CELL_EQUALIZATION       2.55f   /* Tension équalisation */
```

**Compatibilité 12V** (macros de rétrocompatibilité) :
```c
#define BAT_12V_CUTOFF_EMPTY  bat_pb_cell_to_total(BAT_CELL_CUTOFF_EMPTY, 6)  /* 10.5V */
```

→ Fonctionne pour **1 élément (2V), 3 éléments (6V), 6 éléments (12V), 12 éléments (24V), etc.**

---

## Tables de tension

### Délestage (tension de coupure charge)

Indexées par **DOD** (10%, 20%, ..., 100%) et **C-rate** (C/100 à C/5).

```c
float V = bat_pb_get_disconnect_voltage(type, dod_pct, rate, nb_cells, temp, temp_ref);
```

**Interpolation linéaire** : `dod_pct = 33.0f` retourne une valeur interpolée entre 30% et 40%.

**Compensation thermique** : la tension est automatiquement ajustée selon la température.

### Reconnexion (tension de reconnexion chargeur)

Indexées par **SOC** (0%, 10%, ..., 90%) et **C-rate** (C/100 à C/10).

```c
float V = bat_pb_get_reconnect_voltage(type, soc_pct, rate, nb_cells, temp, temp_ref);
```

### OCV → SOC

Table de correspondance tension au repos → état de charge.

```c
float soc = bat_pb_estimate_soc_from_ocv(type, V_ocv, nb_cells);
```

---

## 🌡️ Compensation thermique

**Toutes** les tensions sont compensées automatiquement selon :

```
V_compensé = V_référence + (-0.005 V/°C/élément) × (T - 25°C)
```

Recalcul automatique si ΔT > 2°C lors de `bat_pb_update_state()`.

| Température | V_absorption AGM (12V) | V_absorption AGM (24V) |
|-------------|------------------------|------------------------|
| -10°C | 14.70 V | 29.40 V |
| 0°C   | 14.40 V | 28.80 V |
| 25°C  | 14.10 V | 28.20 V |
| 40°C  | 13.80 V | 27.60 V |

---

## 🔄 Phases de charge (machine à états — Vision 2)

```
IDLE ──► BULK (CC) ──► ABSORPTION (CV) ──► FLOATING ──► [EQUALIZATION] ──► FLOATING
         I = I_max          V = V_abs           V = V_float      V = V_equal
         V monte jusqu'à    I décroît           I = I_float      durée limitée
         V_bulk_threshold   jusqu'à I_end
```

**Vision 2** : BULK et ABSORPTION ont des seuils distincts :
- **BULK** : courant constant, tension monte jusqu'à `V_bulk_threshold` (2.30 V/él)
- **ABSORPTION** : tension constante à `V_absorption` (2.35 V/él), courant décroît

Transitions automatiques via `bat_pb_determine_charge_phase()`.

---

## ⚙️ Constantes par type de batterie

| Paramètre | Sn-Pb Liquide | Ca-Pb Liquide | AGM | GEL |
|-----------|---------------|---------------|-----|-----|
| V absorption [V/él] | 2.35 | 2.40 | 2.35 | 2.40 |
| V floating [V/él] | 2.25 | 2.25 | 2.25 | 2.25 |
| V bulk threshold [V/él] | 2.30 | 2.30 | 2.30 | 2.30 |
| V boost [V/él] | 2.50 | 2.55 | — | — |
| V equalisation [V/él] | 2.55 | 2.55 | — | — |
| I charge max | C/4 | C/4 | C/3.3 | C/5 |
| I floating | C/200 | C/200 | C/333 | C/333 |
| Equalisation | ✅ (2h/30j) | ✅ (2h/30j) | **Non** | **Non** |

---

## 🔧 API complète

### Initialisation et calcul

```c
int bat_pb_init(bat_pb_t *bat, bat_pb_type_t type, uint8_t nb_cells,
                float capacity, bat_regulator_mode_t mode); // Initialise la structure batterie
int bat_pb_compute_thresholds(bat_pb_t *bat);   // Calcule les seuils de tension
int bat_pb_compute_currents(bat_pb_t *bat);   // Calcule les courants
int bat_pb_compute_all(bat_pb_t *bat);        // Calcule seuils + courants
```

### Mise à jour et état

```c
int bat_pb_update_state(bat_pb_t *bat, float V_meas, float I_meas,
                        float temp_meas, float dt_seconds); // Met à jour l'état (V, I, T, dt)
bat_charge_phase_t bat_pb_determine_charge_phase(const bat_pb_t *bat); // Phase de charge
```

### Accès aux tables (interpolation + compensation thermique)

```c
float bat_pb_get_disconnect_voltage(bat_pb_type_t type, float dod_pct,
                                     bat_c_rate_t rate, uint8_t nb_cells,
                                     float temp, float temp_ref); // Délestage compensé
float bat_pb_get_reconnect_voltage(bat_pb_type_t type, float soc_pct,
                                    bat_c_rate_t rate, uint8_t nb_cells,
                                    float temp, float temp_ref); // Reconnexion compensée
float bat_pb_estimate_soc_from_ocv(bat_pb_type_t type, float V_ocv,
                                    uint8_t nb_cells); // SOC depuis OCV
```

### Comptage de Coulomb et cycles

```c
float bat_pb_update_coulomb_counting(bat_pb_t *bat, float I_meas,
                                      float dt_seconds, bool update_ocv); // SOC par Coulomb
```

- Intègre le courant sur le temps (`dt_seconds`)
- Réajuste périodiquement sur OCV si `update_ocv = true` et batterie au repos
- Comptage de cycles selon IEC 61427 (80% DOD cumulé = 1 cycle)
- Conserve le DOD excédentaire après un cycle

### Utilitaires

```c
float bat_pb_temp_compensate(float V_per_cell, float temp, float temp_ref);
float bat_pb_cell_to_total(float V_per_cell, uint8_t nb_cells);
float bat_pb_total_to_cell(float V_total, uint8_t nb_cells);
float bat_pb_interpolate(float x, float x0, float x1, float y0, float y1);
bool bat_pb_is_equalization_allowed(const bat_pb_t *bat);
const char* bat_pb_type_to_string(bat_pb_type_t type);
const char* bat_pb_phase_to_string(bat_charge_phase_t phase);
const char* bat_pb_crate_to_string(bat_c_rate_t rate);
```

---

## Compilation

### Avec GCC

```bash
gcc -std=c99 -O2 -Wall -o mon_projet main.c
```

### Avec Arduino

Copier `batterie_Pb.h` dans le dossier du sketch. Inclure normalement. La bibliothèque utilise `stdint.h` et `stdbool.h` (disponibles sur AVR, ESP32, STM32).

### Avec PlatformIO

Ajouter au `platformio.ini` :

```ini
build_flags = -std=c99
```

---

### 📁 Structure du dépôt
```
batterie_Pb/
├── README.md           # Cette documentation
├── library.json        # PlateformeIO
├── library.properties  # Arduino IDE
├── src/
│   └── batterie_Pb.h   # Fichier unique
├── examples/
│   └── basic_usage/    # Exemple d'utilisation
│       └── main.c
└── LICENSE             # GNU General Public License v3.0
```
---

## 📝 Exemple complet

Voir ./examples/basic_usage/[`main.c`](main.c) pour des cas d'utilisation complets :
- Initialisation et calculs automatiques
- Lecture des seuils de tension
- Interpolation sur les tableaux
- Estimation SOC par OCV
- Comparaison des 4 types de batteries
- Compensation thermique
- Simulation de charge (machine à états Vision 2)
- Gestion de l'équalisation
- Comptage de Coulomb
- Comptage de cycles

---

## 🆕 Changements récents (v1.1.0)

### Nouvelles fonctionnalités
- **Constantes universelles par élément** — compatible 2V/6V/12V/24V/48V
- **BULK et ABSORPTION distincts** (Vision 2) — seuil de transition BULK à 2.30 V/él, absorption à 2.35 V/él
- **Compensation thermique sur toutes les tables** — `get_disconnect_voltage()` et `get_reconnect_voltage()` compensent automatiquement
- **Comptage de Coulomb** — intégration du courant avec réajustement OCV
- **Comptage de cycles** — méthode IEC 61477 avec conservation du DOD excédentaire
- **Timer de charge fonctionnel** — `charge_time_hours` incrémenté avec `dt_seconds`

### Corrections
- Constantes OCV alignées avec les tables (1.75V / 2.15V)
- Compteurs Ah proportionnels au `dt_seconds` (fonctionne avec n'importe quelle période d'échantillonnage)
- Reset du timer de charge quand pas en charge

### Changements d'API
- `bat_pb_update_state()` prend `dt_seconds` en paramètre
- `bat_pb_get_disconnect_voltage()` et `get_reconnect_voltage()` prennent `temp` et `temp_ref`
- Macros `BAT_12V_*` conservées pour rétrocompatibilité

---

## Limites et précautions

- Les tables **AGM** et **GEL** sont des dérivées des valeurs liquides. Pour des applications critiques, vérifier avec la datasheet du fabricant exact.
- L'estimation SOC par OCV nécessite une batterie au repos (pas de courant significatif pendant > 2h).
- La compensation thermique est linéaire ; en dessous de 0°C ou au-dessus de 50°C, consulter le fabricant.
- L'équalisation est **interdite** pour AGM et GEL (risque de destruction).
- Le comptage de Coulomb nécessite un appel régulier avec un `dt_seconds` précis.

---

## Licence

GNU General Public License v3.0. 

Les données du livre Labouret & Villoz sont des constantes physiques ; les sources web sont citées.

---
