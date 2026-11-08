# PD17 M13b — 通常運用向け USB PD/PPS Sink 試作版

本版は `17_pd_spr_sink_rebuild` のM13a-7からの更新で、**CH32X035F8U6 / WeAct CoreBoard向けの実験ファームウェア**です。正式製品版でもUSB-IF準拠認証済みでもありません。`16b_source_caps_integrity` と、M13a-7の配布ZIPは保全してください。

## M13bの特徴

- **SinkからHard Resetを意図的に送信しません**。M13a-5の実験用トリガーを通常のメイン処理とMakefileから削除しました。
- **通常のGet_Source_Cap再要求を停止**。M11bの1秒後再要求と、M13a-7のPPS再取得（1秒後の追加Get_Source_Cap）は、通常運用では行いません。受信したSourceCapsは常に再評価します。
- 初回／再交渉では、提示されるPPS APDOが7V/1Aを満たせば7V/1A、そうでなければ固定5V/1Aを選びます。固定5V/1A自体が提示されなければfail-closedです。
- PPS 7V/1Aの更新Requestは5秒周期です。PD通信中の同期printf、OLED、I²Cは行わず、ログはCC切断後にまとめて出力します。
- SourceからのHard Reset受信を検出した場合、契約失効→PA0の10ms ADCで低電圧3点→5V相当3点→PHY/PE復旧→SourceCaps評価、の保守的な復旧処理を試みます。**1回/物理CC接続だけ**。2回目のHard Reset、ADC異常、時限超過、通信エラーでは停止し、CC物理切断・再接続を待ちます。
- M13bの新規記録では、`auto_recovery=1`は新しい明示的PD契約の成立（固定5Vも含む）、`pps_recovery=1`はPPS Ready復帰を表します。旧M13a-7の `auto_recovery` はPPSのみを意味していたため注意してください。

## 状態

- **M13a-7**：Windows/xPack Build/Flash、UGREEN 90551（Hard Reset後固定5V）、ダイソー GaN 20W（Hard Reset後PPS 7V）、ADC・INA226実機ログを取得済み。
- **M13b**：ソース変更・LinuxホストテストPASS。**Windows/xPack Build/Flashおよび実機通常系・Source起因Hard Reset復旧は未実施**。

## 初回の実機確認（1試験ずつ）

1. 充電器・負荷・USB電力計を外す。WCH-LinkEのみ接続する。
2. `pd-pps.ps1 -Action Build -Project '17_pd_spr_sink_rebuild'` → `Flash` → `Log`。`PD17: alive, charger detached, state=0, lost=0` を確認。
3. **まだ充電器は接続しない**。Build/Flash/heartbeatログをレビューしてから正常系試験へ進む。
4. 正常系試験では、充電器を1台ずつ接続しPPS 7V、周期更新、切断後のログを確認する。`code=19`、`code=20`（SinkのHard Reset送信）が出たら異常として停止する。

通常運用中にSourceからHard Resetが来た場合の自動復旧は、ホストテストのみ確認済みです。Source起因の実機試験は別計画で安全な発生方法を定めてから行います。

詳細：[`docs/M13B_NORMAL_SINK.md`](docs/M13B_NORMAL_SINK.md)
