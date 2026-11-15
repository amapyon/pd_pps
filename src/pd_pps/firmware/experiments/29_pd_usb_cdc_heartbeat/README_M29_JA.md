# M29: PB12生存確認（M28派生・診断専用）

目的：PD充電器接続中にUSB CDC COM10が消える現象で、CH32X035のメインループが進行しているかをUSBに依存せず確認する。

## 変更
- M28をコピーした独立した実験フォルダー。PD17とM28は変更しない。
- `src/main.c` の `m28_usb_init()` 直後にPB12 LED（基板搭載）を設定する。
- メインループ冒頭で、SysTick由来の`now_ms()`を用いて250msごとにLED出力を反転（約2Hz点滅）。`Delay_Ms()`を追加しない。
- USB CDCの実装とVID/PID/シリアル008、`INFO firmware=m28_pd_usb_cdc protocol=7`は**あえてM28のまま**。COM10の再試験手順が変わらないため。ただし診断版はM29である。
- USB割り込みやPD PHYのコードは変更しない。

## Windows 11への配置
ZIP内の`29_pd_usb_cdc_heartbeat`フォルダー全体を既存リポジトリの`firmware/experiments/`直下へコピーする。既存M28へ上書きしない。

```powershell
Set-Location C:\pj\pd_pps\src\pd_pps
.\firmware\experiments\29_pd_usb_cdc_heartbeat\Test-M29-Host.ps1
.\firmware\experiments\29_pd_usb_cdc_heartbeat\Build-M29.ps1
```

生成されるBINは`m29_pd_usb_cdc_heartbeat.bin`。書き込み前に電源配線とM28復旧用BINを保全する。確認後のみ以下を実行。

```powershell
.\dev-workspace\pd-pps.ps1 -Action Flash -Project 'firmware\experiments\29_pd_usb_cdc_heartbeat'
```

PDをつながない状態でLEDが約0.5秒周期で点滅し、COM10と既存CDC試験が動くことを確認する。
PD充電器接続後にCOM10がUnknownに変わっても、LEDが点滅し続けるか観察する。

## 判定
- LED継続 / COM消失: メインループの一部は動作中。USBFS固有問題や再初期化に重点。
- LED停止 / COM消失: メインループの停止や割り込み占有、例外、リセット繰り返しなどを疑う。
- LED再起動のような挙動: 電源瞬断・MCUリセットの切り分けを次に行う。

注意：LED点滅が続いてもPD処理・USBFS ISRの健全性は証明できない。2Hzは観測用であり厳密な測定値ではない。ソフトウェア変更で電源配線の安全性は保証されない。
