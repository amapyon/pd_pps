# M5-1 読取専用 USB CDC テキストAPI仕様 v0.1（提案・ホスト模擬のみ）

この文書は**製品プロトコルの正式決定ではない**。正式M5の読取API定義フェーズを進めるための、実機USB通信を必要としないホスト上の試験仕様。

## ワイヤ形式（提案）

- USB CDC-ACMによる行単位ASCII、要求はCRLFまたはLF、応答はLF終端。
- 1要求1応答。応答は最大**63バイト（LFを含む）**。64バイトのEP3 Bulk IN単一パケットに収めるための**現試作の実装上の制約**であり、USB規格上の上限ではない。
- コマンド名は大文字。先頭・末尾空白、制御文字、複数コマンド連結、63バイト超を拒否。
- `GET_CAPS`で能力一覧の個数・epochを取得し、`GET_CAP 1`から1件ずつ読む。最大7件（PD17 `pe_caps_t.pdo[7]`）まで。
- 数値は10進、電圧mV、要求電流/測定電流mA。`STATUS`の電流は**要求電流**、`MEASURE`の電流は**測定電流**。
- 応答が63バイトを超える場合、`ERR code=reply_too_long`を返して長い値を途中で切らない。`STATUS v=...` の `v` は契約の有効フラグ。
- 任意の出力設定コマンドは明示的に`ERR code=read_only`。今回PD Policyを操作しない。

## コマンド一覧

| 要求 | 応答例 | 意味 |
| --- | --- | --- |
| `INFO` | `INFO board=CH32X035F8U6 firmware=m5_1_mock protocol=7` | ホスト模擬版識別。**実機FWではない** |
| `HEALTH` | `HEALTH bus_resets=12 tx_timeouts=0 reconnects=0` | USB状態の累積カウンター |
| `GET_CAPS` | `CAPS status=ok count=6 epoch=1` | 新鮮なSource Capsの個数と世代 |
| `GET_CAP 1` | `CAP i=1 type=fixed min=5000 max=5000 ma=3000` | 固定PDOの最大供給能力 |
| `GET_CAP 6` | `CAP i=6 type=pps min=3300 max=21000 ma=3000` | SPR PPS APDOの範囲・上限 |
| `STATUS` | `STATUS state=ready v=1 pps=1 pos=6 mv=7000 ma=1000` | PE状態・契約の妥当性・**要求値** |
| `MEASURE` | `MEASURE status=error reason=sensor_unavailable` | センサーが無効なら数値を返さない |
| `SET_PPS 7000 1000` | `ERR code=read_only` | 非読取操作は拒否 |

`GET_CAPS`の`epoch`はPD17 `pe_t.caps_epoch`に対応する候補。PCは切断後に再接続したら、必ず`STATUS`・`GET_CAPS`を再取得する。`epoch`だけで古いホストキャッシュを再利用してはならない（MCU再起動後に同じ値へ戻る可能性がある）。

`CAP type=unsupported raw=0x........`はBattery/Variable/EPR等の未実装PDO形式。未知能力を固定/PPSと誤分類せず元の32bit値を保存する。

センサー未取得を`voltage_mV=0`と偽装しない。`STATUS v=0`では旧契約の要求値を返さず`pos=0 mv=0 ma=0`とする。

### エラーとプロトコルバージョン

| エラー | 条件 |
| --- | --- |
| `ERR code=bad_line` | 空行・制御文字・LFなし・複数行・長さ超過 |
| `ERR code=bad_arg` | `GET_CAP`引数の構文不正 |
| `ERR code=out_of_range` | 能力インデックスが範囲外 |
| `ERR code=caps_unavailable` | 物理切断や停止などSource Caps未確定 |
| `ERR code=read_only` | 設定変更コマンド |
| `ERR code=unknown_command` | 未対応コマンド |
| `ERR code=reply_too_long` | 想定外の長い応答（内部で切り詰めない） |

この実験の`protocol=7`は**仮の次期プロトコルID**。既存実験24の`protocol=6`に対する後方互換性や切り替え方法は、組み込み実装段階で決める。

## 適用範囲と未実装

- `fixtures/pd17_m13g_pps_ready.json`のPDOは既存Gitの`firmware/experiments/17_pd_spr_sink_rebuild/tests/test_policy.c`にある実際の**PD17ホストテスト用定数**。現場で取得した生ログと同一と断定しない。
- ホスト上でデータのフォーマットとエラー処理をテストするだけであり、USB D線・PD交渉・INA226測定には一切アクセスしない。
- ISRからのPEデータ取得は禁止。将来はメインループ上で一貫したスナップショットを構築し、通信IRQから独立に送信する。
- 次工程では、PD17 `pe_t` の`state`、`contract_valid`、`pps_active`、`pdo_index`、`caps_epoch`、`caps`、`requested_mv`、`requested_ma`と`pd_sensor_reading_t`を結合する。
- 実機PDO/APDO取得、PD/USB同時稼働、USB自動再接続とこの新APIの結合、Python製品CLI、PPS設定変更は未実装。
