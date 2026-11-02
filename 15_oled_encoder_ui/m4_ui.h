#ifndef M4_UI_H
#define M4_UI_H

#include <stdbool.h>
#include <stdint.h>
#include "m3_input.h"

/* Pure application UI state: no MCU registers and no direct PD transactions. */
#define M4_MAX_PDOS 8u
#define M4_V_STEP_MV 100u /* 100mV per detent (multiple of PPS's 20mV step). */
#define M4_I_STEP_MA 50u  /* PPS current request encoding step. */

typedef enum {
    M4_PAGE_MONITOR = 0,
    M4_PAGE_MENU,
    M4_PAGE_CAPS,
    M4_PAGE_EDIT_V,
    M4_PAGE_EDIT_I,
    M4_PAGE_CONFIRM,
    M4_PAGE_RESULT
} m4_page_t;

typedef enum { M4_PDO_FIXED = 0, M4_PDO_PPS = 1, M4_PDO_OTHER = 2 } m4_pdo_kind_t;

typedef struct {
    m4_pdo_kind_t kind;
    uint8_t source_index;  /* zero-based index from Source_Capabilities */
    uint32_t min_mv;
    uint32_t max_mv;
    uint32_t max_ma;
} m4_pdo_t;

typedef struct {
    bool apply;
    uint32_t mv;
    uint32_t ma;
    uint32_t original_mv;
    uint32_t original_ma;
} m4_action_t;

typedef struct {
    m4_page_t page;
    uint8_t menu_index;
    uint8_t pdo_index;
    uint8_t pdo_count;
    uint8_t edit_pdo_index;
    m4_pdo_t pdos[M4_MAX_PDOS];
    uint32_t desired_mv;
    uint32_t desired_ma;
    uint32_t base_mv;
    uint32_t base_ma;
    uint32_t edit_mv;
    uint32_t edit_ma;
    bool pd_ready;
    const char *result; /* Constant string owned by the program. */
    uint32_t revision;
} m4_ui_t;

void m4_ui_init(m4_ui_t *ui, uint32_t desired_mv, uint32_t desired_ma);
void m4_ui_set_pd(m4_ui_t *ui, uint32_t desired_mv, uint32_t desired_ma,
                  bool pd_ready);
void m4_ui_set_pdos(m4_ui_t *ui, const m4_pdo_t *pdos, uint8_t count);
m4_action_t m4_ui_input(m4_ui_t *ui, m3_events_t events);
void m4_ui_result(m4_ui_t *ui, const char *message);

#endif
