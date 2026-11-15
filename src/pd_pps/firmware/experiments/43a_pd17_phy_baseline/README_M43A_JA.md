# M43A — 実績のある PD17/M13b PHY を用いた比較実験

**目的**：M42でCC1/CC2双方に現れる「初回RequestのTX_END後、GoodCRC受信できずTX_FAILED」を、
従来のPHY実装に戻すことで切り分ける。**自動復旧や送信タイムアウトを改造しない。**

## 根拠と移植範囲

- 出典：`pd_pps.zip` の `firmware/experiments/17_pd_spr_sink_rebuild/src/pd_phy_ch32x035.c`（M13b）。
  `docs/M13B_NORMAL_SINK.md`では、2026-11-07の実機通常系でUGREEN（CC2）とダイソー（CC1）
  のPPS 7V/1A、周期更新、INA226測定がPASSと記録されている。ただし、この実機成績は**基板改造前の条件**。
- `src/pd_phy_ch32x035.c`の本体は、上記**M13b版をバイト単位でそのまま移植**。
- 末尾に限り、M42が提供した`phy_trace_snapshot`を継続（CDCから診断ログを読むための読み取り専用関数）。
  `USBPD_IRQHandler()`を含む既存処理は改変していない。
- M41で追加されたIRQのレジスタ採取、RX DMAヘッダー採取、TX_END直後のレジスタ採取、
  timeout時の追加`trace_add()`、RXゲートログは**削除**。従来のCC attach / RX GoodCRC /
  retry / TX failed / PEイベントの基本トレースは残る。
- `pd_policy.c`、PPS Request生成、再送上限、タイムアウト、保護・ADC・INA226のソースはM42と同一。
- USB CDCコマンドはM42を継承。`PING`は`PONG M43A`、USBシリアルは`043`。
  **前のCOM15は変化する可能性がある。**
- M42のソースとファームは上書きしない。M43Aは独立フォルダ。

## 検証できること・できないこと

M43Aで成功率が改善すれば、M41追加のUSBPD割り込み内ロギングによる実時間への影響が候補。
ただし、**新しいUSBデバイス識別子・バイナリ配置・診断ログ密度の差**も変わるため、
成功しただけでロギングが原因と断定してはいけない。

M43Aでも失敗すれば、M41 ISR診断のみが原因という仮説は弱まる。
ただし、M42 CDCとの共存、ケーブル・充電器・基板改造の影響、PD PHY基本実装の問題は残る。

**注意**：M43AにはM41のcode29〜34の詳細レジスタ記録は出ない。基本トレースcode1〜28は継続。

## Windows 11 手順

作業ディレクトリ：`C:\pj\pd_pps\src\pd_pps`。
ZIPの`43a_pd17_phy_baseline`を`firmware\experiments`の下に配置する。

**1. ローカルテスト（書込みなし）**

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\43a_pd17_phy_baseline\Test-M43A-Host.ps1"
```

**2. ビルド（書込みなし）**

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\43a_pd17_phy_baseline\Build-M43A.ps1"
```

CH32X035用のビルドに成功したことをログで確認するまでFlashしない。
**3. 実機書込み（ビルド成功、通電・配線を再確認した後）**

- PD充電器、PC用CDC-USBを外し、WCH-LinkEから**3.3Vを給電しない**構成を確認。
- 同じPDケーブルと充電器で比較。外部負荷は接続しない。
- 電源構成に不明点があれば書込前に解消する。別のUSB-Cケーブルに交換する場合は
  J1側D+/D−とPC USBデータ線の電気的競合がないことを確認する（D+・D−の切断状態は未確定）。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\dev-workspace\pd-pps.ps1" -Action Flash -Project "firmware\experiments\43a_pd17_phy_baseline"
```

**4. COM番号を再確認**

```powershell
Get-CimInstance Win32_SerialPort | Select-Object DeviceID,Name,PNPDeviceID
```

USBシリアルは`043`であり、以前の`042`と別デバイスになる。

**5. PD充電器を外した状態で開始する新規Attach取得**（例：COM16は各自の環境で置換）

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\43a_pd17_phy_baseline\Read-M43A-Cdc.ps1" -Port COM16
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\43a_pd17_phy_baseline\Capture-M43A-Session.ps1" -Port COM16 -WaitSeconds 45 -IntervalMs 300 -LogPath ".\M43A_run01.txt"
```

`Monitoring for a NEW PD attachment`を確認してPD充電器をJ1に接続する。成功した場合、
30秒程度維持して、別ウィンドウで`-Immediate`により現在値を保存する。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\43a_pd17_phy_baseline\Capture-M43A-Session.ps1" -Port COM16 -Immediate -LogPath ".\M43A_success_live.txt"
```

**6. 判定**

- `STAT a=1 pe=4 pps=1 mv=7000 ma=1000`：PPS Ready。
- `PDSTAB ready>0 pps>=1 ref_done`：Ready期間とPPS更新成功状況を確認。
- `PDSTOP reason=4`、`TX_RETRY` x2、`TX_FAILED`：M42と同じGoodCRC未確認失敗。
- `VBINFO`、`INA`：計測系が継続できているか確認。電圧はDMMで検証。
- 適切な比較のため、成功率を**同一ケーブル、同一充電器、同一向きで各3回**記録。
  接続のたびに物理切断し、`a=0`確認後にキャプチャを開始。
- M42に戻したい場合は保存済みM42ビルドを再書込み。**M43AとM42を混同しない**。

## ソース整合性

`PD17_BASELINE_SHA256.txt`に3版のソースハッシュを記録した。`tests/test_m38_structure.py`が
M13b PHY本体のSHA-256一致とM41 IRQログ不在を検査する。

## 残る既知のリスク

- J1 D+/D−切断箇所の測定結果が食い違っている。未知のUSBデータ線競合を安全と見なさない。
- USB PD CTSや外部プロトコルアナライザでの測定は行っていない。
- **ここで作成したバイナリは未Flash、実機での動作は未確認**。

## 修正履歴（2026-11-15）

初版パッケージの `m43a_pd17_phy_baseline.c` に `#include "src/main.c"` がなく、リンク時に `undefined reference to main` が発生する問題を修正。M42のビルド方式に合わせた、エントリーポイントの追加のみ。PHY/PD/CDCの処理内容には変更なし。

**修正版ZIPで既存のM43Aフォルダを上書きしてから、ホストテストとビルドを再実行する。** 本修正についてCH32X035向けビルド・Flashは未確認。
