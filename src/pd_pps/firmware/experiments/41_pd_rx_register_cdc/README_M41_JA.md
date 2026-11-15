# M41: PD Request後のGoodCRC受信経路の診断版

## 目的

M40で観測された Request → TX_END → RETRY ×2 → TX_FAILED → Hard Reset について、CH32X035 USBPDレジスタ・受信割り込み・DMAヘッダー・CRCReceiveTimerを観測する。

**注意**：M41は診断実験版であり、USB PD準拠や実機の安全動作を保証するものではない。初回テストは電流制限・電圧監視下で実施。DNの既存加工を維持し、追加のPCB加工は不要。PC用USB-CのVBUSとPD側VBUSを接続しない。

## M40からの主な変更

- 旧イベントコード1〜28、CDC `PING`以外のコマンド仕様、PPS設定、PE状態遷移、再送回数・待ちタイムアウト、VBUS/ADC安全監視は維持。
- `PDTR_M41_IRQ_REG` など6種類の**観測用**イベント29〜34を追加。
- USBPD_IRQHandlerでR8_STATUS / R8_CONTROL / R16_CONFIG / R16_BMC_BYTE_CNTを読み取りトレースに保存。
- IF_RX_ACT時、GoodCRC送信によってDMAが上書きされる**前**に受信ヘッダー・バイト数・BMC_AUXを記録。
- TX_END後のRXモード復帰状態、CRC待ちタイムアウト時のレジスタ・期待MessageID・経過サイクルを保存。
- ISR/タイムアウト処理にログ追加による少量の時間的摂動あり。トレースの有無で結果が変わり得る。
- CDCの`PDTRACE n`、`PDTINFO`をM40から継承。USB IDは`1209:D035`、BCDリビジョン`0x0041`、シリアル`041`。

## コード表（追加分）

| code | name | detail | aux | value |
|---|---|---|---|---|
| 29 | M41_IRQ_REG | BMC_BYTE_CNT下位8bit | R8_STATUS | CONTROL(8bit)<<16 \| CONFIG(16bit) |
| 30 | M41_RX_DMA | BMC_AUX | R8_STATUS | BMC_BYTE_CNT(9bit)<<16 \| DMAヘッダー(16bit) |
| 31 | M41_RX_ARM | BMC_BYTE_CNT下位8bit | RXモード復帰直後STATUS(R8) | CONTROL(8bit)<<16 \| CONFIG(16bit) |
| 32 | M41_CRC_TIMEOUT | 再送回数 | STATUS(R8) | CONTROL(8bit)<<16 \| CONFIG(16bit) |
| 33 | M41_CRC_CTX | 期待MessageID | 現TX MessageID + CRC待機フラグ | TX_ENDからの経過SysTickサイクル |
| 34 | M41_RX_GATED | 1=VBUS障害、2=Hard Resetゲート | STATUS | 受信バイト数 |

> STATUSはR8_STATUS（0x40027009）の値で、bit7..2にIF_*フラグ、bit1..0にBMC_AUXがあります。AUXは`STATUS & BMC_AUX_MASK`と照合してください。CONTROL(R8)のbit0がPD_TX_EN、bit[4:2]がRX_STATEです。

## 設置

ZIP内の`41_pd_rx_register_cdc`を`firmware\experiments`直下に配置する。

PowerShell実行は必ず指定の形式で行う。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\41_pd_rx_register_cdc\Test-M41-Host.ps1"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\41_pd_rx_register_cdc\Build-M41.ps1"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\dev-workspace\pd-pps.ps1" -Action Flash -Project "firmware\experiments\41_pd_rx_register_cdc"
```

RISC-Vビルド完了後、実機へ書き込む。書き込み前はPD充電器・PC通信用USBを外す。COM番号は新しいシリアル`041`に割り当てられたものを確認する。

```powershell
Get-CimInstance Win32_SerialPort | Select-Object DeviceID, Name, PNPDeviceID
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\41_pd_rx_register_cdc\Read-M41-Cdc.ps1" -Port COM14
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\41_pd_rx_register_cdc\Capture-M41-Trace.ps1" -Port COM14 -WaitSeconds 45 -IntervalMs 300 -LogPath ".\M41_trace.txt"
python ".\firmware\experiments\41_pd_rx_register_cdc\Decode-M41-Trace.py" ".\M41_trace.txt" --out ".\M41_trace_decoded.txt"
```

`COM14`は仮の例であり、Windowsに認識されたCOMポート番号へ置き換える。

Captureを開始してからPD充電器を接続し、`PDSTOP found=1`の後も**ケーブルを抜かない**こと。RAMの127件FIFOはPDケーブル切断後のprintfデバッグドレインにより消費される。`PDTINFO dropped=0`を確認する。必要に応じて`Capture-M41-Trace.ps1 -Immediate`で接続中のスナップショットを即取得できる。

## 判断の目安

- `code=29` IRQ_STATUSのIF_RX_ACTが立つか（STATUS bit5）、BMC_AUXはSOP0か。
- `code=30` GoodCRCのヘッダーがDMAに存在するか（`type=1`, `ndo=0`, `mid=期待MessageID`）。
- `code=31` TX_END後にCONTROLのPD_TX_ENが0へ戻っているか。
- `code=32/33` Timeout時もRX待ちか、期待MessageIDと経過時間は適切か。
- `code=34` 受信がソフトウェア側ゲートで除外されていないか。

デコードツールは48MHzのSysTickクロックを**仮定**して経過時間を表示。RISC-Vクロスビルドと実機PD挙動は利用者の環境で検証が必要。
