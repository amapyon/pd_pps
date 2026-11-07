#!/usr/bin/env python3
"""PD17 M13d: assess one complete detached-only diagnostic session.

This checks the *Sink's own observed events*, not electrical USB-IF compliance
or independently verified on-the-wire Hard Reset signaling.
"""
import argparse
from pathlib import Path
import re
import sys

TRACE = re.compile(r"^PD17 TRACE tick=[0-9A-Fa-f]+ code=(\d+) detail=(\d+)\b")
LABELS = {
    "trace_end": "PD17 TRACE END ",
    "stability": "PD17 STABILITY session_ms=",
    "errors": "PD17 STABILITY ERR ",
    "stop": "PD17 STOP ",
    "hr": "PD17 HR VBUS ",
    "mode": "PD17 MODE ",
}


def latest_complete_session(data: str):
    """Use the last block that contains TRACE BEGIN through VBUS END."""
    lines = data.splitlines()
    blocks = []
    start = None
    for i, line in enumerate(lines):
        if line.startswith("PD17 TRACE BEGIN"):
            start = i
        elif line.startswith("PD17 VBUS END") and start is not None:
            blocks.append(lines[start:i + 1])
            start = None
    if not blocks:
        raise ValueError("TRACE BEGIN～VBUS ENDの完結ログがありません。USB-C切断後の出力を保存してください")
    return blocks[-1]


def fields_of(line: str):
    d = {}
    for k, v in re.findall(r"\b([A-Za-z][A-Za-z0-9_]*)=([^\s]+)", line):
        try:
            d[k] = int(v, 10)
        except ValueError:
            d[k] = v
    return d


def examine(data: str, mode: str, expected_ready: str):
    try:
        block = latest_complete_session(data)
    except ValueError as exc:
        return "INCOMPLETE", [str(exc)]
    found = {}
    codes = []
    for line in block:
        hit = TRACE.search(line)
        if hit:
            codes.append((int(hit.group(1)), int(hit.group(2))))
        for key, prefix in LABELS.items():
            if line.startswith(prefix):
                found[key] = fields_of(line)
    missing = [k for k in LABELS if k not in found]
    if missing:
        return "INCOMPLETE", ["必須行なし: " + ", ".join(missing)]

    e, s, stop, hr = (found[x] for x in ("errors", "stability", "stop", "hr"))
    trace_end, device_mode = found["trace_end"], found["mode"]
    trouble = []
    inconclusive = []

    def eq(label, got, want):
        if got != want:
            trouble.append(f"{label}: 期待 {want}, 実測 {got}")

    def one(label, got):
        eq(label, got, 1)

    def exists(label, got):
        if got is None or got < 1:
            trouble.append(f"{label}: 1以上を期待、実測 {got}")

    # A missing trace event isn't proof of no Hard Reset if trace was lost.
    for field, got in (("trace_dropped", trace_end.get("trace_dropped")),
                       ("event_lost", trace_end.get("event_lost")),
                       ("trace_drop_session", e.get("trace_drop_session")),
                       ("event_lost_session", e.get("event_lost_session"))):
        if got is None or got > 0:
            inconclusive.append(f"{field}={got}: 欠落によりイベント列を確証できません")
    eq("adc_fault", e.get("adc_fault"), 0)
    one("normal", device_mode.get("normal"))
    eq("bench_hr_tx", device_mode.get("bench_hr_tx"), 0)
    if any(code in (19, 20) for code, _ in codes):
        trouble.append("Sink起因Hard Reset(code=19/20)が記録されています")
    if s.get("refresh_started") != s.get("refresh_done"):
        trouble.append("PPS更新開始と完了の回数が不一致")

    hrs = [i for i, item in enumerate(codes) if item[0] == 3]
    if mode == "baseline":
        eq("Hard Reset受信TRACE件数", len(hrs), 0)
        eq("source_hr", e.get("source_hr"), 0)
        eq("hr_rx", stop.get("hr_rx"), 0)
        eq("STOP reason", stop.get("reason"), 1)
        if (s.get("pps_ready", 0) + s.get("fixed_ready", 0)) < 1:
            trouble.append("Ready到達がありません")
    elif mode in ("first", "second"):
        expected_hr = 1 if mode == "first" else 2
        eq("Hard Reset受信TRACE件数", len(hrs), expected_hr)
        eq("source_hr", e.get("source_hr"), expected_hr)
        eq("hr_rx", stop.get("hr_rx"), expected_hr)
        one("HR low_seen", hr.get("low_seen"))
        one("HR high_seen", hr.get("high_seen"))
        eq("HR error", hr.get("error"), 0)
        one("recovery_handoff", hr.get("recovery_handoff"))
        one("auto_recovery", hr.get("auto_recovery"))
        if hrs:
            first_index = hrs[0]
            if not any(code == 15 and detail == 4 for code, detail in codes[first_index + 1:]):
                trouble.append("1回目Hard Reset後にReady到達TRACE(code=15 detail=4)がありません")
        if mode == "first":
            eq("STOP reason(最終ケーブル抜去)", stop.get("reason"), 1)
            if expected_ready == "pps":
                one("pps_recovery", hr.get("pps_recovery"))
            elif expected_ready == "fixed":
                eq("pps_recovery", hr.get("pps_recovery"), 0)
                exists("fixed_ready", s.get("fixed_ready"))
            else:
                if (s.get("pps_ready", 0) + s.get("fixed_ready", 0)) < 2:
                    trouble.append("初回Ready＋Hard Reset後Readyの合計が2回未満")
        else:
            eq("STOP reason(2回目Hard Reset保護)", stop.get("reason"), 10)
            if len(hrs) >= 2:
                if any(code == 15 and detail == 4 for code, detail in codes[hrs[1] + 1:]):
                    trouble.append("2回目Hard Reset後にもReadyへ復帰しました")
    else:
        raise ValueError("Unknown mode")

    if inconclusive:
        return "INCOMPLETE", inconclusive + trouble
    if trouble:
        return "FAIL", trouble
    return "PASS", [f"mode={mode}, session_ms={s.get('session_ms')}, source_hr={e.get('source_hr')}, PPS更新={s.get('refresh_done')}"]


def main():
    parser = argparse.ArgumentParser(description="PD17 Source発Hard Reset ログ判定（MCU側の観測のみ）")
    parser.add_argument("logfile", type=Path, help="PD17 TRACE BEGIN～VBUS ENDを含むUTF-8ログ")
    parser.add_argument("--mode", choices=("baseline", "first", "second"), required=True)
    parser.add_argument("--expected-ready", choices=("either", "fixed", "pps"), default="either")
    args = parser.parse_args()
    try:
        data = args.logfile.read_text(encoding="utf-8-sig", errors="replace")
    except OSError as exc:
        parser.error(str(exc))
    result, details = examine(data, args.mode, args.expected_ready)
    print("M13d PD17 Source HR log check:", result)
    for msg in details:
        print("-", msg)
    print("注: PD信号波形と規格適合性は、独立したアナライザ・計測器で別途確認が必要です")
    return {"PASS": 0, "FAIL": 1, "INCOMPLETE": 2}[result]


if __name__ == "__main__":
    sys.exit(main())
