#!/usr/bin/env python3
"""Interactive host-only API console: never opens COM/USB hardware."""
from __future__ import annotations
import argparse
from pathlib import Path
from src.fixture_loader import load_fixture
from src.m5_api import handle_request


def main() -> None:
    parser = argparse.ArgumentParser(description="Mock read-only PD/PPS host API; no MCU/PD hardware activity")
    parser.add_argument("--fixture", default="fixtures/pd17_m13g_pps_ready.json")
    args = parser.parse_args()
    fixture = Path(args.fixture)
    print(f"MOCK ONLY: fixture={fixture}; NO USB/PD connections are used. Type INFO, HEALTH, GET_CAPS, GET_CAP 6, STATUS, MEASURE; Ctrl+C to quit.")
    snapshot = load_fixture(fixture)
    try:
        while True:
            line = input("M5-1> ")
            print(handle_request(snapshot, (line + "\n").encode("utf-8")).decode("ascii"), end="")
    except (KeyboardInterrupt, EOFError):
        print("\nMock console closed")


if __name__ == "__main__":
    main()
