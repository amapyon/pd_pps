"""Reversible allowlist for M43B-5 diagnostic PHY changes.
The reverse transform must reproduce the exact SHA-256 of delivered M43B-4 PHY.
Generated from the verified M43B-4 project and the new M43B-5 project.
"""
from hashlib import sha256
BASE_SHA256='9a5a9e7d608c394aff6c853b91325b0d2832841eae532f564b36f141f60dc09f'
REVERSE_PATCHES=[('/* M43B5 diagnostic book-keeping; no additional USBFS IRQ work.\n * req_seq increments only on a newly accepted PE_CMD_REQUEST, not retries. */\nstatic volatile uint16_t m43b5_req_seq;\nstatic volatile bool m43b5_req_active, m43b5_refresh_pending;\nstatic volatile uint8_t m43b5_req_phase;\nstatic volatile uint32_t m43b5_window_irq_base, m43b5_req_irq_sum;\n\nvoid phy_m43b5_mark_refresh(void) { m43b5_refresh_pending = true; }\nstatic void m43b5_begin_request(uint16_t h) {\n    m43b5_req_seq++;\n    m43b5_req_active = true;\n    m43b5_req_irq_sum = 0u;\n    m43b5_req_phase = m43b5_refresh_pending ? 2u :\n                      (m43b3_sram.reserved1 ? 1u : 0u);\n    m43b5_refresh_pending = false;\n    m43b3_sram_note(M43B5_EVT_REQ_BEGIN, m43b5_req_phase,\n        (uint32_t)m43b5_req_seq | ((uint32_t)h << 16),\n        m43b2_diag_usbfs_crc_count(), SysTick->CNTL);\n}\nstatic void m43b5_window_finish(uint8_t outcome) {\n    if (!m43b5_req_active || !crc_wait_armed) return;\n    uint32_t n = m43b2_diag_usbfs_crc_count() - m43b5_window_irq_base;\n    m43b5_req_irq_sum += n;\n    m43b3_sram_note(M43B5_EVT_REQ_WINDOW, outcome,\n        (uint32_t)m43b5_req_seq | ((uint32_t)retry_count << 16) |\n        ((uint32_t)tx_expected_id << 24), n, SysTick->CNTL);\n}\nstatic void m43b5_finish_request(uint8_t reason) {\n    if (!m43b5_req_active) return;\n    m43b3_sram_note(M43B5_EVT_REQ_END, reason,\n        (uint32_t)m43b5_req_seq | ((uint32_t)retry_count << 16),\n        m43b5_req_irq_sum, SysTick->CNTL);\n    m43b5_req_active = false;\n}\n\n', ''), ('            m43b5_window_finish(1u); /* matched GoodCRC closes this window */\n            m43b5_finish_request(1u);\n', ''), ('        m43b5_window_finish(4u); /* interrupted receive window */\n        m43b5_finish_request(4u); /* diagnostic only: superseded by SOP */\n', ''), ('        /* M43B5: first Source HR is recoverable and MUST NOT freeze SRAM.\n         * We record its occurrence and only freeze on the second HR. */\n        m43b5_window_finish(3u); /* interrupted receive window */\n        m43b5_finish_request(3u);\n        uint32_t m43b5_hr_count = m43b3_sram.reserved1 + 1u;\n        m43b3_sram.reserved1 = m43b5_hr_count;\n        m43b3_sram_note(M43B5_EVT_HR_INDEX,\n                         (uint8_t)(m43b5_hr_count > 255u ? 255u : m43b5_hr_count),\n                         status, (uint32_t)USBPD->CONTROL, SysTick->CNTL);\n        if (m43b5_hr_count >= 2u)\n            m43b3_sram_freeze(M43B3_FREEZE_SOURCE_HR, SysTick->CNTL);\n        m43b5_refresh_pending = false;\n', '        m43b3_sram_freeze(M43B3_FREEZE_SOURCE_HR, SysTick->CNTL); /* M43B3 SRAM */\n'), ('            m43b5_window_irq_base = m43b2_diag_usbfs_crc_count();\n', ''), ('                m43b5_req_seq = 0u;\n                m43b5_req_active = false;\n                m43b5_refresh_pending = false;\n                m43b5_req_irq_sum = 0u;\n                m43b5_window_irq_base = 0u;\n', ''), ('    if (command == PE_CMD_REQUEST) m43b5_begin_request(h);\n', ''), ('    m43b5_window_finish(2u); /* diagnostic only, before retry increment */\n', ''), ('        m43b5_finish_request(2u);\n', '')]
def restore_b4_phy(new_phy):
    for new,old in REVERSE_PATCHES:
        if new_phy.count(new)!=1:
            raise AssertionError('M43B5 patch hunk missing or repeated')
        new_phy=new_phy.replace(new,old,1)
    if sha256(new_phy.encode('utf-8')).hexdigest()!=BASE_SHA256:
        raise AssertionError('M43B4 PHY baseline fingerprint mismatch')
    return new_phy

import unittest
from pathlib import Path
class B5BaselineGuard(unittest.TestCase):
    def test_reversible_phy_diff_only(self):
        src=(Path(__file__).resolve().parents[1]/'src/pd_phy_ch32x035.c').read_text(encoding='utf-8')
        self.assertEqual(sha256(restore_b4_phy(src).encode()).hexdigest(),BASE_SHA256)
