/**
 * @file batterie_Pb.h
 * @brief Bibliotheque complete de gestion des batteries au plomb-acide
 * @version 1.2.0 - Corrections critiques + ameliorations
 *         + timer hysteresis, timer equalization separe
 *         + synchronisation flags, auto-decharge
 *         + seuil OCV configurable, codes erreur specifiques
 * @author Fo170
 *
 * Base sur le livre "Energie solaire photovoltaique" de Anne Labouret
 * et Michel Villoz (p.89-136)
 * et sources : https://www.batterie-solaire.com/accumulateur-plomb-principe-fonctionnement.htm
 * https://fr.wikipedia.org/wiki/Batterie_au_plomb
 *
 * Toutes les fonctions sont inline ou statiques - fichier header unique.
 * Les tableaux de delestage/reconnexion proviennent du livre (batteries liquides).
 * Les valeurs AGM/GEL sont derivees des datasheets industrielles.
 */

#ifndef _BAT_PB_H_
#define _BAT_PB_H_


// Version de la librairie (source UNIQUE — agrégée par liste_lib.h)
#define __BATTERIE_PB_H_VERSION__ "1.1.0"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 *  SECTION 1: CODES D'ERREUR
 * ========================================================================== */

#define BAT_OK                          0
#define BAT_ERR_NULL_PTR               -1
#define BAT_ERR_INVALID_TYPE           -2
#define BAT_ERR_ZERO_CELLS             -3
#define BAT_ERR_NEG_CAPACITY           -4
#define BAT_ERR_INVALID_C_RATE         -5
#define BAT_ERR_INVALID_DOD            -6
#define BAT_ERR_INVALID_SOC            -7
#define BAT_ERR_INVALID_DT             -8

/* ==========================================================================
 *  SECTION 2: CONSTANTES PHYSIQUES ET ELECTROCHIMIQUES
 * ========================================================================== */

/** Rendement energetique theorique des batteries au plomb (70-75%) */
#define BAT_PB_EFFICIENCY_THEORETICAL   0.70f

/** Force electromotrice (f.e.m.) d'un element plomb/acide au repos [V/element] */
#define BAT_PB_EMF_PER_CELL             2.04f

/** Tension nominale par element (en charge legere) [V/element] */
#define BAT_PB_V_NOMINAL_PER_CELL       2.10f

/** Tension de seuil de gazage (electrolyse de l'eau) [V/element] @ 25C */
#define BAT_PB_V_GAS_PER_CELL           2.34f

/** Coefficient de compensation thermique standard [V/C/element] */
#define BAT_PB_TEMP_COEFF_PER_CELL      (-0.005f)   /* -5 mV/C/element */

/** Temperature de reference pour les reglages [C] */
#define BAT_PB_TEMP_REF                 25.0f

/** Taux d'auto-decharge mensuel a 25C [%] */
#define BAT_PB_SELF_DISCHARGE_PER_MONTH 5.0f

/** Profondeur de decharge maximale recommandee [%] */
#define BAT_PB_MAX_DOD_RECOMMENDED      80.0f

/** Profondeur de decharge critique (endommagement) [%] */
#define BAT_PB_CRITICAL_DOD             100.0f

/** Seuil de courant pour detecter la charge [fraction de C] */
#define BAT_CHARGE_CURRENT_THRESHOLD_C  0.0005f     /* 0.05% de C */

/** Seuil de courant pour detecter la decharge [fraction de C] */
#define BAT_DISCHARGE_CURRENT_THRESHOLD_C (-0.0005f)  /* -0.05% de C */

/** Seuil de courant pour estimation OCV [fraction de C] */
#define BAT_OCV_CURRENT_THRESHOLD_C     0.001f      /* 0.1% de C = vrai repos */

/** Duree minimale d'absorption [heures] */
#define BAT_MIN_ABSORPTION_TIME_H       1.0f

/** Duree maximale d'absorption [heures] */
#define BAT_MAX_ABSORPTION_TIME_H       8.0f

/** Seuil SOC pour considerer la batterie pleine [%] */
#define BAT_SOC_FULL_THRESHOLD          95.0f

/** Seuil SOC pour etat critique [%] */
#define BAT_SOC_CRITICAL_THRESHOLD      10.0f

/** Courant de fin d'absorption comme fraction de C */
#define BAT_ABS_END_CURRENT_C           0.02f

/** Delta temperature minimum pour recalculer les seuils [C] */
#define BAT_TEMP_RECALC_DELTA           2.0f

/** Duree avant reset du timer de charge en cas d'interruption [secondes] */
#define BAT_CHARGE_TIMER_HYSTERESIS_S   60.0f       /* 1 minute de grace */

/* ==========================================================================
 *  SECTION 3: SEUILS DE TENSION PAR ELEMENT [V/element @ 25C]
 *  Universels - utiliser bat_pb_cell_to_total() pour n'importe quel nombre
 *  d'elements (1, 3, 6, 12, etc.)
 * ========================================================================== */

#define BAT_CELL_CUTOFF_EMPTY           1.75f   /* Coupure decharge (vide) */
#define BAT_CELL_CUTOFF_FULL_FLOAT      2.30f   /* Coupure charge (floating) */
#define BAT_CELL_OCV_EMPTY              1.75f   /* OCV batterie vide - aligne table OCV */
#define BAT_CELL_OCV_FULL               2.15f   /* OCV batterie pleine - aligne table OCV */
#define BAT_CELL_BULK_THRESHOLD         2.30f   /* Transition bulk -> absorption (Vision 2) */
#define BAT_CELL_ABSORPTION             2.35f   /* Tension d'absorption */
#define BAT_CELL_FLOATING               2.25f   /* Tension de floating */
#define BAT_CELL_GAS_THRESHOLD          2.34f   /* Tension de gazage */
#define BAT_CELL_BOOST                  2.50f   /* Tension de boost */
#define BAT_CELL_EQUALIZATION           2.55f   /* Tension d'equalisation */

