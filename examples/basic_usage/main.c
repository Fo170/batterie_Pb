/**
 * @file main_example.c
 * @brief Exemple d'utilisation de batterie_Pb.h (fichier unique)
 */

#include <stdio.h>
#include "batterie_Pb.h"

int main(void)
{
    printf("=== Exemple batterie_Pb.h v3.0 ===\n\n");

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
     *  EXEMPLE 2: Seuils de tension calcules
     * ================================================================ */
    printf("--- 2. Seuils de tension [V] ---\n");
    printf("Coupure decharge:   %.2f V (%.3f V/el)\n", 
           bat.thresholds.V_cutoff_discharge, 
           bat_pb_total_to_cell(bat.thresholds.V_cutoff_discharge, bat.config.nb_elements));
    printf("Bulk -> Absorption: %.2f V (%.3f V/el)\n", 
           bat.thresholds.V_bulk, 
           bat_pb_total_to_cell(bat.thresholds.V_bulk, bat.config.nb_elements));
    printf("Absorption:         %.2f V (%.3f V/el)\n", 
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
     * ================================================================ */
    printf("--- 4. Tension de delestage (interpolation) ---\n");
    printf("AGM 12V, C/20:\n");

    float dod_values[] = {15.0f, 33.0f, 47.0f, 62.0f, 85.0f};
    for (int i = 0; i < 5; i++) {
        float V = bat_pb_get_disconnect_voltage(BAT_TYPE_AGM, dod_values[i], 
                                                 BAT_C_RATE_C20, 6);
        printf("  DOD = %5.1f%% -> V_delestage = %.2f V\n", dod_values[i], V);
    }
    printf("\n");

    /* ================================================================
     *  EXEMPLE 5: Interpolation - Reconnexion avec SOC quelconque
     * ================================================================ */
    printf("--- 5. Tension de reconnexion (interpolation) ---\n");
    printf("AGM 12V, C/20:\n");

    float soc_values[] = {5.0f, 25.0f, 55.0f, 75.0f, 88.0f};
    for (int i = 0; i < 5; i++) {
        float V = bat_pb_get_reconnect_voltage(BAT_TYPE_AGM, soc_values[i], 
                                                BAT_C_RATE_C20, 6);
        printf("  SOC = %5.1f%% -> V_reconnexion = %.2f V\n", soc_values[i], V);
    }
    printf("\n");

    /* ================================================================
     *  EXEMPLE 6: Estimation SOC par OCV
     * ================================================================ */
    printf("--- 6. Estimation SOC par tension OCV ---\n");
    printf("AGM 12V (au repos):\n");

    float ocv_values[] = {10.6f, 11.5f, 12.0f, 12.3f, 12.65f};
    for (int i = 0; i < 5; i++) {
        float soc = bat_pb_estimate_soc_from_ocv(BAT_TYPE_AGM, ocv_values[i], 6);
        printf("  OCV = %.2f V -> SOC ~ %.1f%%\n", ocv_values[i], soc);
    }
    printf("\n");

    /* ================================================================
     *  EXEMPLE 7: Comparaison des 4 types de batteries
     * ================================================================ */
    printf("--- 7. Comparaison des 4 types (12V, C/20) ---\n");
    printf("%-25s %8s %8s %8s %8s\n", "Parametre", "SN-Pb", "CA-Pb", "AGM", "GEL");
    printf("%-25s %8s %8s %8s %8s\n", "---------", "-----", "-----", "---", "---");

    for (int t = 0; t < BAT_TYPE_COUNT; t++) {
        float V_bulk = bat_pb_cell_to_total(BAT_VCH_PER_CELL[t], 6);
        float V_float = bat_pb_cell_to_total(BAT_VFL_PER_CELL[t], 6);
        float V_boost = bat_pb_cell_to_total(BAT_VBOOST_PER_CELL[t], 6);
        printf("%-25s %8.2f %8.2f %8.2f %8.2f\n",
               (t == 0) ? "V absorption [V]" : "",
               V_bulk, V_bulk, V_bulk, V_bulk);
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
     *  EXEMPLE 9: Simulation de charge
     * ================================================================ */
    printf("--- 9. Simulation machine a etats (charge) ---\n");

    bat_pb_t bat_sim;
    bat_pb_init(&bat_sim, BAT_TYPE_CA_PB_LIQUIDE, 6, 100.0f, BAT_REGULATOR_CC_CV);
    bat_pb_compute_all(&bat_sim);

    /* Phase 1: Bulk */
    bat_pb_update_state(&bat_sim, 13.2f, 25.0f, 25.0f);
    printf("V=13.2V, I=25A -> Phase: %s\n", 
           bat_pb_phase_to_string(bat_pb_determine_charge_phase(&bat_sim)));

    /* Phase 2: Absorption */
    bat_sim.state.charge_phase = BAT_CHARGE_PHASE_ABSORPTION;
    bat_sim.state.charge_time_hours = 2.0f;
    bat_pb_update_state(&bat_sim, 14.4f, 2.0f, 25.0f);
    printf("V=14.4V, I=2A  -> Phase: %s\n", 
           bat_pb_phase_to_string(bat_pb_determine_charge_phase(&bat_sim)));

    /* Phase 3: Floating */
    bat_sim.state.charge_phase = BAT_CHARGE_PHASE_FLOATING;
    bat_pb_update_state(&bat_sim, 13.8f, 0.3f, 25.0f);
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

    return 0;
}
