/**
 * @file main.c
 * @brief Exemple complet d'utilisation de batterie_Pb.h v1.1.0
 *        Demonstration des nouvelles fonctionnalites:
 *        - Constantes universelles par element (2V/6V/12V/24V)
 *        - Vision 2: BULK et ABSORPTION distincts
 *        - Compensation thermique sur toutes les tables
 *        - Comptage de Coulomb avec reajustement OCV
 *        - Comptage de cycles IEC 61427
 */

#include <stdio.h>
#include "batterie_Pb.h"

int main(void)
{
    printf("=== Exemple batterie_Pb.h v1.1.0 ===\n\n");

    /* ================================================================
     *  EXEMPLE 1: Initialisation et calculs automatiques
     * ================================================================ */
    printf("--- 1. Initialisation batterie AGM 12V 100Ah ---\n");

    bat_pb_t bat;
    bat_pb_init(&bat, BAT_TYPE_AGM, 6, 100.0f, BAT_REGULATOR_CC_CV);
    bat_pb_compute_all(&bat);

    printf("Type: %s\n", bat_pb_type_to_string(bat.config.type));
    printf("Elements: %d, Capacite: %.0f Ah\n", bat.config.nb_elements, bat.config.capacity_ah);
    printf("\n");

    /* ================================================================
     *  EXEMPLE 2: Seuils de tension calcules (Vision 2)
     * ================================================================ */
    printf("--- 2. Seuils de tension [V] - Vision 2 ---\n");
    printf("Coupure decharge:   %.2f V (%.3f V/el)\n",
           bat.thresholds.V_cutoff_discharge,
           bat_pb_total_to_cell(bat.thresholds.V_cutoff_discharge, bat.config.nb_elements));
    printf("BULK threshold:     %.2f V (%.3f V/el)  <-- Transition BULK -> ABS\n",
           bat.thresholds.V_bulk,
           bat_pb_total_to_cell(bat.thresholds.V_bulk, bat.config.nb_elements));
    printf("Absorption:         %.2f V (%.3f V/el)  <-- Tension maintenue en CV\n",
           bat.thresholds.V_absorption,
           bat_pb_total_to_cell(bat.thresholds.V_absorption, bat.config.nb_elements));
    printf("Floating:           %.2f V (%.3f V/el)\n",
           bat.thresholds.V_floating,
           bat_pb_total_to_cell(bat.thresholds.V_floating, bat.config.nb_elements));
    printf("OCV pleine:         %.2f V (%.3f V/el)\n",
           bat.thresholds.V_ocv_full,
           bat_pb_total_to_cell(bat.thresholds.V_ocv_full, bat.config.nb_elements));
    printf("\n");

    /* ================================================================
     *  EXEMPLE 3: Courants calcules
     * ================================================================ */
    printf("--- 3. Courants calcules [A] ---\n");
    printf("Charge max:      %.1f A (%.2fC)\n", bat.currents.I_charge_max,
           BAT_I_CHARGE_MAX_C[bat.config.type]);
    printf("Fin absorption:  %.2f A (%.3fC)\n", bat.currents.I_absorption_end,
           BAT_ABS_END_CURRENT_C);
    printf("Floating:        %.3f A (%.4fC)\n", bat.currents.I_float,
           BAT_I_FLOAT_C[bat.config.type]);
    printf("\n");

    /* ================================================================
     *  EXEMPLE 4: Interpolation - Delestage avec DOD quelconque
     *  + Compensation thermique
     * ================================================================ */
    printf("--- 4. Tension de delestage (interpolation + compensation) ---\n");
    printf("AGM 12V, C/20, T=25C:\n");

    float dod_values[] = {15.0f, 33.0f, 47.0f, 62.0f, 85.0f};
    for (int i = 0; i < 5; i++) {
        float V = bat_pb_get_disconnect_voltage(BAT_TYPE_AGM, dod_values[i],
                                                 BAT_C_RATE_C20, 6,
                                                 25.0f, 25.0f);
        printf("  DOD = %5.1f%% -> V_delestage = %.2f V\n", dod_values[i], V);
    }

    printf("\nAGM 12V, C/20, T=-10C (compensation):\n");
    for (int i = 0; i < 5; i++) {
        float V = bat_pb_get_disconnect_voltage(BAT_TYPE_AGM, dod_values[i],
                                                 BAT_C_RATE_C20, 6,
                                                 -10.0f, 25.0f);
        printf("  DOD = %5.1f%% -> V_delestage = %.2f V\n", dod_values[i], V);
    }
    printf("\n");

    /* ================================================================
     *  EXEMPLE 5: Interpolation - Reconnexion avec SOC quelconque
     *  + Compensation thermique
     * ================================================================ */
    printf("--- 5. Tension de reconnexion (interpolation + compensation) ---\n");
    printf("AGM 12V, C/20, T=25C:\n");

    float soc_values[] = {5.0f, 25.0f, 55.0f, 75.0f, 88.0f};
    for (int i = 0; i < 5; i++) {
        float V = bat_pb_get_reconnect_voltage(BAT_TYPE_AGM, soc_values[i],
                                                BAT_C_RATE_C20, 6,
                                                25.0f, 25.0f);
        printf("  SOC = %5.1f%% -> V_reconnexion = %.2f V\n", soc_values[i], V);
    }
    printf("\n");

    /* ================================================================
     *  EXEMPLE 6: Estimation SOC par OCV
     * ================================================================ */
    printf("--- 6. Estimation SOC par tension OCV ---\n");
    printf("AGM 12V (au repos):\n");

    float ocv_values[] = {10.5f, 11.4f, 12.0f, 12.3f, 12.9f};
    for (int i = 0; i < 5; i++) {
        float soc = bat_pb_estimate_soc_from_ocv(BAT_TYPE_AGM, ocv_values[i], 6);
        printf("  OCV = %.2f V -> SOC ~ %.1f%%\n", ocv_values[i], soc);
    }
    printf("\n");

    /* ================================================================
     *  EXEMPLE 7: Comparaison des 4 types de batteries
     * ================================================================ */
    printf("--- 7. Comparaison des 4 types (12V, C/20) ---\n");
    printf("%-20s %10s %10s %10s %10s\n", "Parametre", "SN-Pb", "CA-Pb", "AGM", "GEL");
    printf("%-20s %10s %10s %10s %10s\n", "---------", "-----", "-----", "---", "---");

    for (int t = 0; t < BAT_TYPE_COUNT; t++) {
        float V_abs = bat_pb_cell_to_total(BAT_VCH_PER_CELL[t], 6);
        float V_float = bat_pb_cell_to_total(BAT_VFL_PER_CELL[t], 6);
        float V_bulk = bat_pb_cell_to_total(BAT_CELL_BULK_THRESHOLD, 6);
        float V_boost = bat_pb_cell_to_total(BAT_VBOOST_PER_CELL[t], 6);

        if (t == 0) {
            printf("%-20s %10.2f %10.2f %10.2f %10.2f\n", "V absorption [V]",
                   V_abs, V_abs, V_abs, V_abs);
            printf("%-20s %10.2f %10.2f %10.2f %10.2f\n", "V floating [V]",
                   V_float, V_float, V_float, V_float);
            printf("%-20s %10.2f %10.2f %10.2f %10.2f\n", "V bulk threshold [V]",
                   V_bulk, V_bulk, V_bulk, V_bulk);
            printf("%-20s %10.2f %10.2f %10.2f %10.2f\n", "V boost [V]",
                   V_boost, V_boost, V_boost, V_boost);
        }
    }
    printf("\n");

    /* ================================================================
     *  EXEMPLE 8: Compensation thermique
     * ================================================================ */
    printf("--- 8. Compensation thermique ---\n");
    printf("V_absorption AGM a differentes temperatures:\n");

    float temps[] = {-10.0f, 0.0f, 10.0f, 25.0f, 40.0f};
    float V_ref = BAT_VCH_PER_CELL[BAT_TYPE_AGM];
    for (int i = 0; i < 5; i++) {
        float V_comp = bat_pb_temp_compensate(V_ref, temps[i], 25.0f);
        printf("  T = %5.1fC -> %.3f V/el -> %.2f V (12V)\n",
               temps[i], V_comp, bat_pb_cell_to_total(V_comp, 6));
    }
    printf("\n");

    /* ================================================================
     *  EXEMPLE 9: Simulation machine a etats - Vision 2
     *  BULK et ABSORPTION distincts
     * ================================================================ */
    printf("--- 9. Simulation machine a etats (Vision 2) ---\n");

    bat_pb_t bat_sim;
    bat_pb_init(&bat_sim, BAT_TYPE_CA_PB_LIQUIDE, 6, 100.0f, BAT_REGULATOR_CC_CV);
    bat_pb_compute_all(&bat_sim);

    /* Phase 1: BULK - tension monte, pas encore au seuil */
    printf("\n[Phase BULK]\n");
    bat_sim.state.charge_phase = BAT_CHARGE_PHASE_BULK;
    bat_pb_update_state(&bat_sim, 13.5f, 25.0f, 25.0f, 1.0f);
    printf("V=13.5V (%.3f V/el), I=25A -> Phase: %s\n",
           bat_pb_total_to_cell(13.5f, 6),
           bat_pb_phase_to_string(bat_pb_determine_charge_phase(&bat_sim)));
    printf("  -> V < V_bulk (%.2f V), reste en BULK\n", bat_sim.thresholds.V_bulk);

    /* Phase 1b: BULK - atteint le seuil de transition */
    bat_pb_update_state(&bat_sim, 13.9f, 25.0f, 25.0f, 1.0f);
    printf("V=13.9V (%.3f V/el), I=25A -> Phase: %s\n",
           bat_pb_total_to_cell(13.9f, 6),
           bat_pb_phase_to_string(bat_pb_determine_charge_phase(&bat_sim)));
    printf("  -> V >= V_bulk (%.2f V), passe en ABSORPTION\n", bat_sim.thresholds.V_bulk);

    /* Phase 2: ABSORPTION - tension maintenue, courant decroit */
    printf("\n[Phase ABSORPTION]\n");
    bat_sim.state.charge_phase = BAT_CHARGE_PHASE_ABSORPTION;
    bat_sim.state.charge_time_hours = 0.5f;
    bat_pb_update_state(&bat_sim, 14.4f, 5.0f, 25.0f, 1.0f);
    printf("V=14.4V, I=5A, t=%.1fh -> Phase: %s\n",
           bat_sim.state.charge_time_hours,
           bat_pb_phase_to_string(bat_pb_determine_charge_phase(&bat_sim)));
    printf("  -> I > I_end (%.2fA) ET t < %.1fh, reste en ABSORPTION\n",
           bat_sim.currents.I_absorption_end, BAT_MIN_ABSORPTION_TIME_H);

    /* Phase 2b: ABSORPTION - courant tombe, temps minimum ecoule */
    bat_sim.state.charge_time_hours = 1.5f;
    bat_pb_update_state(&bat_sim, 14.4f, 1.5f, 25.0f, 1.0f);
    printf("V=14.4V, I=1.5A, t=%.1fh -> Phase: %s\n",
           bat_sim.state.charge_time_hours,
           bat_pb_phase_to_string(bat_pb_determine_charge_phase(&bat_sim)));
    printf("  -> I <= I_end (%.2fA) ET t >= %.1fh, passe en FLOATING\n",
           bat_sim.currents.I_absorption_end, BAT_MIN_ABSORPTION_TIME_H);

    /* Phase 3: FLOATING */
    printf("\n[Phase FLOATING]\n");
    bat_sim.state.charge_phase = BAT_CHARGE_PHASE_FLOATING;
    bat_pb_update_state(&bat_sim, 13.8f, 0.3f, 25.0f, 1.0f);
    printf("V=13.8V, I=0.3A -> Phase: %s\n",
           bat_pb_phase_to_string(bat_pb_determine_charge_phase(&bat_sim)));
    printf("\n");

    /* ================================================================
     *  EXEMPLE 10: Equalization (liquide uniquement)
     * ================================================================ */
    printf("--- 10. Equalization ---\n");

    bat_pb_t bat_liq;
    bat_pb_init(&bat_liq, BAT_TYPE_CA_PB_LIQUIDE, 6, 100.0f, BAT_REGULATOR_CC_CV);
    bat_pb_compute_all(&bat_liq);
    bat_liq.state.soc_percent = 100.0f;
    bat_liq.state.days_since_equalization = 35;

    printf("Batterie Ca-Pb liquide:\n");
    printf("  Equalization autorisee: %s\n",
           bat_pb_is_equalization_allowed(&bat_liq) ? "OUI" : "NON");
    printf("  Tension d'equalization: %.2f V\n", bat_liq.thresholds.V_equalization);

    bat_pb_t bat_agm;
    bat_pb_init(&bat_agm, BAT_TYPE_AGM, 6, 100.0f, BAT_REGULATOR_CC_CV);
    bat_pb_compute_all(&bat_agm);
    bat_agm.state.soc_percent = 100.0f;
    bat_agm.state.days_since_equalization = 35;

    printf("Batterie AGM:\n");
    printf("  Equalization autorisee: %s\n",
           bat_pb_is_equalization_allowed(&bat_agm) ? "OUI" : "NON");
    printf("\n");

    /* ================================================================
     *  EXEMPLE 11: Comptage de Coulomb
     * ================================================================ */
    printf("--- 11. Comptage de Coulomb ---\n");

    bat_pb_t bat_coulomb;
    bat_pb_init(&bat_coulomb, BAT_TYPE_AGM, 6, 100.0f, BAT_REGULATOR_CC_CV);
    bat_pb_compute_all(&bat_coulomb);
    bat_coulomb.state.soc_percent = 50.0f;  /* Demi-charge */
    bat_coulomb.state.coulomb_ah = 50.0f;   /* 50 Ah decharges */

    printf("Initial: SOC = %.1f%%, Coulomb = %.1f Ah\n",
           bat_coulomb.state.soc_percent, bat_coulomb.state.coulomb_ah);

    /* Simulation: charge pendant 1 heure a 10A */
    printf("\nCharge pendant 1h @ 10A (dt=3600s):\n");
    bat_pb_update_coulomb_counting(&bat_coulomb, 10.0f, 3600.0f, false);
    printf("  -> SOC = %.1f%%, Coulomb = %.1f Ah\n",
           bat_coulomb.state.soc_percent, bat_coulomb.state.coulomb_ah);

    /* Simulation: decharge pendant 2 heures a 5A */
    printf("\nDecharge pendant 2h @ 5A (dt=7200s):\n");
    bat_pb_update_coulomb_counting(&bat_coulomb, -5.0f, 7200.0f, false);
    printf("  -> SOC = %.1f%%, Coulomb = %.1f Ah\n",
           bat_coulomb.state.soc_percent, bat_coulomb.state.coulomb_ah);
    printf("\n");

    /* ================================================================
     *  EXEMPLE 12: Comptage de cycles
     * ================================================================ */
    printf("--- 12. Comptage de cycles (IEC 61427) ---\n");

    bat_pb_t bat_cycle;
    bat_pb_init(&bat_cycle, BAT_TYPE_AGM, 6, 100.0f, BAT_REGULATOR_CC_CV);
    bat_pb_compute_all(&bat_cycle);
    bat_cycle.state.soc_percent = 100.0f;
    bat_cycle.state.last_soc_for_cycles = 100.0f;
    bat_cycle.state.cumulative_dod = 0.0f;
    bat_cycle.state.cycle_count = 0;

    printf("Initial: cycles = %lu, SOC = %.0f%%\n",
           (unsigned long)bat_cycle.state.cycle_count, bat_cycle.state.soc_percent);

    /* Decharge 30% */
    bat_cycle.state.soc_percent = 70.0f;
    bat_pb_update_coulomb_counting(&bat_cycle, -5.0f, 3600.0f, false);
    printf("Decharge a 70%%: cycles = %lu, DOD cumule = %.1f%%\n",
           (unsigned long)bat_cycle.state.cycle_count, bat_cycle.state.cumulative_dod);

    /* Decharge 30% supplementaire */
    bat_cycle.state.soc_percent = 40.0f;
    bat_pb_update_coulomb_counting(&bat_cycle, -5.0f, 3600.0f, false);
    printf("Decharge a 40%%: cycles = %lu, DOD cumule = %.1f%%\n",
           (unsigned long)bat_cycle.state.cycle_count, bat_cycle.state.cumulative_dod);

    /* Decharge 30% supplementaire -> depasse 80% DOD cumule */
    bat_cycle.state.soc_percent = 10.0f;
    bat_pb_update_coulomb_counting(&bat_cycle, -5.0f, 3600.0f, false);
    printf("Decharge a 10%%: cycles = %lu, DOD cumule = %.1f%%\n",
           (unsigned long)bat_cycle.state.cycle_count, bat_cycle.state.cumulative_dod);

    /* Recharge complete -> reset DOD cumule */
    bat_cycle.state.soc_percent = 100.0f;
    bat_pb_update_coulomb_counting(&bat_cycle, 10.0f, 3600.0f, false);
    printf("Recharge a 100%%: cycles = %lu, DOD cumule = %.1f%% (reset)\n",
           (unsigned long)bat_cycle.state.cycle_count, bat_cycle.state.cumulative_dod);
    printf("\n");

    /* ================================================================
     *  EXEMPLE 13: Batterie 24V (12 elements) - demonstration universalite
     * ================================================================ */
    printf("--- 13. Batterie 24V (12 elements) - Universalite ---\n");

    bat_pb_t bat_24v;
    bat_pb_init(&bat_24v, BAT_TYPE_GEL, 12, 200.0f, BAT_REGULATOR_CC_CV);
    bat_pb_compute_all(&bat_24v);

    printf("Batterie GEL 24V 200Ah:\n");
    printf("  V absorption: %.2f V (%.3f V/el x 12)\n",
           bat_24v.thresholds.V_absorption,
           bat_pb_total_to_cell(bat_24v.thresholds.V_absorption, 12));
    printf("  V floating:   %.2f V (%.3f V/el x 12)\n",
           bat_24v.thresholds.V_floating,
           bat_pb_total_to_cell(bat_24v.thresholds.V_floating, 12));
    printf("  V bulk:       %.2f V (%.3f V/el x 12)\n",
           bat_24v.thresholds.V_bulk,
           bat_pb_total_to_cell(bat_24v.thresholds.V_bulk, 12));
    printf("  I charge max: %.1f A\n", bat_24v.currents.I_charge_max);

    /* Delestage 50% DOD a -10C */
    float V_off_24v = bat_pb_get_disconnect_voltage(BAT_TYPE_GEL, 50.0f,
                                                    BAT_C_RATE_C20, 12,
                                                    -10.0f, 25.0f);
    printf("  Delestage 50%% DOD @ -10C: %.2f V\n", V_off_24v);
    printf("\n");

    /* ================================================================
     *  EXEMPLE 14: Batterie 2V (1 element) - demonstration universalite
     * ================================================================ */
    printf("--- 14. Batterie 2V (1 element) - Universalite ---\n");

    bat_pb_t bat_2v;
    bat_pb_init(&bat_2v, BAT_TYPE_SN_PB_LIQUIDE, 1, 1000.0f, BAT_REGULATOR_CC_CV);
    bat_pb_compute_all(&bat_2v);

    printf("Batterie SN-Pb 2V 1000Ah (stationnaire):\n");
    printf("  V absorption: %.3f V (1 element)\n", bat_2v.thresholds.V_absorption);
    printf("  V floating:   %.3f V (1 element)\n", bat_2v.thresholds.V_floating);
    printf("  V bulk:       %.3f V (1 element)\n", bat_2v.thresholds.V_bulk);
    printf("  I charge max: %.1f A\n", bat_2v.currents.I_charge_max);
    printf("\n");

    printf("=== Fin des exemples ===\n");
    return 0;
}