/* Macros de compatibilite pour code existant (12V = 6 elements) */
#define BAT_12V_CUTOFF_EMPTY            bat_pb_cell_to_total(BAT_CELL_CUTOFF_EMPTY, 6)
#define BAT_12V_CUTOFF_FULL_FLOAT       bat_pb_cell_to_total(BAT_CELL_CUTOFF_FULL_FLOAT, 6)
#define BAT_12V_OCV_EMPTY               bat_pb_cell_to_total(BAT_CELL_OCV_EMPTY, 6)
#define BAT_12V_OCV_FULL                bat_pb_cell_to_total(BAT_CELL_OCV_FULL, 6)
#define BAT_12V_BULK_THRESHOLD          bat_pb_cell_to_total(BAT_CELL_BULK_THRESHOLD, 6)
#define BAT_12V_ABSORPTION              bat_pb_cell_to_total(BAT_CELL_ABSORPTION, 6)
#define BAT_12V_FLOATING                bat_pb_cell_to_total(BAT_CELL_FLOATING, 6)
#define BAT_12V_GAS_THRESHOLD           bat_pb_cell_to_total(BAT_CELL_GAS_THRESHOLD, 6)
#define BAT_12V_BOOST                   bat_pb_cell_to_total(BAT_CELL_BOOST, 6)

/* ==========================================================================
 *  SECTION 4: ENUMERATIONS
 * ========================================================================== */

/** Technologies de batteries au plomb supportees */
typedef enum {
    BAT_TYPE_SN_PB_LIQUIDE = 0,  /**< Plomb-antimoine, electrolyte liquide */
    BAT_TYPE_CA_PB_LIQUIDE = 1,  /**< Plomb-calcium, electrolyte liquide */
    BAT_TYPE_AGM = 2,            /**< Absorbent Glass Mat */
    BAT_TYPE_GEL = 3,            /**< Electrolyte en gel */
    BAT_TYPE_COUNT               /**< Nombre total de types */
} bat_pb_type_t;

/** Etats de charge (State of Charge) approximatifs */
typedef enum {
    BAT_SOC_CRITICAL = 0,   /**< < 10% - danger permanent */
    BAT_SOC_VERY_LOW = 1,   /**< 10-20% - decharge profonde */
    BAT_SOC_LOW = 2,        /**< 20-50% - decharge partielle */
    BAT_SOC_MEDIUM = 3,     /**< 50-80% - etat normal */
    BAT_SOC_HIGH = 4,       /**< 80-95% - bien chargee */
    BAT_SOC_FULL = 5        /**< > 95% - pleine charge */
} bat_soc_level_t;

/** Phases de charge CC/CV */
typedef enum {
    BAT_CHARGE_PHASE_IDLE = 0,          /**< Aucune charge en cours */
    BAT_CHARGE_PHASE_BULK,              /**< Courant constant (CC) */
    BAT_CHARGE_PHASE_ABSORPTION,        /**< Tension constante (CV) */
    BAT_CHARGE_PHASE_FLOATING,          /**< Floating/entretien */
    BAT_CHARGE_PHASE_EQUALIZATION       /**< Equalisation (liquide uniquement) */
} bat_charge_phase_t;

/** Modes de regulation du chargeur */
typedef enum {
    BAT_REGULATOR_ON_OFF = 0,   /**< Regulation tout ou rien (simple) */
    BAT_REGULATOR_CC_CV = 1,    /**< Regulation a tension constante */
    BAT_REGULATOR_MPPT = 2      /**< MPPT */
} bat_regulator_mode_t;

/** Taux de decharge (C-rate) pour l'indexation des tables */
typedef enum {
    BAT_C_RATE_C100 = 0,    /**< Decharge sur 100h */
    BAT_C_RATE_C50 = 1,     /**< Decharge sur 50h */
    BAT_C_RATE_C20 = 2,     /**< Decharge sur 20h (standard) */
    BAT_C_RATE_C10 = 3,     /**< Decharge sur 10h */
    BAT_C_RATE_C5 = 4,      /**< Decharge sur 5h */
    BAT_C_RATE_COUNT        /**< Nombre de taux supportes */
} bat_c_rate_t;

/* ==========================================================================
 *  SECTION 5: CONSTANTES PAR TYPE [V/element @ 25C]
 * ========================================================================== */

/** Tension de fin de charge (coupure haute) - mode tout ou rien [V/element] */
static const float BAT_VFC_PER_CELL[BAT_TYPE_COUNT] = {
    /* SN-Pb Liquide */ 2.40f,
    /* CA-Pb Liquide */ 2.45f,
    /* AGM           */ 2.35f,
    /* GEL           */ 2.35f
};

/** Tension de recharge (reconnexion chargeur) - mode tout ou rien [V/element] */
static const float BAT_VRC_PER_CELL[BAT_TYPE_COUNT] = {
    /* SN-Pb Liquide */ 2.25f,
    /* CA-Pb Liquide */ 2.30f,
    /* AGM           */ 2.20f,
    /* GEL           */ 2.20f
};

/** Tension de boost (equalisation) - mode CC/CV [V/element] */
static const float BAT_VBOOST_PER_CELL[BAT_TYPE_COUNT] = {
    /* SN-Pb Liquide */ 2.50f,
    /* CA-Pb Liquide */ 2.55f,
    /* AGM           */ 0.00f,    /* NON RECOMMANDE */
    /* GEL           */ 0.00f     /* NON RECOMMANDE */
};

/** Tension de charge (absorption) - mode CC/CV [V/element] */
static const float BAT_VCH_PER_CELL[BAT_TYPE_COUNT] = {
    /* SN-Pb Liquide */ 2.35f,
    /* CA-Pb Liquide */ 2.40f,
    /* AGM           */ 2.35f,
    /* GEL           */ 2.40f
};

/** Tension de floating (entretien) - mode CC/CV [V/element] */
static const float BAT_VFL_PER_CELL[BAT_TYPE_COUNT] = {
    /* SN-Pb Liquide */ 2.25f,
    /* CA-Pb Liquide */ 2.25f,
    /* AGM           */ 2.25f,
    /* GEL           */ 2.25f
};

/** Tension d'equalisation - mode CC/CV [V/element] */
static const float BAT_VEG_PER_CELL[BAT_TYPE_COUNT] = {
    /* SN-Pb Liquide */ 2.55f,   /* 0.5 jour / 30 jours */
    /* CA-Pb Liquide */ 2.55f,   /* 0.5 jour / 30 jours */
    /* AGM           */ 0.00f,    /* Interdit */
    /* GEL           */ 0.00f     /* Interdit */
};

/** Courant de charge maximal recommande [fraction de C] */
static const float BAT_I_CHARGE_MAX_C[BAT_TYPE_COUNT] = {
    /* SN-Pb Liquide */ 0.25f,   /* C/4 */
    /* CA-Pb Liquide */ 0.25f,   /* C/4 */
    /* AGM           */ 0.30f,   /* C/3.3 */
    /* GEL           */ 0.20f    /* C/5 */
};

