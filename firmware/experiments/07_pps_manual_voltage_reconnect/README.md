# 07_pps_manual_voltage_reconnect

正常動作を確認した`07_pps_manual_voltage`を基に、充電器の抜去・再接続時の自動復旧を追加した**実機未検証のテスト候補**です。元のディレクトリ・ファームウェアを上書きしません。

## 変更内容

- CCコンパレータでUSB-C充電器の抜去を判定します（150ms継続して非検出のとき）。
- PD/PPSのネゴシエーション失敗やタイムアウトでも停止せず、自動再試行します。
- 再接続時はUSBPD内部状態とBMC受信機を再初期化し、再びPDO/APDOを確認します。
- 接続が成功すると、最後に入力したPPS電圧を再要求します（初期値は7.00V）。
- 設定値が新しい充電器のAPDO範囲外ならPPSを要求せず、固定5V契約のまま待ちます。
- 未接続中に電圧入力した場合は設定値を保存し、再接続後に適用します。
- `status`と`retry`コマンドを追加しました。
- マイコンの文字エコー処理は**追加していません**。動作確認済みのUbuntu側`minichlink_local_echo.py`を使用します。

### USB-C関連のICピン番号（CH32X035F8U6/QFN20）

| 信号 | 論理名 | IC物理ピン番号 | 用途 |
|---|---|---:|---|
| CC1 | PC14 | 19 | Type-C接続状態・PD通信 |
| CC2 | PC15 | 20 | Type-C接続状態・PD通信 |
| デバッグDIO | PC18 | 14 | WCH-LinkEコンソール |
| デバッグDCK | PC19 | 16 | WCH-LinkEコンソール |
| LED | PB12 | 15 | PPS確立時点灯 |

## 配置・ビルド

ZIPを`~/Downloads/07_pps_manual_voltage_reconnect.zip`に置いた場合：

```bash
cd ~/pj/pps
unzip ~/Downloads/07_pps_manual_voltage_reconnect.zip -d .
cd 07_pps_manual_voltage_reconnect
make test
make build
# 充電器を外した状態で、既存のWCH-LinkE給電構成を確認してから：
make flash
```

コンソール（正常動作確認済みのPC側エコー方式）：

```bash
python3 ../minichlink_local_echo.py ../ch32fun/minichlink/minichlink -T
```

## 動作確認手順

1. WCH-LinkEを接続してコンソールを起動し、`PD: waiting for charger`を確認。
2. 20W USB-C充電器を接続する。`PD: cable detected`、`PD: negotiate=OK`、`PPS: restore PS_RDY`を確認。
3. `7.02`を入力し、`PPS: set PS_RDY at requested 7020 mV`を確認。FNB-C2で電圧を測る。
4. 充電器だけ抜く（WCH-LinkE・コンソールは接続したまま）。`PD: cable detached`を確認。
5. 充電器を再接続。自動的にネゴシエーションして、**7.02Vを再要求**することを確認。
6. 何度か繰り返し、FNB-C2の点滅・PDタイムアウト・エラーが起きないか確認。
7. 充電器を先に接続した状態でコンソールを起動するケースも別途試す。このケースは充電器が`Source_Capabilities`を再送しない場合、自動再試行だけでは復帰できないことがあります。必要なら一度抜き差ししてください。

## コンソールコマンド

| 入力 | 機能 |
|---|---|
| `7.00` | 7.00V要求 |
| `7.02` | 7.02V要求（20mVステップ） |
| `7020` | 7020mV要求 |
| `status` / `s` | 設定値・PD状態・接続状態 |
| `retry` / `r` | PD部分だけ再初期化して再試行 |
| `help` / `?` | コマンドの説明 |

## 構造と制約

- 追加関数`USBPD_ActiveCCPresent()`は確立済みCC1/CC2のコンパレータを**読むだけ**です。通信中にCC_SELを書き換えるとPD受信が乱れるため、接続確立後はCC切り替えを行いません。CC1: PC14（IC19番）、CC2: PC15（IC20番）。
- CCコンパレータの実際のしきい値と安定性は、実機で検証する必要があります。今回、抜去を検出できない・誤検出する場合は、コンパレータ値を調べて調整してください。
- 一度のPD交渉につき、Source Capabilitiesの受信を最大20秒待ちます。失敗してもプログラムは停止しません。物理的に充電器が接続済みのままSource Capabilitiesを再送しない場合は、抜き差しが必要になる場合があります。
- PDネゴシエーションが終わった段階では固定5V契約です。その後にPPS要求を送り、PS_RDYを受信すると指定電圧に切り替わります。
- PD要求電流は1.00A固定です。これは電子負荷に1Aが流れるという意味ではありません。
- `PS_RDY`は電圧実測ではありません。FNB-C2やDMMで実電圧を確認してください。
- **外部負荷抵抗は接続せず**、まず無負荷で自動復旧を評価してください。実機が意図せず再起動・異常発熱した場合は直ちに中止してください。
- USB側からの3.3V給電とWCH-LinkE側の3.3V給電の競合に注意してください。これまで正常に動いている接続方法を変えないでください。
- 基盤となった`USBPD_SinkNegotiate()`実装は固定PDOへの初期交渉を実行します。再試行で接続検出中にタイミングを逃す場合があります。

## 検証状況

- `make test`：ホスト環境で入力値の解析とCC抜去デバウンスを検証。
- CH32X035実機用クロスコンパイル：**未実施**（この環境にch32fun/クロスコンパイラがないため）。
- 実機：**未検証**。元の安定版は保存しておいてください。

参考：WCH CH32X035RM 21章（USBPD CC comparator / transceiver）。
