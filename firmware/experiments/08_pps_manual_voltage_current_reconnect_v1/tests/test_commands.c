#include "../pps_console_command.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static PPSCommand check(const char *line, PPSCommandType type)
{
    PPSCommand cmd = pps_parse_command(line);
    if (cmd.type != type)
        fprintf(stderr, "Command '%s': got %d, expected %d (%s)\n",
                line, (int)cmd.type, (int)type, cmd.error ? cmd.error : "no error");
    assert(cmd.type == type);
    return cmd;
}
int main(void)
{
    PPSCommand c;
    c = check("7000", PPS_CMD_SET_V); assert(c.mv == 7000);
    c = check("7.02", PPS_CMD_SET_V); assert(c.mv == 7020);
    c = check("\tv 8.00\t", PPS_CMD_SET_V); assert(c.mv == 8000);
    c = check("voltage 10020", PPS_CMD_SET_V); assert(c.mv == 10020);
    c = check("i 1.00", PPS_CMD_SET_I); assert(c.ma == 1000);
    c = check("i 1.50", PPS_CMD_SET_I); assert(c.ma == 1500);
    c = check("current 1800", PPS_CMD_SET_I); assert(c.ma == 1800);
    c = check("i 3.00", PPS_CMD_SET_I); assert(c.ma == 3000);
    c = check("set 8.00 1.50", PPS_CMD_SET_VI);
    assert(c.mv == 8000 && c.ma == 1500);
    c = check("  set\t10.02   2.20  ", PPS_CMD_SET_VI);
    assert(c.mv == 10020 && c.ma == 2200);
    c = check("7000 1000", PPS_CMD_SET_VI);
    assert(c.mv == 7000 && c.ma == 1000);
    c = check("7.00 1.80", PPS_CMD_SET_VI);
    assert(c.mv == 7000 && c.ma == 1800);
    check("s", PPS_CMD_STATUS); check("status", PPS_CMD_STATUS);
    check("c", PPS_CMD_CAPS); check("caps", PPS_CMD_CAPS);
    check("r", PPS_CMD_RETRY); check("retry", PPS_CMD_RETRY);
    check("help", PPS_CMD_HELP); check("?", PPS_CMD_HELP);
    check("  ", PPS_CMD_EMPTY);
    check("set 8.00", PPS_CMD_INVALID);
    check("set 8.00 1.50 extra", PPS_CMD_INVALID);
    check("i", PPS_CMD_INVALID);
    check("i 1.001", PPS_CMD_INVALID);
    check("i -1", PPS_CMD_INVALID);
    check("current 1.2.0", PPS_CMD_INVALID);
    check("v 7.020", PPS_CMD_INVALID);
    check("v -7.00", PPS_CMD_INVALID);
    check("no_such_command", PPS_CMD_INVALID);
    check("set 7000 nope", PPS_CMD_INVALID);
    check("", PPS_CMD_EMPTY);
    /* RDO / application validity is separate from parsing. */
    assert(pps_valid_voltage(5000)); assert(pps_valid_voltage(7000));
    assert(pps_valid_voltage(21000)); assert(!pps_valid_voltage(4990));
    assert(!pps_valid_voltage(7010)); assert(!pps_valid_voltage(21020));
    assert(pps_valid_current(1000)); assert(pps_valid_current(1050));
    assert(pps_valid_current(1500)); assert(pps_valid_current(6350));
    assert(!pps_valid_current(950)); assert(!pps_valid_current(1025));
    assert(!pps_valid_current(6400));
    puts("voltage+current command parser and range tests OK");
    return 0;
}