/** Courant de floating recommande [fraction de C] */
static const float BAT_I_FLOAT_C[BAT_TYPE_COUNT] = {
    /* SN-Pb Liquide */ 0.005f,  /* C/200 */
    /* CA-Pb Liquide */ 0.005f,  /* C/200 */
    /* AGM           */ 0.003f,  /* C/333 */
    /* GEL           */ 0.003f   /* C/333 */
};

/** Duree d'equalisation recommandee [heures] */
static const float BAT_EQUALIZATION_DURATION_H[BAT_TYPE_COUNT] = {
    /* SN-Pb Liquide */ 2.0f,
    /* CA-Pb Liquide */ 2.0f,
    /* AGM           */ 0.0f,
    /* GEL           */ 0.0f
};

/** Periode entre equalisations [jours] */
static const uint16_t BAT_EQUALIZATION_PERIOD_DAYS[BAT_TYPE_COUNT] = {
    /* SN-Pb Liquide */ 30,
    /* CA-Pb Liquide */ 30,
    /* AGM           */ 0,
    /* GEL           */ 0
};

/* ==========================================================================
 *  SECTION 6: TABLES DE TENSION
 * ========================================================================== */

#define BAT_DOD_TABLE_SIZE 10
#define BAT_SOC_TABLE_SIZE 10
#define BAT_OCV_TABLE_SIZE 11

static const float BAT_VDL_C100_PER_CELL[BAT_TYPE_COUNT][BAT_DOD_TABLE_SIZE] = {
    {2.14f, 2.12f, 2.10f, 2.08f, 2.05f, 2.02f, 2.00f, 1.96f, 1.92f, 1.80f},
    {2.14f, 2.12f, 2.10f, 2.08f, 2.05f, 2.02f, 2.00f, 1.96f, 1.92f, 1.80f},
    {2.15f, 2.13f, 2.11f, 2.09f, 2.06f, 2.03f, 2.01f, 1.97f, 1.93f, 1.82f},
    {2.16f, 2.14f, 2.12f, 2.10f, 2.07f, 2.04f, 2.02f, 1.98f, 1.94f, 1.83f}
};

static const float BAT_VDL_C50_PER_CELL[BAT_TYPE_COUNT][BAT_DOD_TABLE_SIZE] = {
    {2.13f, 2.11f, 2.09f, 2.07f, 2.04f, 2.01f, 1.99f, 1.95f, 1.91f, 1.79f},
    {2.13f, 2.11f, 2.09f, 2.07f, 2.04f, 2.01f, 1.99f, 1.95f, 1.91f, 1.79f},
    {2.14f, 2.12f, 2.10f, 2.08f, 2.05f, 2.02f, 2.00f, 1.96f, 1.92f, 1.81f},
    {2.15f, 2.13f, 2.11f, 2.09f, 2.06f, 2.03f, 2.01f, 1.97f, 1.93f, 1.82f}
};

static const float BAT_VDL_C20_PER_CELL[BAT_TYPE_COUNT][BAT_DOD_TABLE_SIZE] = {
    {2.11f, 2.09f, 2.07f, 2.05f, 2.03f, 2.00f, 1.98f, 1.95f, 1.91f, 1.80f},
    {2.11f, 2.09f, 2.07f, 2.05f, 2.03f, 2.00f, 1.98f, 1.95f, 1.91f, 1.80f},
    {2.12f, 2.10f, 2.08f, 2.06f, 2.04f, 2.01f, 1.99f, 1.96f, 1.92f, 1.81f},
    {2.13f, 2.11f, 2.09f, 2.07f, 2.05f, 2.02f, 2.00f, 1.97f, 1.93f, 1.82f}
};

static const float BAT_VDL_C10_PER_CELL[BAT_TYPE_COUNT][BAT_DOD_TABLE_SIZE] = {
    {2.08f, 2.07f, 2.05f, 2.04f, 2.01f, 1.99f, 1.96f, 1.93f, 1.89f, 1.80f},
    {2.08f, 2.07f, 2.05f, 2.04f, 2.01f, 1.99f, 1.96f, 1.93f, 1.89f, 1.80f},
    {2.09f, 2.08f, 2.06f, 2.05f, 2.02f, 2.00f, 1.97f, 1.94f, 1.90f, 1.81f},
    {2.10f, 2.09f, 2.07f, 2.06f, 2.03f, 2.01f, 1.98f, 1.95f, 1.91f, 1.82f}
};

static const float BAT_VDL_C5_PER_CELL[BAT_TYPE_COUNT][BAT_DOD_TABLE_SIZE] = {
    {2.05f, 2.04f, 2.02f, 2.01f, 1.98f, 1.96f, 1.93f, 1.90f, 1.86f, 1.78f},
    {2.05f, 2.04f, 2.02f, 2.01f, 1.98f, 1.96f, 1.93f, 1.90f, 1.86f, 1.78f},
    {2.06f, 2.05f, 2.03f, 2.02f, 1.99f, 1.97f, 1.94f, 1.91f, 1.87f, 1.79f},
    {2.07f, 2.06f, 2.04f, 2.03f, 2.00f, 1.98f, 1.95f, 1.92f, 1.88f, 1.80f}
};

static const float BAT_VRL_C10_PER_CELL[BAT_TYPE_COUNT][BAT_SOC_TABLE_SIZE] = {
    {2.08f, 2.09f, 2.12f, 2.15f, 2.19f, 2.23f, 2.27f, 2.34f, 2.43f, 2.61f},
    {2.08f, 2.09f, 2.12f, 2.15f, 2.19f, 2.23f, 2.27f, 2.34f, 2.43f, 2.61f},
    {2.09f, 2.10f, 2.13f, 2.16f, 2.20f, 2.24f, 2.28f, 2.35f, 2.44f, 2.62f},
    {2.10f, 2.11f, 2.14f, 2.17f, 2.21f, 2.25f, 2.29f, 2.36f, 2.45f, 2.63f}
};

static const float BAT_VRL_C20_PER_CELL[BAT_TYPE_COUNT][BAT_SOC_TABLE_SIZE] = {
    {2.05f, 2.07f, 2.10f, 2.13f, 2.17f, 2.21f, 2.25f, 2.32f, 2.43f, 2.60f},
    {2.05f, 2.07f, 2.10f, 2.13f, 2.17f, 2.21f, 2.25f, 2.32f, 2.43f, 2.60f},
    {2.06f, 2.08f, 2.11f, 2.14f, 2.18f, 2.22f, 2.26f, 2.33f, 2.44f, 2.61f},
    {2.07f, 2.09f, 2.12f, 2.15f, 2.19f, 2.23f, 2.27f, 2.34f, 2.45f, 2.62f}
};

