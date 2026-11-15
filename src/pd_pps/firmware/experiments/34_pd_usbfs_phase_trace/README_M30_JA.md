# M30 USBFS状態診断（PD17/M28/M29から独立した実験）

## 目的
PD充電器接続中にCOM10がUnknownになり、LEDは点滅し続ける現象を観測します。M29からコピーした診断用ファームウェアです。**PD制御・USB復旧動作は変更していません。**

## 変更内容
- PB12 LED点滅（250ms反転）維持。GPIOBクロック有効化の修正済み。
- RAMにUSBFSレジスタ・USB設定状態・PD状態のスナップショット12件を保持します。起動時1件、PD接続開始時1件、接続中250ms間隔、切断時1件（最後のスロット確保）。**先頭の最大約2.5秒と切断直後の観測**です。長時間運転全体の記録ではありません。
- USB IRQ入口カウンター、およびUSBバスリセットIRQカウンターを保持。
- USB CDCが復帰したあと `DIAG` と `DIAG 0` ～ `DIAG 11` で結果を取得できます。
- USB CDCの既存コマンド、VID/PID/シリアル番号はM28を維持しています。`INFO`表示もM28のままです。

## コマンド
- `DIAG`: `n`=保存件数、`irq`=USB割り込み入口数、`rst`=USBバスリセットIRQ数、`attached`=現在のCC接続状況。
- `DIAG N`: `i`=サンプル番号、`t`=起動後ms、`a`=CC接続状態、`pe`=Policy Engine状態の列挙値、`c`=USB Configuration、`b`=USBFS BASE_CTRL、`u`=USBFS UDEV_CTRL、`e`=USBFS INT_EN、`f`=AFIO CTLR。レジスタは16進表示です。
- すべて**63バイト以下の単一LF終端応答**。既存 `SET_`/`OUTPUT_` コマンド拒否は維持します。

## Windows PowerShell
リポジトリルート `C:\pj\pd_pps\src\pd_pps` で実行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File '.\firmware\experiments\30_pd_usbfs_state_diagnostic\Test-M30-Host.ps1'
powershell.exe -NoProfile -ExecutionPolicy Bypass -File '.\firmware\experiments\30_pd_usbfs_state_diagnostic\Build-M30.ps1'
```

**ビルドPASSでも直ちにFlashしないでください。** PD充電器を外し、PC USB-Cを抜き、M29のBINを保全した上でWCH-LinkEを使用します。

## 制約
- 1セッション・最大12件、RAMのみ。マイコン再起動で消失します。
- PD切断後の既存同期トレース出力がUSB CDCに影響する可能性があります。
- USBFS IRQ内の診断処理はカウンター1回の増分だけです。
- `DIAG` はUSB復帰後にしか取得できません。復帰しない場合はRAMの直接読み出し等の別手段が必要です。
- ホストテストは実機タイミング・安全性を保証しません。

## PD切断後の診断読み出し
COMポートが戻ったら、PowerShellで以下を実行します。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File '.\firmware\experiments\30_pd_usbfs_state_diagnostic\Read-M30-Diag.ps1' -Port COM10
```

USB通信が一時停止していた時間を含むログがRAM内に残るのは、**CH32X035が再起動していない場合のみ**です。`DIAG n` と `DIAG N` の結果をそのまま保存してください。診断中のPD充電器への再接続は行わないでください。
