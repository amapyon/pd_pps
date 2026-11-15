# M42 — VBUS ADC履歴＋INA226キャッシュ診断 / M41を継承

## 目的
M41で再現した初回PD RequestのGoodCRC受信失敗（`reason=4`、接続から約313ms）を、
VBUS/ADC波形の低下と区別するための診断版です。M41のPD送受信、再送回数、タイムアウト、
USBPD割り込み、安全停止処理は一切変更していません。

M41がすでに10ms間隔で取得するADC履歴から、最初の停止イベント時点に直近40サンプルを
前景ループで固定保存します。新たなADC変換を開始しません。保存時のコピー処理は少量ですが
前景ループの実行時間が増えるため、計測干渉が完全にゼロとは言い切れません。

## 回路（電源OFFで確認）
- PD充電器 → WeActの標準USB-C J1（CC1/CC2、VBUS、GNDを保持）
- J1 D− → MCU PC16/DNは切断済み。J1 D+ → MCU PC17はまだ接続されたまま
- PC → 追加USB-C → D+はPC17、D−はPC16、GNDは共有。PC側VBUSをWeActに結線しない
- VBUS → 100kΩ → PA0 → 10kΩ → GND （5V→PA0約0.455V、7V→約0.636V）
- INA226：3.3V電源、PA2=SCL、PA3=SDA、I²Cプルアップは3.3V、GND共通。
  **R010(10mΩ)はVBUSと負荷＋の間に直列**（IN+がSource側、IN−が負荷側）。
  INA226のVBS電圧測定点は負荷側プラス。R010をVBUSとGNDの間に置かないこと。
- 負荷の場所や接続は実機で確認すること。R010の電流値は通過電流に限る。

## 推奨する測定順序
1. **すべて無通電**で、INA226のR010を含む負荷系がVBUSとGNDの直接短絡になっていないことを確認。負荷は最初は外す。PC側USB-CのVBUSはWeActに結線しない。
2. **M41のままでも可能な予備試験**：通常5V給電時に、VBUS–GNDが約5V、PA0–GNDが約0.455Vであることをテスターで確認。異常なら書換え前に回路を修正。
3. M42を書き込み、負荷を外したままPDの初回交渉ログを取得。その際、メーターを接続しておく場合もINA226モジュールのGND/ロジック電源・I2Cレベルは3.3Vとする。
4. 低電流の負荷を戻し、同じ充電器・同じケーブル・同じ向きで比較。電圧低下、`reason`、`PDTRACE`、`VBLOG`を比較する。
5. `VBLOG`が正常な約5Vを示す一方で`reason=4`が繰り返されるなら、電源低下ではなくGoodCRC送受信・CC信号品質・PHYのタイミング問題を優先する。充電器やケーブルを変更した対照試験が次候補。

## 新規CDCコマンド
- `VBINFO`：10ms ADCリングのサンプル数・最終raw/非校正VBUS電圧推定値・停止時固定保存数・ADC異常
- `VBLOG <0..39>`：最初の停止時点に固定保存したサンプル（`ms`はMCU起動からの絶対ミリ秒）
- `INA`：既存のREADY 300ms後の一回限り測定値キャッシュを取得。
  `sampled=0`はREADYが十分持続していないなど、INA226読み出し未実行を示し、センサー故障ではありません。
  **CDCコマンドからI²C転送・ADC変換を開始しません**。

既存コマンド PING, STAT, COUNT, DELTA, PDSTOP, PDSTAB, PDERR, PDTINFO, PDTRACE はそのままです。

## インストール
ZIP内 `42_pd_vbus_ina_cdc` を `firmware/experiments` 下へコピー。端末は
`C:\pj\pd_pps\src\pd_pps` で実行します。全PowerShellスクリプトは `-File` で実行してください。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\42_pd_vbus_ina_cdc\Test-M42-Host.ps1"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\42_pd_vbus_ina_cdc\Build-M42.ps1"
```

ビルド成功後、PD充電器とPC通信用USBを取り外しWCH-LinkEで書き込みます。
```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\dev-workspace\pd-pps.ps1" -Action Flash -Project "firmware\experiments\42_pd_vbus_ina_cdc"
```

USB serial `042` （COM番号はWindows上で確認）。
```powershell
Get-CimInstance Win32_SerialPort | Select-Object DeviceID,Name,PNPDeviceID
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\42_pd_vbus_ina_cdc\Read-M42-Cdc.ps1" -Port COM15
```
`COM15`は例です。実際に認識されたCOM番号を使用してください。

未接続状態でログ取得開始し、実行中にPD充電器を挿します。
```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\42_pd_vbus_ina_cdc\Capture-M42-Session.ps1" -Port COM15 -WaitSeconds 45 -IntervalMs 300 -LogPath ".\M42_session.txt"
python ".\firmware\experiments\42_pd_vbus_ina_cdc\Decode-M42-Trace.py" ".\M42_session.txt" --out ".\M42_decoded.txt"
```

ログ取得終了までPD充電器を外さないでください。トレースは物理切断後約500msからデバッグ出力に消費されます。

## 値の読み方 / 限界
ADC前提：VREF=3.3V（仮定値）、12bit、分圧1/11。5Vならraw約564、7Vなら約790。
計算による非校正値であり、マルチメータの電圧保証値ではありません。
`VBLOG flags` のビット0: CC attached / 1: PE READY / 2: Hard Reset。
VBLOGのmsは接続からの経過時間ではなく起動時刻基準。隣接サンプルの差で相対時刻を確認します。
`INA sampled=0` は約313msで停止するケースでは自然（READY後300msの測定条件未到達）。
I2Cの電圧・電流は追加負荷の位置と測定方向で変化します。外部負荷を外した対照試験を優先。

## 確認状況
ホスト側ユニットテストおよびソースガードを実行。現環境にはRISC-Vコンパイラと実機がなく、
ターゲットビルド/実機テストは未実施です。M41の元ファイルは改変していません。