static const float BAT_VRL_C50_PER_CELL[BAT_TYPE_COUNT][BAT_SOC_TABLE_SIZE] = {
    {2.01f, 2.03f, 2.07f, 2.10f, 2.14f, 2.17f, 2.21f, 2.27f, 2.34f, 2.47f},
    {2.01f, 2.03f, 2.07f, 2.10f, 2.14f, 2.17f, 2.21f, 2.27f, 2.34f, 2.47f},
    {2.02f, 2.04f, 2.08f, 2.11f, 2.15f, 2.18f, 2.22f, 2.28f, 2.35f, 2.48f},
    {2.03f, 2.05f, 2.09f, 2.12f, 2.16f, 2.19f, 2.23f, 2.29f, 2.36f, 2.49f}
};

static const float BAT_VRL_C100_PER_CELL[BAT_TYPE_COUNT][BAT_SOC_TABLE_SIZE] = {
    {1.99f, 2.02f, 2.06f, 2.09f, 2.13f, 2.16f, 2.20f, 2.26f, 2.32f, 2.46f},
    {1.99f, 2.02f, 2.06f, 2.09f, 2.13f, 2.16f, 2.20f, 2.26f, 2.32f, 2.46f},
    {2.00f, 2.03f, 2.07f, 2.10f, 2.14f, 2.17f, 2.21f, 2.27f, 2.33f, 2.47f},
    {2.01f, 2.04f, 2.08f, 2.11f, 2.15f, 2.18f, 2.22f, 2.28f, 2.34f, 2.48f}
};

static const float BAT_OCV_SOC_TABLE[BAT_OCV_TABLE_SIZE] = {
    0.0f, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f,
    60.0f, 70.0f, 80.0f, 90.0f, 100.0f
};

static const float BAT_OCV_PER_CELL[BAT_TYPE_COUNT][BAT_OCV_TABLE_SIZE] = {
    /* SN-Pb Liquide */
    {1.75f, 1.89f, 1.96f, 2.00f, 2.03f, 2.05f, 2.07f, 2.09f, 2.11f, 2.13f, 2.15f},
    /* CA-Pb Liquide */
    {1.75f, 1.89f, 1.96f, 2.00f, 2.03f, 2.05f, 2.07f, 2.09f, 2.11f, 2.13f, 2.15f},
    /* AGM */
    {1.76f, 1.90f, 1.97f, 2.01f, 2.04f, 2.06f, 2.08f, 2.10f, 2.12f, 2.14f, 2.16f},
    /* GEL */
    {1.77f, 1.91f, 1.98f, 2.02f, 2.05f, 2.07f, 2.09f, 2.11f, 2.13f, 2.15f, 2.17f}
};

/* ==========================================================================
 *  SECTION 7: STRUCTURES DE DONNEES
 * ========================================================================== */

/** Configuration statique de la batterie */
typedef struct {
    uint8_t nb_elements;            /**< Nombre d'elements en serie */
    bat_pb_type_t type;             /**< Technologie */
    bat_regulator_mode_t regulator_mode;  /**< Mode de regulation */
    float capacity_ah;              /**< Capacite nominale [Ah] @ C20 */
    float temp_ref;                 /**< Temperature de reference [C] */
} bat_pb_config_t;

/** Seuils de tension calcules pour la batterie [V] */
typedef struct {
    float V_cutoff_discharge;         /**< Coupure decharge (vide) [V] */
    float V_cutoff_charge;          /**< Coupure charge (floating) [V] */
    float V_gas;                    /**< Tension de gazage [V] */
    float V_bulk;                   /**< Transition bulk -> absorption [V] */
    float V_absorption;               /**< Tension d'absorption [V] */
    float V_floating;                 /**< Tension de floating [V] */
    float V_boost;                  /**< Tension de boost [V] */
    float V_equalization;             /**< Tension d'equalization [V] */
    float V_ocv_empty;                /**< OCV batterie vide [V] */
    float V_ocv_full;                 /**< OCV batterie pleine [V] */
    float V_end_of_charge;            /**< Fin de charge (coupure haute) [V] */
    float V_recharge;                 /**< Recharge (reconnexion) [V] */
    float V_delestage;                /**< Delestage [V] */
    float V_reconnect;                /**< Reconnexion [V] */
} bat_pb_thresholds_t;

/** Courants calcules pour la batterie [A] */
typedef struct {
    float I_charge_max;               /**< Courant de charge max [A] */
    float I_bulk_target;              /**< Courant cible bulk [A] */
    float I_absorption_end;           /**< Courant fin absorption [A] */
    float I_float;                    /**< Courant de floating [A] */
    float I_equalization;             /**< Courant d'equalization [A] */
} bat_pb_currents_t;

/** Etat dynamique de la batterie */
typedef struct {
    float V_batt;                     /**< Tension mesuree [V] */
    float I_batt;                     /**< Courant mesure [A] (>0 charge) */
    float temperature;                /**< Temperature [C] */
    float V_batt_compensated;         /**< Tension compensee [V] */
    float V_per_cell;                 /**< Tension par element [V/el] */
    float V_per_cell_compensated;     /**< Tension par el compensee [V/el] */
    float soc_percent;                /**< SOC [%] */
    float dod_percent;                /**< DOD [%] */
    bat_soc_level_t soc_level;        /**< Niveau qualitatif SOC */
    bat_charge_phase_t charge_phase;  /**< Phase de charge */
    float charge_time_hours;          /**< Duree phase charge/absorption [h] */
    float equalization_time_hours;    /**< Duree phase equalization [h] - SEPARE */
    float charge_timer_hysteresis_s;  /**< Compteur hysteresis reset [s] */
    bool is_charging;                 /**< En charge */
    bool is_discharging;              /**< En decharge */
    bool is_floating;                 /**< En floating */
    bool is_equalizing;               /**< En equalization */
    bool is_critical;                 /**< Etat critique */
    bool needs_equalization;          /**< Equalisation necessaire */
    uint32_t cycle_count;             /**< Nombre de cycles */
    float total_ah_charged;           /**< Ah charges cumules */
    float total_ah_discharged;        /**< Ah decharges cumules */
    uint32_t days_since_equalization;   /**< Jours depuis equalization */
    float coulomb_ah;                 /**< Integrale de Coulomb (Ah) */
    float last_soc_for_cycles;        /**< Dernier SOC pour comptage cycles */
    float cumulative_dod;             /**< DOD cumule pour cycles */
    float last_temperature;           /**< Derniere temperature pour recalibrage */
    bool first_measurement;           /**< Premiere mesure de temperature */
} bat_pb_state_t;

