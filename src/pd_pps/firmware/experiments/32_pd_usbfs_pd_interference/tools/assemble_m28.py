#!/usr/bin/env python3
"""Assemble an independent M28 from the user's pinned PD17 source tree.

Copies only into new experiment28; refuses dirty/unexpected PD17 source.
Does NOT modify PD17, ch32fun, Git index, or MCU flash.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import shutil
import subprocess

EXPECTED = {
    "src/main.c": "0a1db506041d12df4071015e899e6f0df2985da6",
    "src/pd_sensors.c": "f5c69512145673de4fe87df5b954994896f65dd2",
    "src/pd_sensors.h": "d1706e2ea452728063af8e25679ce53305233527",
    "src/pd_policy.c": "237d46163e30994cac0adcc75408fcdfde79c569",
    "src/pd_policy.h": "aec69affcc25bf36e62a3786457ee4752b50acfb",
    "src/pd_phy_ch32x035.c": "eecb3eef85d1bac1a4eb4c497d9cddeb805dc39d",
    "src/ina226.c": "7bc8c377ff3cbbd81572648375812622f0978b53",
}


def git(root: Path, *args: str) -> str:
    out = subprocess.run(["git", *args], cwd=root, capture_output=True, text=True, check=True)
    return out.stdout.strip()


def replace_once(text: str, needle: str, replacement: str, label: str) -> str:
    if text.count(needle) != 1:
        raise RuntimeError(f"M28 assembly anchor mismatch: {label} ({text.count(needle)})")
    return text.replace(needle, replacement, 1)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repository", required=True)
    parser.add_argument("--experiment", required=True)
    options = parser.parse_args()
    repo = Path(options.repository).resolve()
    target = Path(options.experiment).resolve()
    pd17_rel = "firmware/experiments/17_pd_spr_sink_rebuild"
    pd17 = repo / pd17_rel
    if not pd17.is_dir() or not target.is_dir():
        raise RuntimeError("PD17 source or M28 destination not found")
    changes = git(repo, "status", "--porcelain", "--untracked-files=all", "--", pd17_rel)
    if changes:
        raise RuntimeError("PD17 has worktree/index modifications; refusing unreviewed source copy:\n"+changes)
    for rel, blob_sha in EXPECTED.items():
        actual = git(repo, "rev-parse", f"HEAD:{pd17_rel}/{rel}")
        if actual != blob_sha:
            raise RuntimeError(f"PD17 pinned source mismatch: {rel}, HEAD={actual}; expected={blob_sha}")
    for src in sorted((pd17 / "src").iterdir()):
        if src.is_file() and src.suffix in (".c", ".h"):
            dst = target / "src" / src.name
            if dst.exists():
                raise RuntimeError(f"M28 file collision: {dst}")
            shutil.copyfile(src, dst)
    mainfile = target / "src" / "main.c"
    s = mainfile.read_text(encoding="utf-8")
    s = replace_once(s, '#include "pd_stop_diag.h"', '#include "pd_stop_diag.h"\n#include "m28_usb.h"', "main include")
    s = replace_once(s,
        'pd_vbus_monitor_init(); /* M13a-2: nonblocking 10ms ADC sampling */',
        'pd_vbus_monitor_init(); /* M13a-2: nonblocking 10ms ADC sampling */\n    m28_usb_init(); /* Experimental USB CDC; PD17 retains PD priority */',
        "USB setup")
    s = replace_once(s,
        'bool sensor_ready_sampled = false;',
        'bool sensor_ready_sampled = false;\n'
        '    bool m28_sample_valid = false;\n'
        '    uint32_t m28_sample_ms = 0;\n'
        '    uint32_t m28_sample_mv = 0, m28_sample_ma = 0;\n'
        '    uint8_t m28_sample_epoch = 0;',
        "sample state")
    s = replace_once(s,
        'pd_sensors_capture_ready();',
        'pd_sensors_capture_ready();\n'
        '                m28_sample_ms = now;\n'
        '                m28_sample_epoch = policy.caps_epoch;\n'
        '                m28_sample_mv = policy.requested_mv;\n'
        '                m28_sample_ma = policy.requested_ma;\n'
        '                m28_sample_valid = true;',
        "sensor timestamp")
    s = replace_once(s,
        'if (ev==PE_EV_HARD_RESET_RX) {',
        'if (ev==PE_EV_HARD_RESET_RX) {\n'
        '                m28_sample_valid = false;\n'
        '                sensor_ready_seen = false;\n'
        '                sensor_ready_sampled = false;\n'
        '                pd_sensors_reset_ready();',
        "hard reset invalidation")
    s = replace_once(s,
        'if (ev==PE_EV_DETACH) {',
        'if (ev==PE_EV_DETACH) {\n                m28_sample_valid = false;',
        "detach invalidation")
    s = replace_once(s,
        '        /* Drain recorded events only after confirmed CC detach + 500 ms.',
        '        /* One bounded USB action after the full PD service pass.\n'
        '           The original sensor is one-shot; never serve stale readings. */\n'
        '        bool m28_fresh = m28_sample_valid && phy_attached() &&\n'
        '            policy.state == PE_READY && policy.contract_valid &&\n'
        '            policy.caps_epoch == m28_sample_epoch &&\n'
        '            policy.requested_mv == m28_sample_mv &&\n'
        '            policy.requested_ma == m28_sample_ma &&\n'
        '            (uint32_t)(now - m28_sample_ms) <= 2000u;\n'
        '        m28_usb_service(now, &policy,\n'
        '            m28_fresh ? pd_sensors_ready_reading() : NULL, m28_fresh);\n'
        '        /* Drain recorded events only after confirmed CC detach + 500 ms.',
        "USB service insertion")
    mainfile.write_text(s, encoding="utf-8", newline="\n")
    sf = target / "src" / "pd_sensors.c"
    sensor_text = sf.read_text(encoding="utf-8")
    sensor_text += '\n/* M28 read-only sensor snapshot; original PD17 remains unchanged. */\n'
    sensor_text += 'const pd_sensor_reading_t *pd_sensors_ready_reading(void) {\n'
    sensor_text += '    return ready_attempted ? &ready_reading : NULL;\n}\n'
    sensor_text += 'void pd_sensors_reset_ready(void) {\n'
    sensor_text += '    ready_reading = (pd_sensor_reading_t){0};\n'
    sensor_text += '    ready_attempted = false;\n}\n'
    sf.write_text(sensor_text, encoding="utf-8", newline="\n")
    sh = target / "src" / "pd_sensors.h"
    hdr = sh.read_text(encoding="utf-8")
    hdr = replace_once(hdr, '#endif',
        '/* M28 snapshots; these helpers only access local cached sensor state. */\n'
        'const pd_sensor_reading_t *pd_sensors_ready_reading(void);\n'
        'void pd_sensors_reset_ready(void);\n#endif', "sensor header")
    sh.write_text(hdr, encoding="utf-8", newline="\n")
    if 'm28_usb_service(now, &policy' not in mainfile.read_text(encoding='utf-8'):
        raise RuntimeError('M28 service injection not found')
    print(f"Assembled M28 independent firmware: {target}")
    print("PD17 snapshot verified; source changes apply ONLY to M28.")
    print("M28 Build and real-hardware USBPD/USBFS coexistence remain to be verified.")


if __name__ == "__main__":
    main()
