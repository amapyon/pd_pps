# Experiment 25: 正式M5-1 読取専用PC API（ホスト専用）

更新日：2026-11-12。**独立したホスト模擬実験であり、CH32X035にFlashするファームウェアではありません。**

## 目的

実験24のUSB CDC自動復旧を基盤とする正式M5のPC制御に向け、情報・状態・能力・計測の応答形式とバリデーションを定義・自動テストする。

- 詳細仕様：[API_V1_DRAFT.md](API_V1_DRAFT.md)
- ロードマップ：[docs/M5_NEXT_PC_CONTROL_PLAN.md](../../../docs/M5_NEXT_PC_CONTROL_PLAN.md)
- 出典：PD17の`src/pd_policy.h`、`src/pd_sensors.h`、`tests/test_policy.c`。
- **安全条件：** 現試作機はCoreBoardのJ1と増設PC用USB-CがD+/D−を共有。両USB端子を同時接続しない。M5-Gハードウェア安全ゲート完了までPD電源＋PC USBの同時試験をしない。

## Windows 11（Python 3）テスト

```powershell
Set-Location 'C:\pj\pd_pps\src\pd_pps'
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File '.\firmware\experiments\25_m5_readonly_api\Test-M5-1-Host.ps1'
```

Ubuntu/Linux：

```bash
cd firmware/experiments/25_m5_readonly_api
python3 -m unittest discover -s tests -v
```

オプションの対話モック（PCだけで動作、COMポートは開かない）：

```powershell
cd firmware/experiments/25_m5_readonly_api
python .\run_mock.py --fixture .\fixtures\pd17_m13g_pps_ready.json
# 手入力: INFO, HEALTH, GET_CAPS, GET_CAP 6, STATUS, MEASURE
```

## ホスト試験対象

`INFO`/`HEALTH`/`GET_CAPS`/`GET_CAP N`/`STATUS`/`MEASURE`、PDO/APDOの値と単位、無効センサー、未接続時の旧データ秘匿、異常行、未対応コマンド、非読取コマンドの拒否、短い応答長、切断→再接続の模擬状態変化。

**現段階の非対応：** USB CDC実機、PD17との実際のメモリ連結、ファームウェアBuild、INA226測定精度、PPS設定。既存17・21～24のコードと共有ch32funは変更しない。Gitへのコミット・Pushも自動では行わない。
