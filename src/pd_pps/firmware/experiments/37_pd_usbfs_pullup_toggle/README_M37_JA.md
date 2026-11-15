# M37：D+プルアップ ON → OFF → ON → USBFS停止 比較実験

## 目的

M36でPD接続中にUSBFSのD+プルアップだけを無効にすると、D+とD−がともに約3Vから0Vに低下した。無通電時の抵抗測定は D+–D− 13MΩ、D+–GND 8.8MΩ、D−–GND 8.9MΩ。低抵抗の恒常短絡は確認されていない。本実験ではプルアップを再度ONにして電圧が再上昇するかを確認する。

## 条件・注意

- **PD充電器とPC通信用USBを同時に接続しない。** USBデータ線の物理的分離確認済みでも、この実験中はPC側を外す。
- VDD33が約3.3V、PD側VBUSが約6.97Vで安定していることを確認。異常時は直ちに中止。
- 本実験は診断専用。USB CDCの復旧アルゴリズムではない。USBPD/CCおよびPD Policy Engineは変更しない。
- 電圧計はD+、D−と基板GND間。端子短絡に注意。

## フェーズ（PD接続検出から）

| フェーズ | 時間 | USBFS | D+プルアップ | PB12 LED | 記録ラベル |
|---|---|---|---|---|---|
| A | 0〜30秒 | ON | ON | 約2Hz | A / B |
| B | 30〜60秒 | ON | OFF | 約5Hz | U / C |
| C | 60〜90秒 | ON | 再びON（Aの設定を復元） | 約1Hz | N / F |
| D | 90秒〜 | OFF | OFF | 約0.5Hz | O / D |

LEDは点滅周期で識別する。PD接続時刻はファームウェアのCC attach検出時刻であり、充電器挿入の物理時刻と少しずれる。

## ビルドと書き込み（Windows PowerShell）

リポジトリルート `C:\pj\pd_pps\src\pd_pps` に配置： `firmware\experiments\37_pd_usbfs_pullup_toggle`。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\37_pd_usbfs_pullup_toggle\Test-M37-Host.ps1"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\37_pd_usbfs_pullup_toggle\Build-M37.ps1"
```

成功後にPD充電器とPC通信用USBを外し、M36の復旧用BIN保管を確認してWCH-LinkE経由で書き込む。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\dev-workspace\pd-pps.ps1" -Action Flash -Project "firmware\experiments\37_pd_usbfs_pullup_toggle"
```

## 実測とログ

PC通信ケーブルを外したままPD充電器だけを接続し、各フェーズで D+/D−/VDD33/VBUS を測定する。D終了後にPD充電器を外し、基板の給電が維持されUSBFS復帰を確認してからPCを接続。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\37_pd_usbfs_pullup_toggle\Read-M37-Diag.ps1" -Port COM10
```

`RUDIAG`、`RUDIAG S0`、`RUDIAG R0` ... でスナップショットを取得する。完全実施時は9件：A、B、U、C、N、F、O、D、R。

**重要**：M37はUSBFSのD+プルアップを無効化してもUSBFSとUSBPDの相互影響を確定できるものではない。ONへの再変更で両端子が再上昇すれば可逆性の確認となり、原因究明には追加の端子・PHY調査が必要。
