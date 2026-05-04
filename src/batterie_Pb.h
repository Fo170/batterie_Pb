/**
 * @file batterie_Pb.h
 * @brief Bibliotheque complete de gestion des batteries au plomb-acide
 * @version 1.0.0 - Basé sur le livre "énergie solaire photovoltaïque" de Anne Labouret et Michel Villoz (p.89-136)
 *                et sources : https://www.batterie-solaire.com/accumulateur-plomb-principe-fonctionnement.htm
 *                             https://fr.wikipedia.org/wiki/Batterie_au_plomb
 * 
 * Toutes les fonctions sont inline ou statiques - fichier header unique.
 * Les tableaux de delestage/reconnexion proviennent du livre (batteries liquides).
 * Les valeurs AGM/GEL sont derivees des datasheets industrielles.
 */

#ifndef _BAT_PB_H_
#define _BAT_PB_H_

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 *  SECTION 1: CONSTANTES PHYSIQUES ET ELECTROCHIMIQUES
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

/** Seuil de courant pour detecter la charge [A] */
#define BAT_CHARGE_CURRENT_THRESHOLD    0.05f

/** Seuil de courant pour detecter la decharge [A] */
#define BAT_DISCHARGE_CURRENT_THRESHOLD (-0.05f)

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

/* ==========================================================================
 *  SECTION 2: SEUILS DE TENSION POUR BATTERIE 12V (6 elements)
 *  Valeurs de reference rapides pour batteries 12V standard.
 * ========================================================================== */

#define BAT_12V_CUTOFF_EMPTY            10.5f       /* 1.75 V/element */
#define BAT_12V_CUTOFF_FULL_FLOAT       13.8f       /* 2.30 V/element */
#define BAT_12V_OCV_EMPTY               10.6f       /* ~1.77 V/element */
#define BAT_12V_OCV_FULL                12.65f      /* ~2.11 V/element */
#define BAT_12V_BULK_THRESHOLD          14.1f       /* 2.35 V/element */
#define BAT_12V_ABSORPTION              14.1f       /* 2.35 V/element */
#define BAT_12V_FLOATING                13.8f       /* 2.30 V/element */
#define BAT_12V_GAS_THRESHOLD           14.04f      /* 2.34 V/element */
#define BAT_12V_BOOST                   15.0f       /* 2.50 V/element */

/* ==========================================================================
 *  SECTION 3: ENUMERATIONS
 * ========================================================================== */

/** Technologies de batteries au plomb supportees */
typedef enum {
    BAT_TYPE_SN_PB_LIQUIDE = 0,     /**< Plomb-antimoine, electrolyte liquide */
    BAT_TYPE_CA_PB_LIQUIDE = 1,     /**< Plomb-calcium, electrolyte liquide */
    BAT_TYPE_AGM           = 2,     /**< Absorbent Glass Mat */
    BAT_TYPE_GEL           = 3,     /**< Electrolyte en gel */
    BAT_TYPE_COUNT                  /**< Nombre total de types */
} bat_pb_type_t;

/** Etats de charge (State of Charge) approximatifs */
typedef enum {
    BAT_SOC_CRITICAL = 0,           /**< < 10% - danger permanent */
    BAT_SOC_VERY_LOW = 1,           /**< 10-20% - decharge profonde */
    BAT_SOC_LOW      = 2,           /**< 20-50% - decharge partielle */
    BAT_SOC_MEDIUM   = 3,           /**< 50-80% - etat normal */
    BAT_SOC_HIGH     = 4,           /**< 80-95% - bien chargee */
    BAT_SOC_FULL     = 5            /**< > 95% - pleine charge */
} bat_soc_level_t;

/** Phases de charge CC/CV */
typedef enum {
    BAT_CHARGE_PHASE_IDLE = 0,      /**< Aucune charge en cours */
    BAT_CHARGE_PHASE_BULK,          /**< Courant constant (CC) */
    BAT_CHARGE_PHASE_ABSORPTION,    /**< Tension constante (CV) */
    BAT_CHARGE_PHASE_FLOATING,      /**< Floating/entretien */
    BAT_CHARGE_PHASE_EQUALIZATION   /**< Equalisation (liquide uniquement) */
} bat_charge_phase_t;

/** Modes de regulation du chargeur */
typedef enum {
    BAT_REGULATOR_ON_OFF = 0,       /**< Regulation tout ou rien (simple) */
    BAT_REGULATOR_CC_CV  = 1,       /**< Regulation a tension constante */
    BAT_REGULATOR_MPPT   = 2        /**< MPPT */
} bat_regulator_mode_t;

/** Taux de decharge (C-rate) pour l'indexation des tables */
typedef enum {
    BAT_C_RATE_C100 = 0,            /**< Decharge sur 100h */
    BAT_C_RATE_C50  = 1,            /**< Decharge sur 50h */
    BAT_C_RATE_C20  = 2,            /**< Decharge sur 20h (standard) */
    BAT_C_RATE_C10  = 3,            /**< Decharge sur 10h */
    BAT_C_RATE_C5   = 4,            /**< Decharge sur 5h */
    BAT_C_RATE_COUNT                /**< Nombre de taux supportes */
} bat_c_rate_t;

