# M7 USB CDC計測コマンド 実機検証記録

<!-- M7_MEASURE_PROTOCOL_VERIFIED_2026-11-09 -->

## 1. 対象と検証範囲

- 実施日：2026-11-09（JST）
- ボード：WeActStudio CH32X035F8U6 CoreBoard
- 実験：`firmware/experiments/20_usb_cdc_measure`（M7）
- ホスト：Windows 11、USB CDC-ACM仮想COM9
- Git基準：M6を検証済みとして保存したコミット`043194a`から新規作成
- 検証範囲：USB経由のASCIIコマンド解析・応答、`MEASURE`の計測不能通知、繰り返し通信

**これは「計測コマンド通信」の検証であり、電圧・電流の実測ではない。** 現状の`m7_measurement.c`はセンサー未構成を返し、数値を生成しない。M7独立のBuild・Flashの実行ログは本記録には含まれないが、実機が`m7_usb_cdc_measure protocol=2`と応答したことは確認済み。

## 2. 基本コマンドの実機結果

実行：`Test-M7-CdcMeasure.ps1 -Port COM9`

| 項目 | 結果・応答 | 判定 |
|---|---|---|
| INFO | `INFO board=CH32X035F8U6 firmware=m7_usb_cdc_measure protocol=2` | PASS |
| STATUS | `STATUS rx_packets=... rx_bytes=... tx_packets=... tx_bytes=... rx_overflows=0 errors=...` | PASS |
| MEASURE | `MEASURE status=unavailable reason=not_configured voltage_mV=NA current_mA=NA` | PASS |
| PING | `PONG` | PASS |
| HELP | `HELP INFO STATUS MEASURE PING HELP` | PASS |
| UNKNOWN | `ERR unknown_command` | PASS |
| 40文字の入力 | `ERR line_too_long` | PASS |
| 追加PING20回 | 全20回`PONG` | PASS |
| 追加MEASURE10回 | 全10回`unavailable` | PASS |

## 3. 10回連続試験

USBケーブルを抜かずに同じテストを10回連続実行し、**10/10回PASS**。1回の要求数は基本7件（INFO・STATUS・MEASURE・PING・HELP・UNKNOWN・長い入力）＋追加PING20件＋追加MEASURE10件＝37件。

| 指標 | 結果 |
|---|---:|
| テスト回数 | **10回すべてPASS** |
| コマンド要求総数 | **370件** |
| PING要求（基本1回＋追加20回） | **210件** |
| MEASURE要求（基本1回＋追加10回） | **110件** |
| 意図的な不正入力 | **20件** |
| 受信リングオーバーフロー | **全10回のSTATUSで0** |
| 応答不一致・タイムアウト | **ログ上0件** |

各回のSTATUSスナップショットは次のとおり。STATUSはその回の不正入力テストを行う前に取得されるため、`errors`には主にそれ以前の不正入力が反映される。

| 回 | rx_packets | rx_bytes | tx_packets | tx_bytes | rx_overflows | errors |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 39 | 271 | 50 | 1232 | 0 | 2 |
| 2 | 76 | 530 | 99 | 2406 | 0 | 4 |
| 3 | 113 | 789 | 148 | 3580 | 0 | 6 |
| 4 | 150 | 1048 | 197 | 4756 | 0 | 8 |
| 5 | 187 | 1307 | 246 | 5933 | 0 | 10 |
| 6 | 224 | 1566 | 295 | 7111 | 0 | 12 |
| 7 | 261 | 1825 | 344 | 8289 | 0 | 14 |
| 8 | 298 | 2084 | 393 | 9467 | 0 | 16 |
| 9 | 335 | 2343 | 442 | 10645 | 0 | 18 |
| 10 | 372 | 2602 | 491 | 11824 | 0 | 20 |

`errors`はM7の`command_errors`を表し、未知コマンドや行長超過の拒否で増える。**USB転送エラー20件を意味しない。** `STATUS`の数値は起動後の累計であり、テスト1回の通信総量と同義ではない。特に最終STATUSは第10回の途中で取得され、その後のコマンドのカウントを含まない。

## 4. 成功条件と未検証事項

確認済み：Windows 11でCOM9を開き、コマンドと応答の一致、計測不能を明確に表すプロトコル、10回の連続実行を検証できた。

未実装・未検証：

- INA226等を使った物理電圧・電流の取得、センサーの配線・校正・シャント値と測定範囲
- PD電源との同時接続、PD/PPS制御との統合
- USB切断・再接続、長時間耐久、過負荷、診断計測あり／なしの比較
- PC接続側VBUS検出、USB D+/D−共有配線の分離・保護、製品用VID/PID
- 旧M5で観測したEP3送信待ち固着の根本原因の究明

## 5. 次の開発

1. M7を実験20としてコミットし、今回検証した通信仕様を保存する。
2. M8では先に**計測対象、測定点、電源・GND構成、センサーの電圧・電流範囲、シャント抵抗・校正条件**を確定する。
3. 計測バックエンドに実センサーを実装し、センサー未接続／通信失敗／範囲外を明示的なエラーとして返す。
4. 実測値が得られた段階で、PD/PPS制御機能と統合する。