/** Structure principale */
typedef struct {
    bat_pb_config_t config;
    bat_pb_thresholds_t thresholds;
    bat_pb_currents_t currents;
    bat_pb_state_t state;
} bat_pb_t;

/* ==========================================================================
 *  SECTION 8: FONCTIONS UTILITAIRES (INLINE)
 * ========================================================================== */

static inline float bat_pb_temp_compensate(float V_per_cell, float temp, float temp_ref) {
    return V_per_cell + BAT_PB_TEMP_COEFF_PER_CELL * (temp - temp_ref);
}

static inline float bat_pb_cell_to_total(float V_per_cell, uint8_t nb_cells) {
    return V_per_cell * (float)nb_cells;
}

static inline float bat_pb_total_to_cell(float V_total, uint8_t nb_cells) {
    return (nb_cells > 0) ? (V_total / (float)nb_cells) : 0.0f;
}

static inline float bat_pb_interpolate(float x, float x0, float x1, float y0, float y1) {
    if (x <= x0) return y0;
    if (x >= x1) return y1;
    return y0 + (y1 - y0) * (x - x0) / (x1 - x0);
}

static inline const char* bat_pb_type_to_string(bat_pb_type_t type) {
    switch (type) {
        case BAT_TYPE_SN_PB_LIQUIDE: return "Plomb-Antimoine (Liquide)";
        case BAT_TYPE_CA_PB_LIQUIDE: return "Plomb-Calcium (Liquide)";
        case BAT_TYPE_AGM: return "AGM";
        case BAT_TYPE_GEL: return "GEL";
        default: return "Inconnu";
    }
}

static inline const char* bat_pb_phase_to_string(bat_charge_phase_t phase) {
    switch (phase) {
        case BAT_CHARGE_PHASE_IDLE: return "Inactif";
        case BAT_CHARGE_PHASE_BULK: return "Bulk (CC)";
        case BAT_CHARGE_PHASE_ABSORPTION: return "Absorption (CV)";
        case BAT_CHARGE_PHASE_FLOATING: return "Floating";
        case BAT_CHARGE_PHASE_EQUALIZATION: return "Equalization";
        default: return "Inconnu";
    }
}

static inline const char* bat_pb_crate_to_string(bat_c_rate_t rate) {
    switch (rate) {
        case BAT_C_RATE_C100: return "C/100";
        case BAT_C_RATE_C50: return "C/50";
        case BAT_C_RATE_C20: return "C/20";
        case BAT_C_RATE_C10: return "C/10";
        case BAT_C_RATE_C5: return "C/5";
        default: return "Inconnu";
    }
}

static inline const char* bat_pb_err_to_string(int err) {
    switch (err) {
        case BAT_OK: return "OK";
        case BAT_ERR_NULL_PTR: return "Pointeur NULL";
        case BAT_ERR_INVALID_TYPE: return "Type invalide";
        case BAT_ERR_ZERO_CELLS: return "0 elements";
        case BAT_ERR_NEG_CAPACITY: return "Capacite negative";
        case BAT_ERR_INVALID_C_RATE: return "C-rate invalide";
        case BAT_ERR_INVALID_DOD: return "DOD invalide";
        case BAT_ERR_INVALID_SOC: return "SOC invalide";
        case BAT_ERR_INVALID_DT: return "dt invalide";
        default: return "Erreur inconnue";
    }
}

/* ==========================================================================
 *  SECTION 9: FONCTIONS D'ACCES AUX TABLEAUX
 * ========================================================================== */

static inline float bat_pb_get_disconnect_voltage(bat_pb_type_t type, float dod_pct,
                                                   bat_c_rate_t rate, uint8_t nb_cells,
                                                   float temp, float temp_ref)
{
    if (type >= BAT_TYPE_COUNT) return 0.0f;
    if (dod_pct < 0.0f || dod_pct > 100.0f) return 0.0f;
    if (nb_cells == 0) return 0.0f;

    const float (*table)[BAT_DOD_TABLE_SIZE];
    switch (rate) {
        case BAT_C_RATE_C100: table = BAT_VDL_C100_PER_CELL; break;
        case BAT_C_RATE_C50:  table = BAT_VDL_C50_PER_CELL;  break;
        case BAT_C_RATE_C20:  table = BAT_VDL_C20_PER_CELL;  break;
        case BAT_C_RATE_C10:  table = BAT_VDL_C10_PER_CELL;  break;
        case BAT_C_RATE_C5:   table = BAT_VDL_C5_PER_CELL;   break;
        default: return 0.0f;
    }

    float idx_f = (dod_pct / 10.0f) - 1.0f;
    int idx_low = (int)idx_f;
    int idx_high = idx_low + 1;

    if (idx_low < 0) {
        float V_cell_comp = bat_pb_temp_compensate(table[type][0], temp, temp_ref);
        return bat_pb_cell_to_total(V_cell_comp, nb_cells);
    }
    if (idx_high >= BAT_DOD_TABLE_SIZE) {
        float V_cell_comp = bat_pb_temp_compensate(table[type][BAT_DOD_TABLE_SIZE - 1], temp, temp_ref);
        return bat_pb_cell_to_total(V_cell_comp, nb_cells);
    }

    float frac = idx_f - (float)idx_low;
    float V_cell = table[type][idx_low] + frac * (table[type][idx_high] - table[type][idx_low]);
    float V_cell_comp = bat_pb_temp_compensate(V_cell, temp, temp_ref);
    return bat_pb_cell_to_total(V_cell_comp, nb_cells);
}

