# M5 USB CDC-ACM エコーバック実験（CH32X035）

最終更新: 2026-11-08 / PD PPS Explorer / 実験18

## 状態

**実機でUSB CDC-ACMの基本通信を確認済み。** Windows 11で`USB シリアル デバイス (COM9)`として認識され、`Test-M5-CdcEcho.ps1`が`PASS: 37 bytes echo-matched via COM9`を出力した。

この実験の範囲は、USB列挙・CDC制御転送・受信したバイト列のエコーバックまで。**M5全体やPD/PPSとの統合が完了したわけではない。** 長時間通信やUSB再接続、PC側VBUS検出、製品適合性は未検証。

詳細な検証記録: [`docs/M5_USB_CDC_ACM_VERIFICATION.md`](../../../docs/M5_USB_CDC_ACM_VERIFICATION.md)

## 配線・電源

| PC側追加USB-C基板 | CoreBoard |
|---|---|
| D+ | TP1 / PC17（USB_DP） |
| D− | TP2 / PC16（USB_DN） |
| GND | GND |
| CC1、CC2 | 各5.1kΩをGNDへ（追加コネクタ基板側） |
| PC側VBUS 5V | MCU給電には使用しない。検出機能は未実装 |

CoreBoardは適切な**別電源**で給電する。電源端子、電圧、極性、レギュレータへの逆流を事前に確認すること。PD側USB-C J1は**未接続**とする。J1のUSBデータ線とTP1/TP2は接続されているため、PDとPCの同時接続は禁止（未検証）。

## 構成

- `m5_usb_cdc_echo.c`：USBFS CDCクラス要求とFIFO付きエコー処理
- `usb_config.h`：M5ローカルのUSBFS/CDC設定（ch32funサンプルから導入）
- `funconfig.h`：CH32X035向け設定。サンプル側の`funconfig.h`とは分離
- `Makefile`：既存`dev-workspace/pd-pps.ps1`からBuild/Flash可能
- `Test-M5-CdcEcho.ps1`：37バイトエコー検証
- `Test-M5-CdcStress.ps1`：短いパケットの境界値・繰り返し・COM再オープン検証（追加テスト、**実機未実施**）
- `host_test/`：エコー・CDC要求処理のCホストテスト

## Build・Flash・通信確認

PowerShellで**Gitリポジトリ直下**へ移動して実行する。

```powershell
Set-Location 'C:\pj\pd_pps\src\pd_pps'
$Project = 'firmware/experiments/18_usb_cdc_acm_echo'

# Build
.\dev-workspace\pd-pps.ps1 -Action Build -Project $Project

# Build後に必要に応じてFlash（Build＋書き込み）
.\dev-workspace\pd-pps.ps1 -Action Flash -Project $Project

# PC用USBを接続した後、COM番号を確認
Get-PnpDevice -PresentOnly -Class Ports |
    Format-Table FriendlyName, Status, InstanceId -AutoSize

# 基本エコー
.\firmware\experiments\18_usb_cdc_acm_echo\Test-M5-CdcEcho.ps1 -Port COM9

# 次の安定性試験（未検証。COM番号は変更すること）
.\firmware\experiments\18_usb_cdc_acm_echo\Test-M5-CdcStress.ps1 -Port COM9 -Rounds 10 -ReopenCycles 3
```

`-Action Log`は従来のログ機能であり、USB CDCのエコーバック検証は上記の個別スクリプトを使う。VS CodeのM5選択肢は今回の更新対象ではない（`.vscode/tasks.json`未変更）。

## 確認した実績と制約

- 初期USBFS設定の修正後のBuildはFLASH 3,172 B（5.00%）、RAM 708 B（3.46%）。この数値は**最後のCDC制御要求修正前**の記録。
- WCH-LinkE/minichlinkによるBuild＋Flash成功ログを確認。
- Windows 11でCOM9（`VID_1209&PID_D035`）として列挙された。
- CDC要求を修正後、COM9で37バイトの送受信が一致した。
- 最終CDC修正後のBuild／Flashの詳細ログ、連続通信・USB再接続の検証結果は未収集。
- `VID_1209&PID_D035`は試験時の観測値であり、製品としてのVID/PID選定・使用権は未検討。

## 保護事項

実験16bの未コミット2ファイル、未追跡ROADMAP、既存実験01〜17を変更しない。`git add -A`、`git clean -fd`、`git reset --hard`は使用しない。