/* ==========================================================================
 *  SECTION 4: CONSTANTES PAR TYPE [V/element @ 25C]
 *  Source: Labouret & Villoz, p.89-136, complete par datasheets
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
    /* AGM           */ 0.00f,      /* NON RECOMMANDE */
    /* GEL           */ 0.00f       /* NON RECOMMANDE */
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
    /* SN-Pb Liquide */ 2.55f,      /* 0.5 jour / 30 jours */
    /* CA-Pb Liquide */ 2.55f,      /* 0.5 jour / 30 jours */
    /* AGM           */ 0.00f,      /* Interdit */
    /* GEL           */ 0.00f       /* Interdit */
};

/** Courant de charge maximal recommande [fraction de C] */
static const float BAT_I_CHARGE_MAX_C[BAT_TYPE_COUNT] = {
    /* SN-Pb Liquide */ 0.25f,      /* C/4 */
    /* CA-Pb Liquide */ 0.25f,      /* C/4 */
    /* AGM           */ 0.30f,      /* C/3.3 */
    /* GEL           */ 0.20f       /* C/5 */
};

/** Courant de floating recommande [fraction de C] */
static const float BAT_I_FLOAT_C[BAT_TYPE_COUNT] = {
    /* SN-Pb Liquide */ 0.005f,     /* C/200 */
    /* CA-Pb Liquide */ 0.005f,     /* C/200 */
    /* AGM           */ 0.003f,     /* C/333 */
    /* GEL           */ 0.003f      /* C/333 */
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
 *  SECTION 5: TABLES DE TENSION DE DELESTAGE [V/element @ 25C]
 *  
 *  Source principale: Labouret & Villoz, p.89-136 (batteries liquides)
 *  Les valeurs pour AGM/GEL sont derivees avec ajustement +0.01 a +0.02V
 *  
 *  Index: profondeur de decharge 10%, 20%, 30%, 40%, 50%, 60%, 70%, 80%, 90%, 100%
 * ========================================================================== */

#define BAT_DOD_TABLE_SIZE  10

/** Tensions de delestage pour C/100 [V/element] - tres faible courant */
static const float BAT_VDL_C100_PER_CELL[BAT_TYPE_COUNT][BAT_DOD_TABLE_SIZE] = {
    /* SN-Pb Liquide - Source: Labouret & Villoz */
    {2.14f, 2.12f, 2.10f, 2.08f, 2.05f, 2.02f, 2.00f, 1.96f, 1.92f, 1.80f},
    /* CA-Pb Liquide - Source: Labouret & Villoz */
    {2.14f, 2.12f, 2.10f, 2.08f, 2.05f, 2.02f, 2.00f, 1.96f, 1.92f, 1.80f},
    /* AGM - Derivee */
    {2.15f, 2.13f, 2.11f, 2.09f, 2.06f, 2.03f, 2.01f, 1.97f, 1.93f, 1.82f},
    /* GEL - Derivee */
    {2.16f, 2.14f, 2.12f, 2.10f, 2.07f, 2.04f, 2.02f, 1.98f, 1.94f, 1.83f}
};

/** Tensions de delestage pour C/50 [V/element] */
static const float BAT_VDL_C50_PER_CELL[BAT_TYPE_COUNT][BAT_DOD_TABLE_SIZE] = {
    /* SN-Pb Liquide - Interpole entre C100 et C20 */
    {2.13f, 2.11f, 2.09f, 2.07f, 2.04f, 2.01f, 1.99f, 1.95f, 1.91f, 1.79f},
    /* CA-Pb Liquide */
    {2.13f, 2.11f, 2.09f, 2.07f, 2.04f, 2.01f, 1.99f, 1.95f, 1.91f, 1.79f},
    /* AGM */
    {2.14f, 2.12f, 2.10f, 2.08f, 2.05f, 2.02f, 2.00f, 1.96f, 1.92f, 1.81f},
    /* GEL */
    {2.15f, 2.13f, 2.11f, 2.09f, 2.06f, 2.03f, 2.01f, 1.97f, 1.93f, 1.82f}
};

/** Tensions de delestage pour C/20 [V/element] - taux standard */
static const float BAT_VDL_C20_PER_CELL[BAT_TYPE_COUNT][BAT_DOD_TABLE_SIZE] = {
    /* SN-Pb Liquide - Source: Labouret & Villoz */
    {2.11f, 2.09f, 2.07f, 2.05f, 2.03f, 2.00f, 1.98f, 1.95f, 1.91f, 1.80f},
    /* CA-Pb Liquide - Source: Labouret & Villoz */
    {2.11f, 2.09f, 2.07f, 2.05f, 2.03f, 2.00f, 1.98f, 1.95f, 1.91f, 1.80f},
    /* AGM - Derivee */
    {2.12f, 2.10f, 2.08f, 2.06f, 2.04f, 2.01f, 1.99f, 1.96f, 1.92f, 1.81f},
    /* GEL - Derivee */
    {2.13f, 2.11f, 2.09f, 2.07f, 2.05f, 2.02f, 2.00f, 1.97f, 1.93f, 1.82f}
};

/** Tensions de delestage pour C/10 [V/element] - fort courant */
static const float BAT_VDL_C10_PER_CELL[BAT_TYPE_COUNT][BAT_DOD_TABLE_SIZE] = {
    /* SN-Pb Liquide - Source: Labouret & Villoz */
    {2.08f, 2.07f, 2.05f, 2.04f, 2.01f, 1.99f, 1.96f, 1.93f, 1.89f, 1.80f},
    /* CA-Pb Liquide - Source: Labouret & Villoz */
    {2.08f, 2.07f, 2.05f, 2.04f, 2.01f, 1.99f, 1.96f, 1.93f, 1.89f, 1.80f},
    /* AGM - Derivee */
    {2.09f, 2.08f, 2.06f, 2.05f, 2.02f, 2.00f, 1.97f, 1.94f, 1.90f, 1.81f},
    /* GEL - Derivee */
    {2.10f, 2.09f, 2.07f, 2.06f, 2.03f, 2.01f, 1.98f, 1.95f, 1.91f, 1.82f}
};

/** Tensions de delestage pour C/5 [V/element] - tres fort courant */
static const float BAT_VDL_C5_PER_CELL[BAT_TYPE_COUNT][BAT_DOD_TABLE_SIZE] = {
    /* SN-Pb Liquide - Extrapole */
    {2.05f, 2.04f, 2.02f, 2.01f, 1.98f, 1.96f, 1.93f, 1.90f, 1.86f, 1.78f},
    /* CA-Pb Liquide */
    {2.05f, 2.04f, 2.02f, 2.01f, 1.98f, 1.96f, 1.93f, 1.90f, 1.86f, 1.78f},
    /* AGM */
    {2.06f, 2.05f, 2.03f, 2.02f, 1.99f, 1.97f, 1.94f, 1.91f, 1.87f, 1.79f},
    /* GEL */
    {2.07f, 2.06f, 2.04f, 2.03f, 2.00f, 1.98f, 1.95f, 1.92f, 1.88f, 1.80f}
};

/* ==========================================================================
 *  SECTION 6: TABLES DE TENSION DE REENCLENCHEMENT [V/element @ 25C]
 *  
 *  Source principale: Labouret & Villoz, p.89-136 (batteries liquides)
 *  
 *  Index: etat de charge 0%, 10%, 20%, 30%, 40%, 50%, 60%, 70%, 80%, 90%
 * ========================================================================== */

#define BAT_SOC_TABLE_SIZE  10

/** Tensions de reenclenchement pour C/10 [V/element] */
static const float BAT_VRL_C10_PER_CELL[BAT_TYPE_COUNT][BAT_SOC_TABLE_SIZE] = {
    /* SN-Pb Liquide - Source: Labouret & Villoz */
    {2.08f, 2.09f, 2.12f, 2.15f, 2.19f, 2.23f, 2.27f, 2.34f, 2.43f, 2.61f},
    /* CA-Pb Liquide - Source: Labouret & Villoz */
    {2.08f, 2.09f, 2.12f, 2.15f, 2.19f, 2.23f, 2.27f, 2.34f, 2.43f, 2.61f},
    /* AGM - Derivee */
    {2.09f, 2.10f, 2.13f, 2.16f, 2.20f, 2.24f, 2.28f, 2.35f, 2.44f, 2.62f},
    /* GEL - Derivee */
    {2.10f, 2.11f, 2.14f, 2.17f, 2.21f, 2.25f, 2.29f, 2.36f, 2.45f, 2.63f}
};

/** Tensions de reenclenchement pour C/20 [V/element] */
static const float BAT_VRL_C20_PER_CELL[BAT_TYPE_COUNT][BAT_SOC_TABLE_SIZE] = {
    /* SN-Pb Liquide - Source: Labouret & Villoz */
    {2.05f, 2.07f, 2.10f, 2.13f, 2.17f, 2.21f, 2.25f, 2.32f, 2.43f, 2.60f},
    /* CA-Pb Liquide - Source: Labouret & Villoz */
    {2.05f, 2.07f, 2.10f, 2.13f, 2.17f, 2.21f, 2.25f, 2.32f, 2.43f, 2.60f},
    /* AGM - Derivee */
    {2.06f, 2.08f, 2.11f, 2.14f, 2.18f, 2.22f, 2.26f, 2.33f, 2.44f, 2.61f},
    /* GEL - Derivee */
    {2.07f, 2.09f, 2.12f, 2.15f, 2.19f, 2.23f, 2.27f, 2.34f, 2.45f, 2.62f}
};

/** Tensions de reenclenchement pour C/50 [V/element] */
static const float BAT_VRL_C50_PER_CELL[BAT_TYPE_COUNT][BAT_SOC_TABLE_SIZE] = {
    /* SN-Pb Liquide - Source: Labouret & Villoz */
    {2.01f, 2.03f, 2.07f, 2.10f, 2.14f, 2.17f, 2.21f, 2.27f, 2.34f, 2.47f},
    /* CA-Pb Liquide - Source: Labouret & Villoz */
    {2.01f, 2.03f, 2.07f, 2.10f, 2.14f, 2.17f, 2.21f, 2.27f, 2.34f, 2.47f},
    /* AGM - Derivee */
    {2.02f, 2.04f, 2.08f, 2.11f, 2.15f, 2.18f, 2.22f, 2.28f, 2.35f, 2.48f},
    /* GEL - Derivee */
    {2.03f, 2.05f, 2.09f, 2.12f, 2.16f, 2.19f, 2.23f, 2.29f, 2.36f, 2.49f}
};

/** Tensions de reenclenchement pour C/100 [V/element] */
static const float BAT_VRL_C100_PER_CELL[BAT_TYPE_COUNT][BAT_SOC_TABLE_SIZE] = {
    /* SN-Pb Liquide - Source: Labouret & Villoz */
    {1.99f, 2.02f, 2.06f, 2.09f, 2.13f, 2.16f, 2.20f, 2.26f, 2.32f, 2.46f},
    /* CA-Pb Liquide - Source: Labouret & Villoz */
    {1.99f, 2.02f, 2.06f, 2.09f, 2.13f, 2.16f, 2.20f, 2.26f, 2.32f, 2.46f},
    /* AGM - Derivee */
    {2.00f, 2.03f, 2.07f, 2.10f, 2.14f, 2.17f, 2.21f, 2.27f, 2.33f, 2.47f},
    /* GEL - Derivee */
    {2.01f, 2.04f, 2.08f, 2.11f, 2.15f, 2.18f, 2.22f, 2.28f, 2.34f, 2.48f}
};

/* ==========================================================================
 *  SECTION 7: TABLE OCV -> SOC [V/element @ 25C]
 *  Tension au repos (Open Circuit Voltage) en fonction du SOC
 * ========================================================================== */

#define BAT_OCV_TABLE_SIZE  11

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
 *  SECTION 8: STRUCTURES DE DONNEES
 * ========================================================================== */

/** Configuration statique de la batterie */
typedef struct {
    uint8_t             nb_elements;        /**< Nombre d'elements en serie */
    bat_pb_type_t       type;               /**< Technologie */
    bat_regulator_mode_t regulator_mode;    /**< Mode de regulation */
    float               capacity_ah;        /**< Capacite nominale [Ah] @ C20 */
    float               temp_ref;           /**< Temperature de reference [C] */
} bat_pb_config_t;

/** Seuils de tension calcules pour la batterie [V] */
typedef struct {
    float V_cutoff_discharge;       /**< Coupure decharge (vide) [V] */
    float V_cutoff_charge;          /**< Coupure charge (floating) [V] */
    float V_gas;                    /**< Tension de gazage [V] */
    float V_bulk;                   /**< Transition bulk -> absorption [V] */
    float V_absorption;             /**< Tension d'absorption [V] */
    float V_floating;               /**< Tension de floating [V] */
    float V_boost;                  /**< Tension de boost [V] */
    float V_equalization;           /**< Tension d'equalization [V] */
    float V_ocv_empty;              /**< OCV batterie vide [V] */
    float V_ocv_full;               /**< OCV batterie pleine [V] */
    float V_end_of_charge;          /**< Fin de charge (coupure haute) [V] */
    float V_recharge;               /**< Recharge (reconnexion) [V] */
    float V_delestage;              /**< Delestage [V] */
    float V_reconnect;              /**< Reconnexion [V] */
} bat_pb_thresholds_t;

/** Courants calcules pour la batterie [A] */
typedef struct {
    float I_charge_max;             /**< Courant de charge max [A] */
    float I_bulk_target;            /**< Courant cible bulk [A] */
    float I_absorption_end;         /**< Courant fin absorption [A] */
    float I_float;                  /**< Courant de floating [A] */
    float I_equalization;           /**< Courant d'equalization [A] */
} bat_pb_currents_t;

/** Etat dynamique de la batterie */
typedef struct {
    float V_batt;                   /**< Tension mesuree [V] */
    float I_batt;                   /**< Courant mesure [A] (>0 charge) */
    float temperature;              /**< Temperature [C] */
    float V_batt_compensated;       /**< Tension compensee [V] */
    float V_per_cell;               /**< Tension par element [V/el] */
    float V_per_cell_compensated;   /**< Tension par el compensee [V/el] */
    float soc_percent;              /**< SOC [%] */
    float dod_percent;              /**< DOD [%] */
    bat_soc_level_t soc_level;      /**< Niveau qualitatif SOC */
    bat_charge_phase_t charge_phase;/**< Phase de charge */
    float charge_time_hours;        /**< Duree phase charge [h] */
    bool is_charging;               /**< En charge */
    bool is_discharging;            /**< En decharge */
    bool is_floating;               /**< En floating */
    bool is_equalizing;             /**< En equalization */
    bool is_critical;               /**< Etat critique */
    bool needs_equalization;        /**< Equalisation necessaire */
    uint32_t cycle_count;           /**< Nombre de cycles */
    float total_ah_charged;         /**< Ah charges cumules */
    float total_ah_discharged;      /**< Ah decharges cumules */
    uint32_t days_since_equalization; /**< Jours depuis equalization */
    /* CHAMPS POUR COULOMB COUNTING */
    float coulomb_ah;               /**< Intégrale de Coulomb (Ah) */
    float last_soc_for_cycles;      /**< Dernier SOC pour comptage cycles */
    float cumulative_dod;           /**< DOD cumulé pour cycles */
    float last_temperature;         /**< Dernière température pour recalibrage */

    bool first_measurement;         /**< Première mesure de température (pour recalibrage) */

} bat_pb_state_t;

/** Structure principale */
typedef struct {
    bat_pb_config_t     config;
    bat_pb_thresholds_t thresholds;
    bat_pb_currents_t   currents;
    bat_pb_state_t      state;
} bat_pb_t;

/* ==========================================================================
 *  SECTION 9: FONCTIONS UTILITAIRES (INLINE)
 * ========================================================================== */

/**
 * @brief Applique la compensation thermique a une tension par element
 * @param V_per_cell Tension par element a compenser [V/element]
 * @param temp Temperature actuelle [C]
 * @param temp_ref Temperature de reference [C]
 * @return Tension compensee [V/element]
 */
static inline float bat_pb_temp_compensate(float V_per_cell, float temp, float temp_ref) {
    return V_per_cell + BAT_PB_TEMP_COEFF_PER_CELL * (temp - temp_ref);
}

/**
 * @brief Convertit une tension par element en tension totale
 */
static inline float bat_pb_cell_to_total(float V_per_cell, uint8_t nb_cells) {
    return V_per_cell * (float)nb_cells;
}

/**
 * @brief Convertit une tension totale en tension par element
 */
static inline float bat_pb_total_to_cell(float V_total, uint8_t nb_cells) {
    return (nb_cells > 0) ? (V_total / (float)nb_cells) : 0.0f;
}

/**
 * @brief Interpolation lineaire entre deux points
 * @param x Valeur d'entree
 * @param x0, x1 Bornes de l'intervalle
 * @param y0, y1 Valeurs correspondantes
 * @return Valeur interpolee
 */
static inline float bat_pb_interpolate(float x, float x0, float x1, float y0, float y1) {
    if (x <= x0) return y0;
    if (x >= x1) return y1;
    return y0 + (y1 - y0) * (x - x0) / (x1 - x0);
}

/**
 * @brief Retourne le nom du type de batterie
 */
static inline const char* bat_pb_type_to_string(bat_pb_type_t type) {
    switch (type) {
        case BAT_TYPE_SN_PB_LIQUIDE: return "Plomb-Antimoine (Liquide)";
        case BAT_TYPE_CA_PB_LIQUIDE: return "Plomb-Calcium (Liquide)";
        case BAT_TYPE_AGM:           return "AGM";
        case BAT_TYPE_GEL:           return "GEL";
        default:                     return "Inconnu";
    }
}

/**
 * @brief Retourne le nom de la phase de charge
 */
static inline const char* bat_pb_phase_to_string(bat_charge_phase_t phase) {
    switch (phase) {
        case BAT_CHARGE_PHASE_IDLE:         return "Inactif";
        case BAT_CHARGE_PHASE_BULK:         return "Bulk (CC)";
        case BAT_CHARGE_PHASE_ABSORPTION:   return "Absorption (CV)";
        case BAT_CHARGE_PHASE_FLOATING:     return "Floating";
        case BAT_CHARGE_PHASE_EQUALIZATION: return "Equalization";
        default:                            return "Inconnu";
    }
}

/**
 * @brief Retourne le nom du taux de decharge
 */
static inline const char* bat_pb_crate_to_string(bat_c_rate_t rate) {
    switch (rate) {
        case BAT_C_RATE_C100: return "C/100";
        case BAT_C_RATE_C50:  return "C/50";
        case BAT_C_RATE_C20:  return "C/20";
        case BAT_C_RATE_C10:  return "C/10";
        case BAT_C_RATE_C5:   return "C/5";
        default:              return "Inconnu";
    }
}

/* ==========================================================================
 *  SECTION 10: FONCTIONS D'ACCES AUX TABLEAUX (AVEC INTERPOLATION)
 * ========================================================================== */

/**
 * @brief Recupere la tension de delestage pour un DOD et un taux donnes
 * avec interpolation lineaire entre les points du tableau.
 * 
 * @param type      Type de batterie
 * @param dod_pct   Profondeur de decharge [%] (0-100)
 * @param rate      Taux de decharge
 * @param nb_cells  Nombre d'elements en serie
 * @return          Tension de delestage [V], 0.0f si erreur
 */
static inline float bat_pb_get_disconnect_voltage(bat_pb_type_t type, float dod_pct, 
                                                   bat_c_rate_t rate, uint8_t nb_cells)
{
    if (type >= BAT_TYPE_COUNT || dod_pct < 0.0f || dod_pct > 100.0f || nb_cells == 0) {
        return 0.0f;
    }

    const float (*table)[BAT_DOD_TABLE_SIZE];
    switch (rate) {
        case BAT_C_RATE_C100: table = BAT_VDL_C100_PER_CELL; break;
        case BAT_C_RATE_C50:  table = BAT_VDL_C50_PER_CELL;  break;
        case BAT_C_RATE_C20:  table = BAT_VDL_C20_PER_CELL;  break;
        case BAT_C_RATE_C10:  table = BAT_VDL_C10_PER_CELL;  break;
        case BAT_C_RATE_C5:   table = BAT_VDL_C5_PER_CELL;   break;
        default: return 0.0f;
    }

    /* Index dans la table DOD (10%, 20%, ..., 100%) */
    float idx_f = (dod_pct / 10.0f) - 1.0f;
    int idx_low = (int)idx_f;
    int idx_high = idx_low + 1;

    if (idx_low < 0) {
        return bat_pb_cell_to_total(table[type][0], nb_cells);
    }
    if (idx_high >= BAT_DOD_TABLE_SIZE) {
        return bat_pb_cell_to_total(table[type][BAT_DOD_TABLE_SIZE - 1], nb_cells);
    }

    /* Interpolation lineaire */
    float frac = idx_f - (float)idx_low;
    float V_cell = table[type][idx_low] + frac * (table[type][idx_high] - table[type][idx_low]);
    return bat_pb_cell_to_total(V_cell, nb_cells);
}

/**
 * @brief Recupere la tension de reconnexion pour un SOC et un taux donnes
 * avec interpolation lineaire entre les points du tableau.
 * 
 * @param type      Type de batterie
 * @param soc_pct   Etat de charge [%] (0-100)
 * @param rate      Taux de decharge
 * @param nb_cells  Nombre d'elements en serie
 * @return          Tension de reconnexion [V], 0.0f si erreur
 */
static inline float bat_pb_get_reconnect_voltage(bat_pb_type_t type, float soc_pct,
                                                  bat_c_rate_t rate, uint8_t nb_cells)
{
    if (type >= BAT_TYPE_COUNT || soc_pct < 0.0f || soc_pct > 100.0f || nb_cells == 0) {
        return 0.0f;
    }

    const float (*table)[BAT_SOC_TABLE_SIZE];
    switch (rate) {
        case BAT_C_RATE_C100: table = BAT_VRL_C100_PER_CELL; break;
        case BAT_C_RATE_C50:  table = BAT_VRL_C50_PER_CELL;  break;
        case BAT_C_RATE_C20:  table = BAT_VRL_C20_PER_CELL;  break;
        case BAT_C_RATE_C10:  table = BAT_VRL_C10_PER_CELL;  break;
        default: return 0.0f;  /* C5 non defini pour reconnexion */
    }

    /* Index dans la table SOC (0%, 10%, ..., 90%) */
    float idx_f = soc_pct / 10.0f;
    int idx_low = (int)idx_f;
    int idx_high = idx_low + 1;

    if (idx_low < 0) {
        return bat_pb_cell_to_total(table[type][0], nb_cells);
    }
    if (idx_high >= BAT_SOC_TABLE_SIZE) {
        return bat_pb_cell_to_total(table[type][BAT_SOC_TABLE_SIZE - 1], nb_cells);
    }

    /* Interpolation lineaire */
    float frac = idx_f - (float)idx_low;
    float V_cell = table[type][idx_low] + frac * (table[type][idx_high] - table[type][idx_low]);
    return bat_pb_cell_to_total(V_cell, nb_cells);
}

/**
 * @brief Estime le SOC a partir de la tension OCV (batterie au repos)
 * avec interpolation lineaire dans la table OCV.
 * 
 * @param type      Type de batterie
 * @param V_ocv     Tension OCV mesuree [V]
 * @param nb_cells  Nombre d'elements en serie
 * @return          SOC estime [%] (0-100), -1.0f si erreur
 */
static inline float bat_pb_estimate_soc_from_ocv(bat_pb_type_t type, float V_ocv, uint8_t nb_cells)
{
    if (type >= BAT_TYPE_COUNT || nb_cells == 0) {
        return -1.0f;
    }

    float V_cell = bat_pb_total_to_cell(V_ocv, nb_cells);
    const float *ocv_table = BAT_OCV_PER_CELL[type];

    /* Hors limites */
    if (V_cell <= ocv_table[0]) {
        return 0.0f;
    }
    if (V_cell >= ocv_table[BAT_OCV_TABLE_SIZE - 1]) {
        return 100.0f;
    }

    /* Recherche et interpolation */
    for (int i = 0; i < BAT_OCV_TABLE_SIZE - 1; i++) {
        if (V_cell >= ocv_table[i] && V_cell <= ocv_table[i + 1]) {
            return bat_pb_interpolate(V_cell, ocv_table[i], ocv_table[i + 1],
                                       BAT_OCV_SOC_TABLE[i], BAT_OCV_SOC_TABLE[i + 1]);
        }
    }
    return -1.0f;
}

/**
 * @brief Verifie si l'equalisation est autorisee pour ce type de batterie
 * 
 * @param bat   Pointeur vers la structure batterie
 * @return      true si l'equalisation est autorisee et necessaire
 */
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
 *  SECTION 11: FONCTIONS D'INITIALISATION ET DE CALCUL
 * ========================================================================== */

/**
 * @brief Initialise la structure batterie avec les parametres par defaut
 * 
 * @param bat       Pointeur vers la structure a initialiser
 * @param type      Type de batterie
 * @param nb_cells  Nombre d'elements en serie
 * @param capacity  Capacite nominale [Ah]
 * @param mode      Mode de regulation
 * @return          0 si succes, -1 si erreur (parametres invalides)
 */
static inline int bat_pb_init(bat_pb_t *bat, bat_pb_type_t type, uint8_t nb_cells,
                               float capacity, bat_regulator_mode_t mode)
{
    if (bat == NULL || type >= BAT_TYPE_COUNT || nb_cells == 0 || capacity <= 0.0f) {
        return -1;
    }

    memset(bat, 0, sizeof(bat_pb_t));

    bat->config.nb_elements = nb_cells;
    bat->config.type = type;
    bat->config.regulator_mode = mode;
    bat->config.capacity_ah = capacity;
    bat->config.temp_ref = BAT_PB_TEMP_REF;

    /* Etat initial */
    bat->state.charge_phase = BAT_CHARGE_PHASE_IDLE;
    bat->state.soc_percent = 50.0f;
    bat->state.soc_level = BAT_SOC_MEDIUM;
    bat->state.temperature = BAT_PB_TEMP_REF;

    bat->state.coulomb_ah = 0.0f;
    bat->state.last_soc_for_cycles = 50.0f;
    bat->state.cumulative_dod = 0.0f;
    bat->state.last_temperature = BAT_PB_TEMP_REF;
    bat->state.first_measurement = true;

    return 0;
}

/**
 * @brief Calcule les seuils de tension pour la batterie
 * Applique la compensation thermique si la temperature est differente
 * de la temperature de reference.
 * 
 * @param bat   Pointeur vers la structure batterie
 * @return      0 si succes
 */
static inline int bat_pb_compute_thresholds(bat_pb_t *bat)
{
    if (bat == NULL) return -1;

    uint8_t n = bat->config.nb_elements;
    bat_pb_type_t t = bat->config.type;
    float temp = bat->state.temperature;
    float temp_ref = bat->config.temp_ref;

    /* Seuils de securite */
    bat->thresholds.V_cutoff_discharge = bat_pb_cell_to_total(1.75f, n);
    bat->thresholds.V_cutoff_charge = bat_pb_cell_to_total(2.30f, n);
    bat->thresholds.V_gas = bat_pb_cell_to_total(BAT_PB_V_GAS_PER_CELL, n);

    /* Seuils CC/CV avec compensation thermique */
    float V_bulk_cell  = bat_pb_temp_compensate(BAT_VCH_PER_CELL[t], temp, temp_ref);
    float V_abs_cell   = bat_pb_temp_compensate(BAT_VCH_PER_CELL[t], temp, temp_ref);
    float V_float_cell = bat_pb_temp_compensate(BAT_VFL_PER_CELL[t], temp, temp_ref);
    float V_boost_cell = bat_pb_temp_compensate(BAT_VBOOST_PER_CELL[t], temp, temp_ref);
    float V_equal_cell = bat_pb_temp_compensate(BAT_VEG_PER_CELL[t], temp, temp_ref);

    bat->thresholds.V_bulk = bat_pb_cell_to_total(V_bulk_cell, n);
    bat->thresholds.V_absorption = bat_pb_cell_to_total(V_abs_cell, n);
    bat->thresholds.V_floating = bat_pb_cell_to_total(V_float_cell, n);
    bat->thresholds.V_boost = bat_pb_cell_to_total(V_boost_cell, n);
    bat->thresholds.V_equalization = bat_pb_cell_to_total(V_equal_cell, n);

    /* Seuils OCV */
    bat->thresholds.V_ocv_empty = bat_pb_cell_to_total(1.75f, n);
    bat->thresholds.V_ocv_full = bat_pb_cell_to_total(2.15f, n);

    /* Seuils mode tout ou rien avec compensation */
    float Vfc_cell = bat_pb_temp_compensate(BAT_VFC_PER_CELL[t], temp, temp_ref);
    float Vrc_cell = bat_pb_temp_compensate(BAT_VRC_PER_CELL[t], temp, temp_ref);

    bat->thresholds.V_end_of_charge = bat_pb_cell_to_total(Vfc_cell, n);
    bat->thresholds.V_recharge = bat_pb_cell_to_total(Vrc_cell, n);

    /* Delestage et reconnexion par defaut a 50% avec interpolation */
    bat->thresholds.V_delestage = bat_pb_get_disconnect_voltage(t, 50.0f, BAT_C_RATE_C20, n);
    bat->thresholds.V_reconnect = bat_pb_get_reconnect_voltage(t, 50.0f, BAT_C_RATE_C20, n);

    return 0;
}

/**
 * @brief Calcule les courants de charge pour la batterie
 * @param bat   Pointeur vers la structure batterie
 * @return      0 si succes
 */
static inline int bat_pb_compute_currents(bat_pb_t *bat)
{
    if (bat == NULL) return -1;

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

    return 0;
}

/**
 * @brief Met a jour l'etat de la batterie avec les nouvelles mesures
 * 
 * @param bat           Pointeur vers la structure batterie
 * @param V_meas        Tension mesuree [V]
 * @param I_meas        Courant mesure [A] (>0 charge, <0 decharge)
 * @param temp_meas     Temperature mesuree [C]
 * @return              0 si succes
 */
static inline int bat_pb_update_state(bat_pb_t *bat, float V_meas, float I_meas, float temp_meas)
{
    if (bat == NULL) return -1;

    bat->state.V_batt = V_meas;
    bat->state.I_batt = I_meas;
    bat->state.temperature = temp_meas;

    uint8_t n = bat->config.nb_elements;
    bat->state.V_per_cell = bat_pb_total_to_cell(V_meas, n);
    bat->state.V_per_cell_compensated = bat_pb_temp_compensate(bat->state.V_per_cell, temp_meas, bat->config.temp_ref);
    bat->state.V_batt_compensated = bat_pb_cell_to_total(bat->state.V_per_cell_compensated, n);

    /* Detection charge/decharge */
    bat->state.is_charging = (I_meas > BAT_CHARGE_CURRENT_THRESHOLD);
    bat->state.is_discharging = (I_meas < BAT_DISCHARGE_CURRENT_THRESHOLD);
    bat->state.is_floating = false;
    bat->state.is_equalizing = false;

    /* Estimation SOC depuis OCV si repos (courant faible) */
    if (fabsf(I_meas) < BAT_CHARGE_CURRENT_THRESHOLD) {
        bat->state.soc_percent = bat_pb_estimate_soc_from_ocv(
            bat->config.type, V_meas, n);
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

    /* Compteurs */
    if (bat->state.is_charging) {
        bat->state.total_ah_charged += I_meas / 3600.0f;
    } else if (bat->state.is_discharging) {
        bat->state.total_ah_discharged += (-I_meas) / 3600.0f;
    }

    /* Recalcul des seuils si temperature changee significativement */
    if (bat->state.first_measurement || fabsf(temp_meas - bat->state.last_temperature) > 2.0f) {
       bat_pb_compute_thresholds(bat);
       bat->state.last_temperature = temp_meas;
       bat->state.first_measurement = false;  // Plus besoin après
    }

    return 0;
}

/**
 * @brief Determine la phase de charge suivante en fonction de l'etat actuel
 * Machine a etats pour la charge CC/CV/Float/Equalization.
 * 
 * @param bat   Pointeur vers la structure batterie
 * @return      Phase de charge recommandee
 */
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
            if (V >= bat->thresholds.V_absorption) {
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
            if (bat->state.charge_time_hours >= BAT_EQUALIZATION_DURATION_H[bat->config.type]) {
                return BAT_CHARGE_PHASE_FLOATING;
            }
            return BAT_CHARGE_PHASE_EQUALIZATION;

        default:
            return BAT_CHARGE_PHASE_IDLE;
    }
}

/**
 * @brief Met à jour le SOC par comptage de Coulomb (intégration courant)
 * À appeler régulièrement (toutes les secondes ou moins) pour précision
 * 
 * @param bat           Pointeur vers la structure batterie
 * @param I_meas        Courant mesuré [A] (>0 charge, <0 décharge)
 * @param dt_seconds    Temps écoulé depuis dernier appel [s]
 * @param update_ocv    Si true, réajuste périodiquement sur l'OCV
 * @return              SOC estimé par Coulomb [%]
 */
static inline float bat_pb_update_coulomb_counting(bat_pb_t *bat, float I_meas, 
                                                     float dt_seconds, bool update_ocv)
{
    if (bat == NULL || dt_seconds <= 0.0f) return bat->state.soc_percent;
    
    float C = bat->config.capacity_ah;
    float efficiency = BAT_PB_EFFICIENCY_THEORETICAL;
    
    /* Intégration du courant (Ah) - utilise le champ de la structure */
    float dAh = I_meas * dt_seconds / 3600.0f;
    
    /* Rendement différent selon charge/décharge */
    if (I_meas > 0) {
        dAh *= efficiency;  /* Charge: rendement < 1 */
    }
    
    bat->state.coulomb_ah += dAh;
    
    /* SOC par Coulomb */
    float soc_coulomb = 100.0f * (1.0f - bat->state.coulomb_ah / C);
    
    /* Réajustement périodique sur OCV (repos, pas de courant) */
    if (update_ocv && fabsf(I_meas) < BAT_CHARGE_CURRENT_THRESHOLD) {
        float soc_ocv = bat_pb_estimate_soc_from_ocv(bat->config.type, 
                                                      bat->state.V_batt, 
                                                      bat->config.nb_elements);
        if (soc_ocv >= 0) {
            /* Filtre complémentaire: 95% OCV, 5% Coulomb pour stabilité */
            bat->state.soc_percent = soc_ocv * 0.95f + soc_coulomb * 0.05f;
            bat->state.coulomb_ah = C * (1.0f - bat->state.soc_percent / 100.0f);
        } else {
            bat->state.soc_percent = soc_coulomb;
        }
    } else {
        bat->state.soc_percent = soc_coulomb;
    }
    
    /* Bornes */
    if (bat->state.soc_percent < 0.0f) bat->state.soc_percent = 0.0f;
    if (bat->state.soc_percent > 100.0f) bat->state.soc_percent = 100.0f;
    
    /* Comptage des cycles - utilise les champs de la structure */
    float delta_dod = bat->state.last_soc_for_cycles - bat->state.soc_percent;
    if (delta_dod > 0) {
        bat->state.cumulative_dod += delta_dod;
        if (bat->state.cumulative_dod >= 80.0f) {  /* Cycle = 80% DOD cumulé */
            bat->state.cycle_count++;
            bat->state.cumulative_dod = 0.0f;
        }
    }
    bat->state.last_soc_for_cycles = bat->state.soc_percent;
    
    return bat->state.soc_percent;
}

/**
 * @brief Calcule tous les parametres de la batterie (seuils + courants)
 * A appeler apres bat_pb_init() ou apres changement de temperature.
 * 
 * @param bat   Pointeur vers la structure batterie
 * @return      0 si succes
 */
static inline int bat_pb_compute_all(bat_pb_t *bat)
{
    int ret = bat_pb_compute_thresholds(bat);
    if (ret != 0) return ret;
    return bat_pb_compute_currents(bat);
}

#ifdef __cplusplus
}
#endif

#endif /* _BAT_PB_H_ */
