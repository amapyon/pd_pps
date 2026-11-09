# M6 USB CDC-ACMコマンド通信 実機検証記録

<!-- M6_COMMANDS_VERIFIED_2026-11-09 -->

## 1. 検証の対象

- 実施日：2026-11-09（JST）
- 対象：`firmware/experiments/19_usb_cdc_commands`（M6、CH32X035F8U6 / WeActStudio CoreBoard）
- ホスト：Windows 11、USB CDC-ACM仮想COM9、WCH-LinkE（Flash）
- Git基準：実験18/M5の検証記録を含む`55406f1`。実験19はこの後に追加した。
- 接続条件：PC用USB-CでCDC通信を実施。PD側J1は未接続。PC側VBUSをMCU電源に直結しない構成。
- 範囲：PCからASCIIコマンドをLF終端で送信し、改行終端の応答を照合。PD/PPS制御・電気計測は実施していない。

## 2. Build / Flash

Windows 11の既存ワークスペーススクリプトを使用し、次の結果を確認。

| 項目 | 実測 | 判定 |
|---|---:|---|
| `-Action Flash`内のBuild | `Build succeeded.` | PASS |
| FLASH使用量 | 4,416 B / 62 KB（6.96%） | PASS |
| RAM使用量 | 996 B / 20 KB（4.86%） | PASS |
| CH32X035の検出 | `Detected CH32X035` | PASS |
| Flash | `Image written.`、`Flash succeeded.` | PASS |

## 3. コマンド単体試験

`Test-M6-CdcCommands.ps1 -Port COM9` の実行結果。最初の実行時にはPowerShell文字列内の`$i:`がパーサーエラーとなったため、`${i}:`に修正し、**再Build・再Flashなし**で再試験した。修正後のテストは以下すべてPASS。

| 項目 | 応答・検証内容 | 判定 |
|---|---|---|
| INFO | `INFO board=CH32X035F8U6 firmware=m6_usb_cdc_commands protocol=1` | PASS |
| STATUS | `STATUS rx_packets=... rx_bytes=... tx_packets=... tx_bytes=... rx_overflows=... errors=...` | PASS |
| PING | `PONG` | PASS |
| HELP | `HELP INFO STATUS PING HELP` | PASS |
| 未知のコマンド | `ERR unknown_command` | PASS |
| 40文字のコマンド | `ERR line_too_long` | PASS |
| 20回連続PING | 20回すべて`PONG` | PASS |

## 4. USBケーブル抜き差しなしの10回連続試験

同じテストスクリプトを**10回連続で実行し、10回すべてPASS**。毎回、INFO、STATUS、PING、HELP、未知コマンド、長いコマンド、20回PINGを検証した。

| 集計項目 | 実測・算定 |
|---|---:|
| 試験回数 | 10回すべてPASS |
| 1回のコマンド要求数 | 26件（通常4件 + 不正2件 + 追加PING20件） |
| 総コマンド要求数 | **260件** |
| PINGの合計 | **210回** |
| 意図的な不正入力 | **20件**（1回あたり2件） |
| `rx_overflows` | **全10回で0** |
| 応答不一致・タイムアウト | **試験ログ上0件** |

テスト中の`STATUS`出力は次のとおり。各回の`STATUS`は、その回の未知コマンド・長いコマンド試験**より前**に取得されている。

| 回 | rx_packets | rx_bytes | tx_packets | tx_bytes | rx_overflows | errors |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 28 | 183 | 30 | 379 | 0 | 2 |
| 2 | 54 | 354 | 58 | 698 | 0 | 4 |
| 3 | 80 | 525 | 86 | 1017 | 0 | 6 |
| 4 | 106 | 696 | 114 | 1337 | 0 | 8 |
| 5 | 132 | 867 | 142 | 1659 | 0 | 10 |
| 6 | 158 | 1038 | 170 | 1982 | 0 | 12 |
| 7 | 184 | 1209 | 198 | 2306 | 0 | 14 |
| 8 | 210 | 1380 | 226 | 2630 | 0 | 16 |
| 9 | 236 | 1551 | 254 | 2954 | 0 | 18 |
| 10 | 262 | 1722 | 282 | 3278 | 0 | 20 |

**`errors`の意味：** M6の実装では、未知コマンド、行長超過、無効文字で`command_errors`を増分する。今回は各テスト回で未知コマンドと長いコマンドを1件ずつ意図的に送信するため、`errors`は毎回2増えて期待どおり。USBの転送エラー数ではない。

**注意：** `STATUS`のカウンターはMCU起動以降の累計。表の初回値には直前の試験が含まれる。`rx_packets`はコマンド数でなくUSB OUTパケット数、`tx_packets`もUSB IN送信受理回数であり、コマンド件数と直接一致するものではない。

## 5. 確認できた範囲と残課題

今回実証したのはWindows 11のCOM9を通じたコマンド解析、応答生成、20回連続PINGを含む10回繰り返し試験が成功したことである。USB CDC・コマンド層が設計上の全条件で安定していることを証明するものではない。

- 未検証：長時間稼働、USBの抜き差し・自動再列挙と再オープン、ホスト切断中の復帰、過負荷、連続ストリーム処理。
- 未検証：診断計測を除去したファームウェアでのA/B比較。M5でのEP3送信待ち固着の根本原因は未特定。
- 未実装：PD/PPS交渉との統合、INA226/ADCによる実測値取得、PPS設定コマンド。
- ハードウェア課題：PD側J1とPC側USBコネクターのD+/D−共有、PC側VBUS検出、製品向けUSB VID/PIDの選定など。

## 6. 次に進める作業

1. 実験19を検証済みコマンド層としてコミットし、現状のソース・テストを保存する。
2. 継続性確認のため、USB再接続・エラーパス・長時間テストの優先順位を決める。
3. 別実験で、`MEASURE`などの計測情報取得を追加する。まず値の出所・単位・有効性を定義し、実センサー未接続時には架空の計測値を返さない。
4. PD/PPS制御と統合する前に、電源系とUSBデータ線の独立性・保護・同時接続条件を設計する。
