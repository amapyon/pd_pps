# M12a — CRCReceiveTimerタイマー起点修正（単一変更）

対象: `17_pd_spr_sink_rebuild/src/pd_phy_ch32x035.c` のみ。
これまで成功したM11bのGet_Source_Cap単発試験を保持し、GoodCRCの再送タイマー起点のみ修正。

## 根拠

USB PD 3.2 Rev 3.2 v1.2 §7.31.1: CRCReceiveTimerはMessage EOPの最終ビットをPHYが送信した後に開始。Table 7.9: `tReceive` 900〜1100µs、典型1000µs。

M11b実機観測: Request → TX_END 633µs、TX_END → 1回目の再送 334µs (48 MHz換算)。この初回再送は早すぎる。

旧コードは `retry_started_ms=now_ms` を送信開始前に設定し、1ms量子化タイマーが2ms進むと再送する実装だった。

## 修正内容

- CRCReceiveTimerはIF_TX_ENDで起動 (SysTick CNTL 32bit を取得)。
- 1000µs (48MHzなら48000tick) 経過後に未ACKなら再送。再送フレームのIF_TX_ENDで再び起動。
- GoodCRC成功、Hard Reset、Detach、TX_FAILEDではタイマーを無効化。
- 既存のMessageID、TX再送上限2回、Policy Engine、Get_Source_Cap単発試験は変更しない。
- 監視は既存RAM traceだけ。接続中 `printf` はしない。

## 未確認・制限

- CH32X035 IF_TX_ENDがEOP最終ビット送信完了と厳密に一致するか、IRQ遅延がどの程度かは実機評価が必要。
- 正式な `tRetry` 制約と所要時間全体のUSB PD適合性は未検証。
- ホストGCCの厳格な構文チェックをPASS。USBPDレジスタ模擬テストでTX_END前・期限前に再送せず、TX_END後1000µsで再送することをPASS。これは実機検証ではない。Windows/xPack Build/Flashおよび実機結果は未検証。

## 最初の1操作（安全確認）

USB-Cを外し、LogをCtrl+Cで終了。ZIPをrepo直下へ展開してBuild/Flash/Logし、heartbeatを確認。次の回にUSB-C接続試験。

```powershell
Set-Location 'C:\pj\pd_pps\src\pd_pps'
Expand-Archive -LiteralPath 'C:\pj\pd_pps\PD17_M12a_crc_timer_tx_end.zip' -DestinationPath . -Force
.\dev-workspace\pd-pps.ps1 -Action Build -Project '17_pd_spr_sink_rebuild'
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
.\dev-workspace\pd-pps.ps1 -Action Flash -Project '17_pd_spr_sink_rebuild'
if ($LASTEXITCODE -ne 0) { throw 'Flash failed' }
.\dev-workspace\pd-pps.ps1 -Action Log
```
