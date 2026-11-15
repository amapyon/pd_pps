# 実験19：USB CDC-ACM コマンド通信（M6）

対象：CH32X035F8U6 / WeActStudio CoreBoard / Windows 11。2026-11-09作成。

## 目的

実機で基本通信・反復試験を確認した実験18（M5）を変更せず、新しい実験19で**PCからの行単位の問い合わせ**を実装する。
現段階では**PD/PPS交渉、電圧・電流設定、INA226/OLED等との連携は行わない**。

M5診断版のUSBFS CDC制御要求、EP2受信、EP3送信（最大63バイトの短いINパケット、割り込み保護付き `USBFS_SendEndpoint()`）と診断カウンターを引き継ぐ。

## 通信プロトコル V1

- Windowsの仮想COMポート：115200 8N1（CDC上の慣例設定。物理UARTではない）
- 送信：**ASCIIのコマンド + LF**。CRLFも可。大文字で完全一致。
- 応答：ASCIIの**1行 + LF**。
- コマンド最大31文字。超過時 `ERR line_too_long`。非印字ASCIIを含む場合 `ERR invalid_character`。
- 不明なコマンド：`ERR unknown_command`。
- PING: `PONG`
- HELP: `HELP INFO STATUS PING HELP`
- INFO: `INFO board=CH32X035F8U6 firmware=m6_usb_cdc_commands protocol=1`
- STATUS: `STATUS rx_packets=<数> rx_bytes=<数> tx_packets=<数> tx_bytes=<数> rx_overflows=<数> errors=<数>`

`STATUS`の値はMCU起動以降の診断カウンターで、応答作成時のスナップショット。`tx_packets`はUSB IN送信の受理回数（応答の行数ではない）。カウンターの一貫した同時刻性は保証しない。

## 制限事項

- コマンドは1件ずつリクエスト・応答を行う。連続大量送信時のRXリング（実効255バイト）溢れはカウントするが、再送制御は未実装。
- 応答バッファ192バイト。最大63バイトのINパケットに分割して送信。正常なUSBホスト接続とEP3完了を前提とする。
- USB抜き差し・ホスト切断時の復旧や長時間耐久は未検証。
- `usb_config.h`のVID/PID・記述子は実験18の試験構成を継承。製品用のVID/PID選定・ライセンスは未検討。
- 別電源給電。PC側VBUS 5Vは電源に直結せず、PD側USB-C J1は未接続。J1とPC側USBのD+/D−は同一ネットなので同時接続しないこと。VBUS検出未実装。

## Build / Flash（実験18を上書きしない）

```powershell
Set-Location 'C:\pj\pd_pps\src\pd_pps'
$Project = 'firmware/experiments/19_usb_cdc_commands'
.\dev-workspace\pd-pps.ps1 -Action Build -Project $Project
# Build成功と内容を確認した後、WCH-LinkEで実機に上書きFlash
.\dev-workspace\pd-pps.ps1 -Action Flash -Project $Project
```

`-Action Flash`は接続されているMCUのファームウェアを上書きする。Git上の実験18はそのまま残る。

USB通信用ケーブルを抜き差ししてCOM番号を確認し、テストする。

```powershell
Get-PnpDevice -PresentOnly -Class Ports |
    Format-Table FriendlyName, Status, InstanceId -AutoSize
.\firmware\experiments\19_usb_cdc_commands\Test-M6-CdcCommands.ps1 -Port COM9
```

## ホストCテスト（Windows実機Buildとは別）

```bash
cd firmware/experiments/19_usb_cdc_commands
cc -std=c11 -Wall -Wextra -I. host_test/test_commands.c -o /tmp/m6_host_test
/tmp/m6_host_test
```

## 作業ツリー保護

実験16bの未コミット2ファイル、実験18の`host_test/mock_usb.h`未コミット変更、未追跡ROADMAP、ch32fun、`dev-workspace`、`.vscode/tasks.json`は変更しない。検証済みコードのコミットでは実験19のファイルだけを明示的にステージする。関連しない未コミット変更とバックアップ類はコミットしない。

## 実機検証結果（2026-11-09）

<!-- M6_COMMANDS_VERIFIED_2026-11-09 -->

- 実機：WeActStudio CH32X035F8U6、Windows 11、WCH-LinkE、PC通信用USB-C経由のCOM9。PD側J1未接続。
- `dev-workspace/pd-pps.ps1 -Action Flash`によるBuildとFlashを確認。FLASH **4,416 B / 62 KB（6.96%）**、RAM **996 B / 20 KB（4.86%）**。
- PowerShellテストの構文誤り `"PING failed at iteration $i: [$p]"` は `"PING failed at iteration ${i}: [$p]"` に修正。ファームウェアの再書込みは不要だった。
- `INFO`、`STATUS`、`PING`、`HELP`、不明コマンド応答、長過ぎるコマンド応答、20回連続PINGの全項目がPASS。
- 上記のスクリプトを**USBケーブルを抜かず10回連続実行し、全10回PASS**。各回26要求、合計260要求（うちPINGは210回：単独PING 10回＋連続PING 200回）。
- `STATUS`の`errors`は各回2増加し、試験内の意図的な不正入力（未知コマンド・40文字入力）それぞれ1件と一致。**通信エラー20件ではない**。`rx_overflows=0`が全10回継続。
- これらは所定条件におけるコマンド・応答テストの成功であり、USB再接続、長時間稼働、過負荷、診断計測を外した版、PD/PPSとの同時動作の保証ではない。

詳細は [`docs/M6_USB_CDC_COMMANDS_VERIFICATION.md`](../../../docs/M6_USB_CDC_COMMANDS_VERIFICATION.md) を参照。

### 検証の再実行

```powershell
Set-Location 'C:\pj\pd_pps\src\pd_pps'
$test = '.\firmware\experiments\19_usb_cdc_commands\Test-M6-CdcCommands.ps1'
for ($i = 1; $i -le 10; $i++) {
    Write-Host "===== M6 Test $i / 10 ====="
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File $test -Port COM9
    if ($LASTEXITCODE -ne 0) { throw "M6 test failed at iteration ${i}" }
}
```
