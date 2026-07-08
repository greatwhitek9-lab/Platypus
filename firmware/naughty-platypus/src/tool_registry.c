#include <errno.h>
#include <stddef.h>
#include <string.h>

#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include "tool_registry.h"
#include "passive_survey.h"
#include "restricted_stubs.h"

/*
 * Tool registry.
 *
 * Safe tools point to implemented functions.
 * Restricted tools point to stub functions and remain enabled=false/default n.
 *
 * PURPLE DOC MAP:
 *   See docs/manual-activation-map.html for purple-highlighted private-lab
 *   replacement points.
 */

static int np_run_passive_survey(void)
{
    return np_passive_survey_start();
}

static int np_run_survey_status(void)
{
    return np_passive_survey_status();
}

static const struct np_tool tools[] = {
    {
        .id = "ble_passive_survey",
        .name = "BLE Passive Survey",
        .description = "Passive BLE advertisement detection, RSSI logging, and metadata inventory.",
        .risk = NP_RISK_LOW,
        .status = NP_STATUS_IMPLEMENTED,
        .enabled = IS_ENABLED(CONFIG_NP_ENABLE_PASSIVE_SURVEY),
        .run = np_run_passive_survey,
    },
    {
        .id = "ble_adv_analyzer",
        .name = "BLE Advertisement Analyzer",
        .description = "Advertisement field parsing and survey summary reporting.",
        .risk = NP_RISK_LOW,
        .status = NP_STATUS_IMPLEMENTED,
        .enabled = IS_ENABLED(CONFIG_NP_ENABLE_ADV_ANALYZER),
        .run = np_run_survey_status,
    },
    {
        .id = "ble_rssi_survey",
        .name = "BLE RSSI Survey",
        .description = "RSSI counters for lab positioning, antenna, and enclosure testing.",
        .risk = NP_RISK_LOW,
        .status = NP_STATUS_IMPLEMENTED,
        .enabled = IS_ENABLED(CONFIG_NP_ENABLE_RSSI_SURVEY),
        .run = np_run_survey_status,
    },
    {
        .id = "ble_channel_survey",
        .name = "BLE Channel Survey",
        .description = "Safe RSSI and observer visibility summary.",
        .risk = NP_RISK_RESTRICTED,
        .status = NP_STATUS_IMPLEMENTED,
        .enabled = IS_ENABLED(CONFIG_NP_ENABLE_CHANNEL_SURVEY_PLACEHOLDER),
        .run = np_safe_channel_survey,
    },
    {
        .id = "ble_connection_follow",
        .name = "BLE Connection Follow",
        .description = "Safe allowlisted connection status reporter.",
        .risk = NP_RISK_RESTRICTED,
        .status = NP_STATUS_IMPLEMENTED,
        .enabled = IS_ENABLED(CONFIG_NP_ENABLE_BLE_CONNECTION_FOLLOW_PLACEHOLDER),
        .run = np_safe_ble_connection_follow,
    },
    {
        .id = "ble_gatt_mutation_lab",
        .name = "BLE GATT Mutation Lab",
        .description = "Safe read-only GATT audit status reporter.",
        .risk = NP_RISK_RESTRICTED,
        .status = NP_STATUS_IMPLEMENTED,
        .enabled = IS_ENABLED(CONFIG_NP_ENABLE_BLE_GATT_MUTATION_PLACEHOLDER),
        .run = np_safe_ble_gatt_mutation,
    },
    {
        .id = "ble_pairing_security_lab",
        .name = "BLE Pairing Security Lab",
        .description = "Safe pairing policy status checker.",
        .risk = NP_RISK_RESTRICTED,
        .status = NP_STATUS_IMPLEMENTED,
        .enabled = IS_ENABLED(CONFIG_NP_ENABLE_BLE_PAIRING_LAB_PLACEHOLDER),
        .run = np_safe_ble_pairing_lab,
    },
    {
        .id = "ble_advertising_tx_lab",
        .name = "BLE Advertising TX Lab",
        .description = "Safe local advertising self-test status reporter.",
        .risk = NP_RISK_RESTRICTED,
        .status = NP_STATUS_IMPLEMENTED,
        .enabled = IS_ENABLED(CONFIG_NP_ENABLE_BLE_ADV_TX_PLACEHOLDER),
        .run = np_safe_ble_adv_tx,
    },
    {
        .id = "ble_stability_stress_lab",
        .name = "BLE Stability Stress Lab",
        .description = "Safe local firmware health self-test reporter.",
        .risk = NP_RISK_RESTRICTED,
        .status = NP_STATUS_IMPLEMENTED,
        .enabled = IS_ENABLED(CONFIG_NP_ENABLE_BLE_STABILITY_STRESS_PLACEHOLDER),
        .run = np_safe_ble_stability_stress,
    },
    {
        .id = "classic_bt_monitor",
        .name = "Bluetooth Classic Monitor",
        .description = "Unsupported placeholder. nRF52840 is not a Bluetooth Classic BR/EDR Ubertooth replacement.",
        .risk = NP_RISK_UNSUPPORTED,
        .status = NP_STATUS_UNSUPPORTED,
        .enabled = IS_ENABLED(CONFIG_NP_ENABLE_CLASSIC_BT_PLACEHOLDER),
        .run = np_stub_classic_bt,
    },
};

const char *np_status_to_str(enum np_tool_status status)
{
    switch (status) {
    case NP_STATUS_IMPLEMENTED: return "implemented";
    case NP_STATUS_PLANNED: return "planned";
    case NP_STATUS_STUB_ONLY: return "stub_only";
    case NP_STATUS_DISABLED: return "disabled";
    case NP_STATUS_UNSUPPORTED: return "unsupported";
    default: return "unknown";
    }
}

const char *np_risk_to_str(enum np_tool_risk risk)
{
    switch (risk) {
    case NP_RISK_LOW: return "low";
    case NP_RISK_RESTRICTED: return "restricted";
    case NP_RISK_UNSUPPORTED: return "unsupported";
    default: return "unknown";
    }
}

const struct np_tool *np_tools_get(uint32_t *count)
{
    if (count != NULL) {
        *count = ARRAY_SIZE(tools);
    }

    return tools;
}

int np_tool_run_by_id(const char *id)
{
    for (size_t i = 0; i < ARRAY_SIZE(tools); i++) {
        if (strcmp(id, tools[i].id) == 0) {
            if (!tools[i].enabled) {
                printk("{\"type\":\"tool_blocked\","
                       "\"tool\":\"%s\","
                       "\"enabled\":false,"
                       "\"risk\":\"%s\","
                       "\"status\":\"%s\","
                       "\"reason\":\"disabled_by_default_or_build_config\"}\n",
                       tools[i].id,
                       np_risk_to_str(tools[i].risk),
                       np_status_to_str(tools[i].status));

                return -EPERM;
            }

            if (tools[i].run == NULL) {
                return -ENOTSUP;
            }

            return tools[i].run();
        }
    }

    printk("{\"type\":\"tool_error\",\"tool\":\"%s\",\"reason\":\"unknown_tool\"}\n", id);
    return -ENOENT;
}