static inline float bat_pb_get_reconnect_voltage(bat_pb_type_t type, float soc_pct,
                                                  bat_c_rate_t rate, uint8_t nb_cells,
                                                  float temp, float temp_ref)
{
    if (type >= BAT_TYPE_COUNT) return 0.0f;
    if (soc_pct < 0.0f || soc_pct > 100.0f) return 0.0f;
    if (nb_cells == 0) return 0.0f;

    const float (*table)[BAT_SOC_TABLE_SIZE];
    switch (rate) {
        case BAT_C_RATE_C100: table = BAT_VRL_C100_PER_CELL; break;
        case BAT_C_RATE_C50:  table = BAT_VRL_C50_PER_CELL;  break;
        case BAT_C_RATE_C20:  table = BAT_VRL_C20_PER_CELL;  break;
        case BAT_C_RATE_C10:  table = BAT_VRL_C10_PER_CELL;  break;
        default: return 0.0f;
    }

    float idx_f = soc_pct / 10.0f;
    int idx_low = (int)idx_f;
    int idx_high = idx_low + 1;

    if (idx_low < 0) {
        float V_cell_comp = bat_pb_temp_compensate(table[type][0], temp, temp_ref);
        return bat_pb_cell_to_total(V_cell_comp, nb_cells);
    }
    if (idx_high >= BAT_SOC_TABLE_SIZE) {
        float V_cell_comp = bat_pb_temp_compensate(table[type][BAT_SOC_TABLE_SIZE - 1], temp, temp_ref);
        return bat_pb_cell_to_total(V_cell_comp, nb_cells);
    }

    float frac = idx_f - (float)idx_low;
    float V_cell = table[type][idx_low] + frac * (table[type][idx_high] - table[type][idx_low]);
    float V_cell_comp = bat_pb_temp_compensate(V_cell, temp, temp_ref);
    return bat_pb_cell_to_total(V_cell_comp, nb_cells);
}

static inline float bat_pb_estimate_soc_from_ocv(bat_pb_type_t type, float V_ocv, uint8_t nb_cells)
{
    if (type >= BAT_TYPE_COUNT || nb_cells == 0) {
        return -1.0f;
    }

    float V_cell = bat_pb_total_to_cell(V_ocv, nb_cells);
    const float *ocv_table = BAT_OCV_PER_CELL[type];

    if (V_cell <= ocv_table[0]) return 0.0f;
    if (V_cell >= ocv_table[BAT_OCV_TABLE_SIZE - 1]) return 100.0f;

    for (int i = 0; i < BAT_OCV_TABLE_SIZE - 1; i++) {
        if (V_cell >= ocv_table[i] && V_cell <= ocv_table[i + 1]) {
            return bat_pb_interpolate(V_cell, ocv_table[i], ocv_table[i + 1],
                                       BAT_OCV_SOC_TABLE[i], BAT_OCV_SOC_TABLE[i + 1]);
        }
    }
    return -1.0f;
}

static inline bool bat_pb_is_equalization_allowed(const bat_pb_t *bat)
{
    if (bat == NULL) return false;
    bat_pb_type_t t = bat->config.type;
    if (BAT_VEG_PER_CELL[t] <= 0.0f) return false;
    if (bat->state.days_since_equalization < BAT_EQUALIZATION_PERIOD_DAYS[t]) return false;
    if (bat->state.soc_percent < BAT_SOC_FULL_THRESHOLD) return false;
    return true;
}

/* ==========================================================================
 *  SECTION 10: INITIALISATION ET CALCUL
 * ========================================================================== */

static inline int bat_pb_init(bat_pb_t *bat, bat_pb_type_t type, uint8_t nb_cells,
                               float capacity, bat_regulator_mode_t mode)
{
    if (bat == NULL) return BAT_ERR_NULL_PTR;
    if (type >= BAT_TYPE_COUNT) return BAT_ERR_INVALID_TYPE;
    if (nb_cells == 0) return BAT_ERR_ZERO_CELLS;
    if (capacity <= 0.0f) return BAT_ERR_NEG_CAPACITY;

    memset(bat, 0, sizeof(bat_pb_t));

    bat->config.nb_elements = nb_cells;
    bat->config.type = type;
    bat->config.regulator_mode = mode;
    bat->config.capacity_ah = capacity;
    bat->config.temp_ref = BAT_PB_TEMP_REF;

    bat->state.charge_phase = BAT_CHARGE_PHASE_IDLE;
    bat->state.soc_percent = 50.0f;
    bat->state.soc_level = BAT_SOC_MEDIUM;
    bat->state.temperature = BAT_PB_TEMP_REF;

    bat->state.coulomb_ah = 0.0f;
    bat->state.last_soc_for_cycles = 50.0f;
    bat->state.cumulative_dod = 0.0f;
    bat->state.last_temperature = BAT_PB_TEMP_REF;
    bat->state.first_measurement = true;

    return BAT_OK;
}

static inline int bat_pb_compute_thresholds(bat_pb_t *bat)
{
    if (bat == NULL) return BAT_ERR_NULL_PTR;

    uint8_t n = bat->config.nb_elements;
    bat_pb_type_t t = bat->config.type;
    float temp = bat->state.temperature;
    float temp_ref = bat->config.temp_ref;

    bat->thresholds.V_cutoff_discharge = bat_pb_cell_to_total(BAT_CELL_CUTOFF_EMPTY, n);
    bat->thresholds.V_cutoff_charge = bat_pb_cell_to_total(BAT_CELL_CUTOFF_FULL_FLOAT, n);
    bat->thresholds.V_gas = bat_pb_cell_to_total(BAT_PB_V_GAS_PER_CELL, n);

    float V_bulk_cell = bat_pb_temp_compensate(BAT_CELL_BULK_THRESHOLD, temp, temp_ref);
    float V_abs_cell  = bat_pb_temp_compensate(BAT_CELL_ABSORPTION, temp, temp_ref);
    float V_float_cell = bat_pb_temp_compensate(BAT_CELL_FLOATING, temp, temp_ref);
    float V_boost_cell = bat_pb_temp_compensate(BAT_CELL_BOOST, temp, temp_ref);
    float V_equal_cell = bat_pb_temp_compensate(BAT_CELL_EQUALIZATION, temp, temp_ref);

    bat->thresholds.V_bulk = bat_pb_cell_to_total(V_bulk_cell, n);
    bat->thresholds.V_absorption = bat_pb_cell_to_total(V_abs_cell, n);
    bat->thresholds.V_floating = bat_pb_cell_to_total(V_float_cell, n);
    bat->thresholds.V_boost = bat_pb_cell_to_total(V_boost_cell, n);
    bat->thresholds.V_equalization = bat_pb_cell_to_total(V_equal_cell, n);

    bat->thresholds.V_ocv_empty = bat_pb_cell_to_total(BAT_CELL_OCV_EMPTY, n);
    bat->thresholds.V_ocv_full = bat_pb_cell_to_total(BAT_CELL_OCV_FULL, n);

    float Vfc_cell = bat_pb_temp_compensate(BAT_VFC_PER_CELL[t], temp, temp_ref);
    float Vrc_cell = bat_pb_temp_compensate(BAT_VRC_PER_CELL[t], temp, temp_ref);

    bat->thresholds.V_end_of_charge = bat_pb_cell_to_total(Vfc_cell, n);
    bat->thresholds.V_recharge = bat_pb_cell_to_total(Vrc_cell, n);

    bat->thresholds.V_delestage = bat_pb_get_disconnect_voltage(t, 50.0f, BAT_C_RATE_C20, n, temp, temp_ref);
    bat->thresholds.V_reconnect = bat_pb_get_reconnect_voltage(t, 50.0f, BAT_C_RATE_C20, n, temp, temp_ref);

    return BAT_OK;
}

