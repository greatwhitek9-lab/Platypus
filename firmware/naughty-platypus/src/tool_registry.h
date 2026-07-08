#ifndef NAUGHTY_PLATYPUS_TOOL_REGISTRY_H
#define NAUGHTY_PLATYPUS_TOOL_REGISTRY_H

#include <stdbool.h>
#include <stdint.h>

enum np_tool_risk {
    NP_RISK_LOW = 0,
    NP_RISK_RESTRICTED = 1,
    NP_RISK_UNSUPPORTED = 2,
};

enum np_tool_status {
    NP_STATUS_IMPLEMENTED = 0,
    NP_STATUS_PLANNED = 1,
    NP_STATUS_STUB_ONLY = 2,
    NP_STATUS_DISABLED = 3,
    NP_STATUS_UNSUPPORTED = 4,
};

struct np_tool {
    const char *id;
    const char *name;
    const char *description;
    enum np_tool_risk risk;
    enum np_tool_status status;
    bool enabled;
    int (*run)(void);
};

const struct np_tool *np_tools_get(uint32_t *count);
int np_tool_run_by_id(const char *id);
const char *np_status_to_str(enum np_tool_status status);
const char *np_risk_to_str(enum np_tool_risk risk);

#endif
