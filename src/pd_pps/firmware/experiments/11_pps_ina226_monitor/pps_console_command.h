#ifndef PPS_CONSOLE_COMMAND_H
#define PPS_CONSOLE_COMMAND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "console_parse.h"

/* The PPS RDO uses 20 mV and 50 mA steps. The PD transport library
 * now accepts 50mA..6350mA. The connected APDO supplies final bounds.
 * A command sets the SOURCE'S REQUESTED current limit, not load current.
 */
#define PPS_TARGET_MIN_MV   3300u
#define PPS_TARGET_MAX_MV  21000u
#define PPS_TARGET_MIN_MA     50u
#define PPS_TARGET_MAX_MA   6350u

static inline bool pps_valid_voltage(uint32_t mv)
{
    return mv >= PPS_TARGET_MIN_MV && mv <= PPS_TARGET_MAX_MV &&
           mv % 20u == 0u;
}

static inline bool pps_valid_current(uint32_t ma)
{
    return ma >= PPS_TARGET_MIN_MA && ma <= PPS_TARGET_MAX_MA &&
           ma % 50u == 0u;
}

typedef enum {
    PPS_CMD_EMPTY,
    PPS_CMD_HELP,
    PPS_CMD_STATUS,
    PPS_CMD_METER,
    PPS_CMD_SCAN,
    PPS_CMD_CAPS,
    PPS_CMD_LIMITS,
    PPS_CMD_RETRY,
    PPS_CMD_SET_V,
    PPS_CMD_SET_I,
    PPS_CMD_SET_VI,
    PPS_CMD_INVALID
} PPSCommandType;

typedef struct {
    PPSCommandType type;
    uint32_t mv;
    uint32_t ma;
    const char *error;
} PPSCommand;

static inline bool parse_current_ma(const char *text, uint32_t *ma)
{
    /* Same syntax as voltage: 1.50 = 1500 mA; 1500 = 1500 mA.
     * All electrical limits are enforced by the application/library. */
    return parse_voltage_mv(text, ma);
}

static inline bool pps_ws(char c)
{
    return c == ' ' || c == '\t';
}

static inline PPSCommand pps_invalid(const char *why)
{
    PPSCommand cmd = {0};
    cmd.type = PPS_CMD_INVALID;
    cmd.error = why;
    return cmd;
}

/* Accepts a null-terminated console line; leaves it unmodified.
 * Single bare number remains a voltage, as in the working v3 program.
 * The optional two-number form is an atomic voltage+current command.
 */
static inline PPSCommand pps_parse_command(const char *line)
{
    PPSCommand cmd = {0};
    if (!line) return pps_invalid("empty command pointer");
    char tokens[3][32] = {{0}};
    unsigned n = 0u;
    const char *s = line;
    while (*s) {
        while (pps_ws(*s)) ++s;
        if (!*s) break;
        if (n >= 3u) return pps_invalid("too many arguments");
        unsigned k = 0u;
        while (*s && !pps_ws(*s)) {
            if (k + 1u >= sizeof(tokens[0]))
                return pps_invalid("argument is too long");
            tokens[n][k++] = *s++;
        }
        tokens[n][k] = '\0';
        ++n;
    }
    if (n == 0u) { cmd.type = PPS_CMD_EMPTY; return cmd; }
    if (n == 1u) {
        if (!strcmp(tokens[0], "help") || !strcmp(tokens[0], "?"))
            cmd.type = PPS_CMD_HELP;
        else if (!strcmp(tokens[0], "status") || !strcmp(tokens[0], "s"))
            cmd.type = PPS_CMD_STATUS;
        else if (!strcmp(tokens[0], "meter") || !strcmp(tokens[0], "m"))
            cmd.type = PPS_CMD_METER;
        else if (!strcmp(tokens[0], "scan"))
            cmd.type = PPS_CMD_SCAN;
        else if (!strcmp(tokens[0], "caps") || !strcmp(tokens[0], "c"))
            cmd.type = PPS_CMD_CAPS;
        else if (!strcmp(tokens[0], "limits") || !strcmp(tokens[0], "l"))
            cmd.type = PPS_CMD_LIMITS;
        else if (!strcmp(tokens[0], "retry") || !strcmp(tokens[0], "r"))
            cmd.type = PPS_CMD_RETRY;
        else if (parse_voltage_mv(tokens[0], &cmd.mv))
            cmd.type = PPS_CMD_SET_V;
        else return pps_invalid("expected voltage or a valid command");
    } else if (n == 2u) {
        if (!strcmp(tokens[0], "v") || !strcmp(tokens[0], "voltage")) {
            if (!parse_voltage_mv(tokens[1], &cmd.mv))
                return pps_invalid("voltage: use 7.02 or 7020 (mV)");
            cmd.type = PPS_CMD_SET_V;
        } else if (!strcmp(tokens[0], "i") || !strcmp(tokens[0], "current")) {
            if (!parse_current_ma(tokens[1], &cmd.ma))
                return pps_invalid("current: use 1.50 or 1500 (mA)");
            cmd.type = PPS_CMD_SET_I;
        } else {
            if (!parse_voltage_mv(tokens[0], &cmd.mv) ||
                !parse_current_ma(tokens[1], &cmd.ma))
                return pps_invalid("use: set 7.02 1.50 (or 7020 1500)");
            cmd.type = PPS_CMD_SET_VI;
        }
    } else {
        if (strcmp(tokens[0], "set") ||
            !parse_voltage_mv(tokens[1], &cmd.mv) ||
            !parse_current_ma(tokens[2], &cmd.ma))
            return pps_invalid("use: set 7.02 1.50 (or 7020 1500)");
        cmd.type = PPS_CMD_SET_VI;
    }
    return cmd;
}

#endif
