# 08_pps_manual_voltage_current_reconnect_v2

CH32X035F8U6 (WeActStudio) + WCH-LinkE を使用した USB PD/PPS の電圧・要求電流の手動設定プログラム。

- ベース: `07_pps_manual_voltage_reconnect_v3` の接続／再接続処理を継承した `08_pps_manual_voltage_current_reconnect_v1`。
- 変更点: **Source_Capabilities 内の PPS APDO から電圧範囲・最大電流を毎回読み取り、入力を照合する**。固定PDOの能力とは混同しない。
- USB-Cの抜き差し検出、再接続後の電圧・電流要求値の復元、3秒ごとのPPS更新、`caps`コマンド等は維持。
- WCH-LinkE側ローカルエコーは従来の `minichlink_local_echo.py` を使用。MCU側にエコー機能は追加しない。

## 接続中の充電器に合わせた範囲

UGREEN 200Wの **今回ユーザーが取得した** Source_Capabilities:

```
PD: Source PDO count=6
PD: PDO[0] raw=0x0801912C FIXED 5000 mV / 3000 mA
PD: PDO[1] raw=0x0002D12C FIXED 9000 mV / 3000 mA
PD: PDO[2] raw=0x0003C12C FIXED 12000 mV / 3000 mA
PD: PDO[3] raw=0x0004B12C FIXED 15000 mV / 3000 mA
PD: PDO[4] raw=0x00064145 FIXED 20000 mV / 3250 mA
PD: PDO[5] raw=0xC1A4213C PPS 3300-21000 mV, max 3000 mA
```

v2では `PDO[5]` から **3.30～21.00V（20mV刻み）／最大要求3.00A（50mA刻み）**を判定する。PPS RDOで表現できる電流の最小正値は50mA。ただし、この最小値は充電器が通知した最小供給電流ではなく、**PPS RDOのエンコード上の最小値**。実際の負荷がその設定を超えると出力停止・PD再接続が起こり得る。抵抗到着前の試験はこれまでの1A以上を推奨。

FIXED 20V/3.25A はPPSの要求最大3.00Aを3.25Aに引き上げない。充電器のポート使用状況・接続状態でSource_Capabilitiesが変わる場合、その時点の通知に従う。

複数のPPS APDOがある場合、**電圧・電流の組が単一のAPDOに適合すること**を検証。無条件に電圧最大値と電流最大値を組み合わせて許可しない。範囲外は `ERROR` を出して要求値を変えず、**自動的な丸め込み・クランプは行わない**。

`limits` は現在の充電器のPPS APDOごとの範囲と最大要求電流を表示する。`caps` は固定PDOを含む全PDOを表示する。充電器接続前は `limits` に未取得と表示される。充電前に入力した要求値は仮保存し、接続完了時にAPDO検証される。接続先の充電器が保存値を満たさない場合は固定契約のままとなり、`limits` を見て有効な設定値を入力する。

## コマンド

| コマンド | 例 | 機能 |
|---|---|---|
| `l` / `limits` | `limits` | 取得済みPPS APDOの電圧・最大電流を表示 |
| `c` / `caps` | `caps` | 取得済み全PDOの生値と解釈を表示 |
| `s` / `status` | `s` | 設定値、PD状態、PPS限界を表示 |
| `7000` / `7.00` | `7020` | 電圧のみ設定（従来互換） |
| `v <value>` | `v 3.30` | 電圧のみ設定 |
| `i <value>` | `i 1.50` | PPS要求電流のみ設定 |
| `set <V> <I>` | `set 8.00 2.50` | 電圧・電流を同一PPS Requestで変更 |
| `<mV> <mA>` | `3300 1000` | 同時設定の短縮形 |
| `r` / `retry` | `r` | PD通信を再開し保存目標値を再検証 |
| `help` / `?` | `?` | ヘルプ表示 |

小数点付きはV/A、整数が100以下の場合もV/A、101以上の場合はmV/mA。初期値は7.00V / 1.00A。

`i 3.05`（UGREENのPPS最大3Aを超える）および `v 21.02` はエラー。`v 3.30` と `i 3.00` はこのUGREENのAPDO範囲内だが、接続ハードウェアの定格と負荷条件を確認するまでは極端な設定は避ける。

**要求電流と実電流は別**。`KWS-X1` の `PPS 07.00V 01.50A` は要求したPPS動作電流値、`I:0.078A` は実際の負荷電流。要求値を変えても抵抗負荷なしでは実電流はほぼ変化しない。

## Ubuntu / ch32fun

既存の `~/pj/pps` 配下で展開。`../ch32fun/ch32fun` と `../minichlink_local_echo.py` を利用する。

```bash
cd ~/pj/pps
unzip ~/Downloads/08_pps_manual_voltage_current_reconnect_v2.zip
cd 08_pps_manual_voltage_current_reconnect_v2
make test
make clean
make build
# 充電器と負荷を外してから書き込み
make flash
python3 ../minichlink_local_echo.py ../ch32fun/minichlink/minichlink -T
```

## 安全上の注意

- UGREENが21VまでのPPSを広告することは、**WeAct基板上のVBUS経路や接続部品に21Vを印加して安全**という意味ではない。USB-Cポート周辺回路・コンデンサ等の定格を確認するまでは、既に実験済みの7～11V付近に限定する。
- 50～150mAなどの低い要求電流は、現状のWeAct基板 + テスターの約80mAの負荷と余裕が小さいか不足し、PD契約解除・電源断の原因になる。まず1A以上で試す。
- PPS要求電流は設定可能な上限値であり、定電流を厳密に制御する保証ではない。実際の電流制限特性は負荷抵抗到着後に測定する。
- 以前のWCH-LinkE USB切断現象に注意。抜き差しは無負荷・充電器を外した状態で行う。パワー抵抗器の電流をマイコンGPIOピンやブレッドボード電源レールに通さない。

## 使用ピン（論理名 + IC物理番号）

CH32X035F8U6 (QFN20): CC1 PC14（19番）、CC2 PC15（20番）、LED PB12（15番）、WCH-LinkE DIO PC18（14番）、DCK PC19（16番）。

## テスト状況

- ホストC11の単体テスト: コマンド、PPS最小設定値、UGREENの生PDO `0xC1A4213C` のデコード、範囲外拒否、50mA単位RDO、再接続フィルターを確認。
- CH32X035向けクロスコンパイルと実機・KWS-X1の確認: **未実施**。`make build` のログを確認してから使用すること。