static inline int bat_pb_compute_currents(bat_pb_t *bat)
{
    if (bat == NULL) return BAT_ERR_NULL_PTR;

    bat_pb_type_t t = bat->config.type;
    float C = bat->config.capacity_ah;

    bat->currents.I_charge_max = BAT_I_CHARGE_MAX_C[t] * C;
    bat->currents.I_bulk_target = bat->currents.I_charge_max;
    bat->currents.I_absorption_end = BAT_ABS_END_CURRENT_C * C;
    bat->currents.I_float = BAT_I_FLOAT_C[t] * C;

    if (BAT_VEG_PER_CELL[t] > 0.0f) {
        bat->currents.I_equalization = C / 20.0f;
    } else {
        bat->currents.I_equalization = 0.0f;
    }

    return BAT_OK;
}

/* ==========================================================================
 *  SECTION 11: MISE A JOUR ET ETAT
 * ========================================================================== */

static inline int bat_pb_update_state(bat_pb_t *bat, float V_meas, float I_meas,
                                       float temp_meas, float dt_seconds)
{
    if (bat == NULL) return BAT_ERR_NULL_PTR;
    if (dt_seconds <= 0.0f) return BAT_ERR_INVALID_DT;

    float C = bat->config.capacity_ah;
    float I_threshold_charge = BAT_CHARGE_CURRENT_THRESHOLD_C * C;
    float I_threshold_discharge = BAT_DISCHARGE_CURRENT_THRESHOLD_C * C;
    float I_threshold_ocv = BAT_OCV_CURRENT_THRESHOLD_C * C;

    bat->state.V_batt = V_meas;
    bat->state.I_batt = I_meas;
    bat->state.temperature = temp_meas;

    uint8_t n = bat->config.nb_elements;
    bat->state.V_per_cell = bat_pb_total_to_cell(V_meas, n);
    bat->state.V_per_cell_compensated = bat_pb_temp_compensate(bat->state.V_per_cell, temp_meas, bat->config.temp_ref);
    bat->state.V_batt_compensated = bat_pb_cell_to_total(bat->state.V_per_cell_compensated, n);

    /* Detection charge/decharge avec hysteresis */
    bat->state.is_charging = (I_meas > I_threshold_charge);
    bat->state.is_discharging = (I_meas < I_threshold_discharge);

    /* Estimation SOC depuis OCV si vrai repos (courant tres faible) */
    if (fabsf(I_meas) < I_threshold_ocv) {
        float soc_ocv = bat_pb_estimate_soc_from_ocv(bat->config.type, V_meas, n);
        if (soc_ocv >= 0.0f) {
            bat->state.soc_percent = soc_ocv;
        }
    }
    bat->state.dod_percent = 100.0f - bat->state.soc_percent;

    /* Niveau qualitatif SOC */
    if (bat->state.soc_percent >= BAT_SOC_FULL_THRESHOLD) {
        bat->state.soc_level = BAT_SOC_FULL;
    } else if (bat->state.soc_percent >= 80.0f) {
        bat->state.soc_level = BAT_SOC_HIGH;
    } else if (bat->state.soc_percent >= 50.0f) {
        bat->state.soc_level = BAT_SOC_MEDIUM;
    } else if (bat->state.soc_percent >= 20.0f) {
        bat->state.soc_level = BAT_SOC_LOW;
    } else if (bat->state.soc_percent >= BAT_SOC_CRITICAL_THRESHOLD) {
        bat->state.soc_level = BAT_SOC_VERY_LOW;
    } else {
        bat->state.soc_level = BAT_SOC_CRITICAL;
    }

    bat->state.is_critical = (bat->state.soc_percent < BAT_SOC_CRITICAL_THRESHOLD);

    /* Compteurs Ah */
    if (bat->state.is_charging) {
        bat->state.total_ah_charged += I_meas * dt_seconds / 3600.0f;
    } else if (bat->state.is_discharging) {
        bat->state.total_ah_discharged += (-I_meas) * dt_seconds / 3600.0f;
    }

    /* Timer de charge avec hysteresis */
    if (bat->state.is_charging) {
        bat->state.charge_time_hours += dt_seconds / 3600.0f;
        bat->state.charge_timer_hysteresis_s = 0.0f;
    } else {
        /* Accumule le temps hors charge */
        bat->state.charge_timer_hysteresis_s += dt_seconds;
        /* Reset seulement apres la periode d'hysteresis */
        if (bat->state.charge_timer_hysteresis_s >= BAT_CHARGE_TIMER_HYSTERESIS_S) {
            bat->state.charge_time_hours = 0.0f;
            bat->state.charge_timer_hysteresis_s = 0.0f;
        }
    }

    /* Timer equalization separe */
    if (bat->state.charge_phase == BAT_CHARGE_PHASE_EQUALIZATION && bat->state.is_charging) {
        bat->state.equalization_time_hours += dt_seconds / 3600.0f;
    } else if (bat->state.charge_phase != BAT_CHARGE_PHASE_EQUALIZATION) {
        bat->state.equalization_time_hours = 0.0f;
    }

    /* Synchronisation des flags avec la phase */
    bat->state.is_floating = (bat->state.charge_phase == BAT_CHARGE_PHASE_FLOATING);
    bat->state.is_equalizing = (bat->state.charge_phase == BAT_CHARGE_PHASE_EQUALIZATION);

    /* Recalcul des seuils si temperature changee significativement */
    if (bat->state.first_measurement ||
        fabsf(temp_meas - bat->state.last_temperature) > BAT_TEMP_RECALC_DELTA) {
        bat_pb_compute_thresholds(bat);
        bat->state.last_temperature = temp_meas;
        bat->state.first_measurement = false;
    }

    return BAT_OK;
}

/* ==========================================================================
 *  SECTION 12: MACHINE A ETATS
 * ========================================================================== */

