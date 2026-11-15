# M40：USB CDCからPD PHY時系列トレースを取得する診断版

## 目的と構成

M39で記録された `PDSTOP reason=4 ms=309`, `PDERR hr=1`,
`PDSTAB caps=1 req=1 ready=0` の原因を、PD PHYのイベント時系列から分析する。
**PD交渉ロジック、送受信タイミング、Hard Reset復旧、安全監視は変更しない。**

- M39を複製し、新規 `40_pd_phy_trace_cdc` として配置。M39を上書きしない。
- PD PHYファイルへの変更は末尾に追加した **読み取り専用スナップショット関数1個だけ**。
  元の全関数と状態機械、ISRはM39とバイト単位で一致。既存リングの値を**消費しない**。
- USBFS ISR内にPD関数呼び出し・同期出力を追加しない。
- VID:PID `1209:D035`、USB Serial `040`、Product `PD PPS M40 PD Trace CDC`。
- 変更したファイルは `src/pd_phy_ch32x035.c/.h`、`src/m38_usb.c`、`src/main.c`のコメント、`usb_config.h`、`Makefile`、Windowsスクリプトとテスト。

## CDC診断コマンド（読み取り専用）

M39の `PING STAT COUNT DELTA PDSTOP PDSTAB PDERR` を継続。

- `PING`：`PONG M40`
- `PDTINFO`：`count=<0..127> capacity=127 dropped=<n> attached=<0|1>`
- `PDTRACE n`：保存済みのn番目（**0始まり、最大126**）のイベントを返す

例：

```text
PDTINFO count=18 capacity=127 dropped=0 attached=1
PDTRACE idx=0 count=18 tick=12345678 code=1 detail=1 aux=0041 value=00000000
PDTRACE idx=1 count=18 tick=12345789 code=7 detail=30 aux=1a41 value=00000001
```

`tick` はSysTickの下位32bit、**48MHz動作なら約89秒周期で周回**。
近いイベントの差分はwrapを考慮して `((tick2-tick1) & 0xffffffff) / 48000` [ms]。
`code`→イベント名は `PDTRACE_codes.csv` で確認。
特に `code=12` TX_COMMAND、`code=13` TX_RETRY、`code=14` TX_FAILED、
`code=10` RX_GOODCRC、`code=3` IRQ_RESET、`code=15` PE_EVENTを確認する。
`RX_GOODCRC` の `value` は **下位8bitが期待MessageID、bit8が一致判定**。
`TX_COMMAND` の `detail` はコマンド番号、`aux` は生PDヘッダ、`value` はRequestのRDO。

### 計測上の制約（重要）

1. トレースのリングは**127件固定、先頭記録保持方式**。満杯になると新規イベントを捨て、`dropped`が増える。
2. `PDTRACE n` は読み取り専用で、USB CDC側から取り出しても既存のデバッグ出力用記録を消費しない。
3. **PDケーブルを物理的に抜くと、500ms後から従来の`printf`処理が記録を排出する。**
   必ずPDケーブルを挿したまま、異常発生後すぐ`Capture-M40-Trace.ps1`で保存する。
4. 同じPDケーブルを挿したまま失敗状態になっても、PEの異常復旧状態へ移行するだけであり、観測用コマンドはPD送信を要求しない。
5. この仕組みはIC内RAMに残ったイベントを読むものであり、電源OFF、リセット、USBケーブル抜去によるMCU再起動で消える。
6. `PDTINFO`→`PDTRACE n` の間に接続切断・リング排出が起きるとインデックスが変わる。スクリプトは`none=1`を検知する。
7. `code=3`はPD PHYのReset IRQ記録であり、USBFSのバスリセット数`COUNT rst`とは別。

## セットアップ

`40_pd_phy_trace_cdc`フォルダを`C:\pj\pd_pps\src\pd_pps\firmware\experiments\`以下に展開する。
PD充電器とPC通信USBを外した状態で、従来のWCH-LinkEから書き込む。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\40_pd_phy_trace_cdc\Test-M40-Host.ps1"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\40_pd_phy_trace_cdc\Build-M40.ps1"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\dev-workspace\pd-pps.ps1" -Action Flash -Project "firmware\experiments\40_pd_phy_trace_cdc"
```

USB Serialは`040`、COM番号はWindows側で実際に確認する（COM12とは限らない）。

```powershell
Get-CimInstance Win32_SerialPort | Select-Object DeviceID,Name,PNPDeviceID
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\40_pd_phy_trace_cdc\Read-M40-Cdc.ps1" -Port COM13
```

## 失敗時の時系列取得（推奨）

**PD充電器を外した状態でスクリプトを開始し、表示に従い接続**。自動的に
`PDSTOP found=1` または `STAT pe=6` を検知するとトレース全件を取得する。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\40_pd_phy_trace_cdc\Capture-M40-Trace.ps1" -Port COM13 -WaitSeconds 40 -IntervalMs 300 -LogPath ".\M40_trace.txt"
```

**失敗がすでに発生していてまだPDケーブルを挿したままの場合**は`-Immediate`でその時点のトレースを保存する。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\40_pd_phy_trace_cdc\Capture-M40-Trace.ps1" -Port COM13 -Immediate -LogPath ".\M40_trace.txt"
```

ログは文字列・HEXのまま保存される。`M40_trace.txt`をChatGPTへアップロードする。
`dropped>0`なら失敗前後の新しいイベントがバッファに入り切らなかった可能性を示す。

## 注意

- 現在のDN切断状態を変更しない。PD側のD+はPC側USBFSに残っているため恒久的な絶縁ではない。
- PC側VBUSとPD側VBUSは結線しない。
- 連続Hard ResetやVBUS断続が生じたら、測定を短時間に限定し発熱・異常電圧時は中止。
- このコードは規格適合や電源安全性を保証しない実験用。
- Linux上でのホストテスト実施可。Windows側RISC-Vコンパイルと実機書き込みは別途必要。
