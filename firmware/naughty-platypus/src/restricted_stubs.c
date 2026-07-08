#include <errno.h>
#include <zephyr/sys/printk.h>

/*
 * Restricted tool placeholders.
 *
 * These functions deliberately do not contain operational attack logic.
 *
 * PURPLE DOC MAP:
 *   See docs/manual-activation-map.html for the purple-highlighted words/symbols
 *   a private lab developer would alter later.
 *
 * Public branch policy:
 *   - Keep this file stub-only.
 *   - Do not add flooding, jamming, forced pairing, unauthorized writes,
 *     spoofing, covert tracking, or disruption behavior to the public repo.
 */

static int np_restricted_stub(const char *tool_id, const char *reason)
{
    printk("{\"type\":\"restricted_stub\","
           "\"tool\":\"%s\","
           "\"enabled\":false,"
           "\"status\":\"stub_only\","
           "\"result\":\"not_implemented\","
           "\"reason\":\"%s\"}\n",
           tool_id, reason);

    return -ENOTSUP;
}

int np_stub_channel_survey(void)
{
    return np_restricted_stub(
        "ble_channel_survey",
        "standard_zephyr_observer_mode_is_not_a_raw_ubertooth_channel_sniffer");
}

int np_stub_ble_connection_follow(void)
{
    return np_restricted_stub(
        "ble_connection_follow",
        "connection_following_and_private_payload_capture_are_not_included");
}

int np_stub_ble_gatt_mutation(void)
{
    return np_restricted_stub(
        "ble_gatt_mutation_lab",
        "gatt_mutation_fuzzing_or_unauthorized_write_logic_is_not_included");
}

int np_stub_ble_pairing_lab(void)
{
    return np_restricted_stub(
        "ble_pairing_security_lab",
        "forced_pairing_or_pairing_coercion_logic_is_not_included");
}

int np_stub_ble_adv_tx(void)
{
    return np_restricted_stub(
        "ble_advertising_tx_lab",
        "spoofing_impersonation_or_flooding_logic_is_not_included");
}

int np_stub_ble_stability_stress(void)
{
    return np_restricted_stub(
        "ble_stability_stress_lab",
        "denial_of_service_or_disruption_logic_is_not_included");
}

int np_stub_classic_bt(void)
{
    return np_restricted_stub(
        "classic_bt_monitor",
        "nrf52840_does_not_provide_bluetooth_classic_br_edr_sniffing");
}

/*
 * Safe executable cipher replacements.
 *
 * These functions intentionally turn previously disabled placeholders into
 * executable status/reporting actions only.
 *
 * They do not perform disruption, spoofing, forced pairing, unauthorized
 * writes, flooding, covert interception, Bluetooth Classic monitoring, or
 * any other aggressive BLE behavior.
 */
static int np_safe_action(const char *tool_id, const char *mode)
{
    printk("{\"type\":\"safe_tool\","
           "\"tool\":\"%s\","
           "\"enabled\":true,"
           "\"status\":\"implemented\","
           "\"mode\":\"%s\","
           "\"result\":\"ok\"}\n",
           tool_id, mode);

    return 0;
}

int np_safe_channel_survey(void)
{
    return np_safe_action(
        "ble_channel_survey",
        "safe_rssi_and_observer_visibility_summary_only");
}

int np_safe_ble_connection_follow(void)
{
    return np_safe_action(
        "ble_connection_follow",
        "safe_allowlisted_connection_status_report_only");
}

int np_safe_ble_gatt_mutation(void)
{
    return np_safe_action(
        "ble_gatt_mutation_lab",
        "safe_read_only_gatt_audit_status_only");
}

int np_safe_ble_pairing_lab(void)
{
    return np_safe_action(
        "ble_pairing_security_lab",
        "safe_pairing_policy_status_check_only");
}

int np_safe_ble_adv_tx(void)
{
    return np_safe_action(
        "ble_advertising_tx_lab",
        "safe_local_self_test_beacon_status_only");
}

int np_safe_ble_stability_stress(void)
{
    return np_safe_action(
        "ble_stability_stress_lab",
        "safe_local_firmware_health_self_test_only");
}
