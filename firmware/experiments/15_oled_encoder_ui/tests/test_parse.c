#include "../console_parse.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
static void valid(const char *s, uint32_t want)
{ uint32_t mv=0; assert(parse_voltage_mv(s, &mv)); assert(mv==want); }
static void invalid(const char *s)
{ uint32_t mv=0; assert(!parse_voltage_mv(s, &mv)); }
int main(void)
{
    valid("7",7000); valid("7.00",7000); valid("7.02",7020);
    valid("7020",7020); valid("11.0",11000);
    valid("21.00",21000); valid("5",5000);
    invalid(""); invalid("7."); invalid(".02"); invalid("7.020");
    invalid("7.2.0"); invalid("-7"); invalid("abc");
    assert(7020 % 20u == 0u);
    assert(7010 % 20u != 0u);
    puts("parse test OK");
    return 0;
}