static inline bat_charge_phase_t bat_pb_determine_charge_phase(const bat_pb_t *bat)
{
    if (bat == NULL || !bat->state.is_charging) {
        return BAT_CHARGE_PHASE_IDLE;
    }

    float V = bat->state.V_batt;
    float I = bat->state.I_batt;

    switch (bat->state.charge_phase) {
        case BAT_CHARGE_PHASE_IDLE:
            return BAT_CHARGE_PHASE_BULK;

        case BAT_CHARGE_PHASE_BULK:
            if (V >= bat->thresholds.V_bulk) {
                return BAT_CHARGE_PHASE_ABSORPTION;
            }
            return BAT_CHARGE_PHASE_BULK;

        case BAT_CHARGE_PHASE_ABSORPTION:
            if (I <= bat->currents.I_absorption_end &&
                bat->state.charge_time_hours >= BAT_MIN_ABSORPTION_TIME_H) {
                return BAT_CHARGE_PHASE_FLOATING;
            }
            if (bat->state.charge_time_hours >= BAT_MAX_ABSORPTION_TIME_H) {
                return BAT_CHARGE_PHASE_FLOATING;
            }
            return BAT_CHARGE_PHASE_ABSORPTION;

        case BAT_CHARGE_PHASE_FLOATING:
            if (bat_pb_is_equalization_allowed(bat)) {
                return BAT_CHARGE_PHASE_EQUALIZATION;
            }
            return BAT_CHARGE_PHASE_FLOATING;

        case BAT_CHARGE_PHASE_EQUALIZATION:
            if (bat->state.equalization_time_hours >= BAT_EQUALIZATION_DURATION_H[bat->config.type]) {
                return BAT_CHARGE_PHASE_FLOATING;
            }
            return BAT_CHARGE_PHASE_EQUALIZATION;

        default:
            return BAT_CHARGE_PHASE_IDLE;
    }
}

/**
 * @brief Applique la phase de charge determinee a l'etat de la batterie.
 * Met a jour charge_phase et synchronise les flags.
 *
 * @param bat Pointeur vers la structure batterie
 * @return Phase appliquee
 */
static inline bat_charge_phase_t bat_pb_apply_charge_phase(bat_pb_t *bat)
{
    if (bat == NULL) return BAT_CHARGE_PHASE_IDLE;

    bat_charge_phase_t new_phase = bat_pb_determine_charge_phase(bat);

    /* Reset du timer equalization si on sort de cette phase */
    if (bat->state.charge_phase == BAT_CHARGE_PHASE_EQUALIZATION &&
        new_phase != BAT_CHARGE_PHASE_EQUALIZATION) {
        bat->state.equalization_time_hours = 0.0f;
    }

    /* Reset du timer charge si on entre en bulk (nouveau cycle) */
    if (new_phase == BAT_CHARGE_PHASE_BULK &&
        bat->state.charge_phase != BAT_CHARGE_PHASE_BULK) {
        bat->state.charge_time_hours = 0.0f;
        bat->state.charge_timer_hysteresis_s = 0.0f;
    }

    bat->state.charge_phase = new_phase;

    /* Synchronisation des flags */
    bat->state.is_floating = (new_phase == BAT_CHARGE_PHASE_FLOATING);
    bat->state.is_equalizing = (new_phase == BAT_CHARGE_PHASE_EQUALIZATION);

    return new_phase;
}

/* ==========================================================================
 *  SECTION 13: COULOMB COUNTING
 * ========================================================================== */

static inline float bat_pb_update_coulomb_counting(bat_pb_t *bat, float I_meas,
                                                    float dt_seconds, bool update_ocv)
{
    if (bat == NULL || dt_seconds <= 0.0f) return bat->state.soc_percent;

    float C = bat->config.capacity_ah;
    float efficiency = BAT_PB_EFFICIENCY_THEORETICAL;

    /* Auto-decharge (appliquee meme sans courant mesure) */
    float self_discharge_ah = C * (BAT_PB_SELF_DISCHARGE_PER_MONTH / 100.0f) * (dt_seconds / (30.0f * 24.0f * 3600.0f));
    bat->state.coulomb_ah += self_discharge_ah;

    /* Integration du courant mesure */
    float dAh = I_meas * dt_seconds / 3600.0f;
    if (I_meas > 0) {
        dAh *= efficiency;
    }
    bat->state.coulomb_ah += dAh;

    /* Clamping */
    if (bat->state.coulomb_ah > C) bat->state.coulomb_ah = C;
    if (bat->state.coulomb_ah < -C) bat->state.coulomb_ah = -C;

    float soc_coulomb = 100.0f * (1.0f - bat->state.coulomb_ah / C);

    /* Reajustement OCV */
    float I_threshold_ocv = BAT_OCV_CURRENT_THRESHOLD_C * C;
    if (update_ocv && fabsf(I_meas) < I_threshold_ocv) {
        float soc_ocv = bat_pb_estimate_soc_from_ocv(bat->config.type,
                                                      bat->state.V_batt,
                                                      bat->config.nb_elements);
        if (soc_ocv >= 0.0f) {
            bat->state.soc_percent = soc_ocv * 0.95f + soc_coulomb * 0.05f;
            bat->state.coulomb_ah = C * (1.0f - bat->state.soc_percent / 100.0f);
        } else {
            bat->state.soc_percent = soc_coulomb;
        }
    } else {
        bat->state.soc_percent = soc_coulomb;
    }

    if (bat->state.soc_percent < 0.0f) bat->state.soc_percent = 0.0f;
    if (bat->state.soc_percent > 100.0f) bat->state.soc_percent = 100.0f;

    /* Comptage cycles IEC 61427 */
    float delta_dod = bat->state.last_soc_for_cycles - bat->state.soc_percent;
    if (delta_dod > 0) {
        bat->state.cumulative_dod += delta_dod;
        if (bat->state.cumulative_dod >= 80.0f) {
            bat->state.cycle_count++;
            bat->state.cumulative_dod -= 80.0f;
        }
    } else if (delta_dod < 0 && bat->state.soc_percent >= BAT_SOC_FULL_THRESHOLD) {
        bat->state.cumulative_dod = 0.0f;
    }
    bat->state.last_soc_for_cycles = bat->state.soc_percent;

    return bat->state.soc_percent;
}

/* ==========================================================================
 *  SECTION 14: FONCTIONS GLOBALES
 * ========================================================================== */

static inline int bat_pb_compute_all(bat_pb_t *bat)
{
    int ret = bat_pb_compute_thresholds(bat);
    if (ret != BAT_OK) return ret;
    return bat_pb_compute_currents(bat);
}

#ifdef __cplusplus
}
#endif

#endif /* _BAT_PB_H_ */