# 07_pps_manual_voltage

USB PD/PPSの設定電圧を、Ubuntu 24.04の`minichlink -T`コンソールで手入力して変更する実験です。

## デバイス
- WeActStudio CH32X035F8U6 CoreBoard
- WCH-LinkE（既存のSWIO接続を使用、新規UART配線なし）
- USB PD/PPS充電器（20Wまたは45W）
- FNB-C2などのUSB電圧テスター（出力電圧の実測用）
- 外付け負荷抵抗なしで最初の実験を実施
- ステータスLED：PB12（IC物理ピン15）

## 特徴
- 7.00Vから開始
- コンソール入力を1文字ずつ表示（エコーバック）。Backspaceで修正可能
- 20mV単位のPPS電圧指定（例：`7.02`、`7.04`、`7020`）
- 充電器が通知したAPDOの範囲外を拒否
- 複数APDOを持つ充電器の場合は指定電圧に合うAPDOを自動選択
- PPS要求電流上限は1.00A固定（無負荷で1A流れるという意味ではない）
- `PS_RDY`を待って要求成功を出力（**実測電圧ではない**）
- 3秒ごとにPPS要求を更新
- 元の`06_pps_current_limit`、`../ch32fun/`は変更しない

## インストール

更新版`07_pps_manual_voltage_echo.zip`を`~/Downloads`に置いたと仮定します。既存ファイルは上書きされます。

```bash
cd ~/pj/pps
unzip -o ~/Downloads/07_pps_manual_voltage_echo.zip -d .
cd 07_pps_manual_voltage
make test
make
# 書き込み時は充電器と外部負荷を取り外し、安全な給電状態を確保
make flash
# 充電器、FNB-C2、WCH-LinkEを既存の実績ある構成に接続
../ch32fun/minichlink/minichlink -T
```

## コンソール操作

起動時は7.00Vです。

```text
BOOT: CH32X035 PPS manual voltage console
PD: PDO[3] PPS 5000-11000 mV, max 1800 mA
PPS: start PS_RDY at requested 7000 mV
V> 7.02
PPS: set 7020 mV / 1000 mA, send=OK, state=Waiting for Accept
PPS: set PS_RDY at requested 7020 mV
V> 7.10
...
V> 11.00
...
```

数字を入力すると、`V> 7.02`のようにリアルタイムに表示されます。Backspace/Deleteで1文字ずつ取り消せます。Enterで実行します。`s`/`status`で状態表示、`?`/`help`で使い方を表示します。
`7.01`（10mVきざみ）、`12.00`（20W充電器のPPS範囲外）などは拒否します。

**注意：上記ログは動作イメージで、実機測定結果ではありません。**

## 既知の制約・切り分け

1. `minichlink -T`のコンソール入力はch32funの`poll_input()`/`handle_debug_input()`で受けます。
2. ch32fun/minichlinkのバージョンやプログラマの種類によって、入力が届かない／重複する報告があります。出力だけ表示される場合は、まず`s`＋Enterで反応を確認してください。未動作の場合はUART経由方式を検討します。
3. 3秒ごとのPPS更新は成功時にログを出さず、入力途中の表示を妨げません。異常時はエラーを表示して停止します。
4. `WaitForDebuggerToAttach(0)`でコンソール接続を待つため、端末を接続するまでPD処理を開始しません。
5. `PS_RDY`はPDネゴシエーション完了です。20mV刻みの実電圧精度を意味しません。FNB-C2・DMMで独立測定します。
6. 旧版のビルド・書き込み・電圧変更動作はユーザーが確認済みです。今回追加した入力エコー機能の実機動作は未確認です。添付の`make test`は入力値パーサーとエコー編集のホスト検証です。

## ファイル
- `pps_manual_voltage.c` : マイコンファームウェア
- `usbpd_current.h` : 06で使用したローカル拡張PD/PPSライブラリ
- `console_parse.h` : 数値パーサー
- `console_input.h` : 入力編集・エコーバック処理（入力コールバック内では`printf`しない）
- `tests/test_input.c` : エコー・Backspace・CRLFのホストテスト
- `tests/test_parse.c` : ホスト側パーサーテスト
- `Makefile`, `funconfig.h`
