# PD17 M13g — PPS REJECT/WAITと状態遷移の安全停止ガード

更新日：2026-11-08

## 目的・対象

M13f（Git `c48234e`、UGREEN/ダイソーの通常PPS短時間回帰PASS）を基準に、
**PPS更新時のREJECT/WAIT反復と、Request未送信時に到着する応答**を調べる。

- 対象：`src/pd_policy.c`、`src/pd_policy.h`と、そのPolicy Engineを使用するホスト試験。
- 既存のPPS要求は7V/1A、更新起点は`PS_RDY`、周期5,000ms、WAIT後の待機は120ms。
- PHY/`main.c`/`Makefile`/本番故障注入フックは変更しない。
- 本文の上限値は**本試作の安全側の設計判断**であり、USB PD規格適合性を示さない。
- Source発Hard Reset実機、ADC故障注入実機、長時間連続運転は引き続き保留する。

## M13f旧版で再現した2件の問題

### G-01：契約中のPPS更新がREJECTされた後のRequest連続送信

通常Readyで5,000ms経過しPPS Requestを送信後、Sourceから`REJECT`を受信した場合：

1. 旧PEは既存契約を保持して`PE_READY`に戻る。
2. `last_pps_ms`は前回の`PS_RDY`時刻のままなので、次の`pe_tick()`で再び5,000ms経過と判定する。
3. **待機なしに新たなRequestを生成する。** 旧実装で`REJECT`直後の次tickに`command=PE_CMD_REQUEST`となることを再現した。

M13gは、**有効契約中の新しいRequestがREJECTされたら直ちに`PE_RECOVERY`へ安全停止**する。
以降、CC物理切断・再接続まではRequestを生成しない（Hard Reset復旧の例外経路は既存と同じ）。

### G-02：Request未送信中のWAIT/REJECTが未送信コマンドを残す

`PE_WAIT_ACCEPT`でも、`request_sent=false`でまだPHYが送信を開始していない状態がある。
旧実装は、この時にWAIT/REJECTが届いても状態だけをReady/WaitCapsへ変更し、
`PE_CMD_REQUEST`を未送信のまま残す。旧実装のホストテストで再現。

M13gは**対応するRequest未送信ならWAIT/REJECTを現AMSの正当な応答として扱わず、安全停止**する。
このため`command=PE_CMD_NONE`となり、停止後のPD送信を防止する。

## WAIT応答の有限上限

- 有効なPPS契約がある更新中、WAITは最大**2回まで継続**を許容し、各回120ms後に再試行する。
- **連続3回目のWAITで安全停止**する。すなわち3回目のWAIT後に4回目のRequestを送信しない。
- 計数は**PS_RDYで再びReadyとなった時**にゼロに戻す。WAIT受信と新しいSourceCapsではリセットしない。
- `PE_EV_DETACH`、ハードウェア異常による契約失効、Hard Resetでもカウンタを破棄する。
- 初回の明示的契約がまだないWAIT/REJECTは従来どおり、新しいSourceCaps待機（650ms期限）後に停止する。ただし**未送信Requestへの応答は直ちに停止**。

通常動作でWAIT/REJECTがない場合、メッセージ内容とPPS更新周期には変更を加えない。

## ホスト試験と検証済み範囲

`tests/run_m13g_host_test.sh`をPOSIXシェル＋ホストCコンパイラー環境で実行。

| 領域 | 検証内容 | 結果 |
| --- | --- | --- |
| 状態遷移 ST-01～ST-02 | 未送信RequestへのWAIT/REJECTで安全停止 | PASS |
| ST-03～ST-04 | PPS更新REJECT、既存固定契約からの再交渉REJECT停止 | PASS |
| ST-05～ST-07 | 連続WAIT上限、成功後のリセット、32bit折り返し | PASS |
| ST-08～ST-11 | READY中の無関係応答、早期/遅延ACCEPT、停止後の遅延応答 | PASS |
| ST-12～ST-16 | CC再接続、初回WAIT/REJECT、Caps置換/重複AMS | PASS |
| ST-17～ST-18 | ADC故障・受信Soft ResetによるWAIT待機破棄 | PASS |
| PHY結合 IT-01 | 未送信RequestにWAIT→PE停止→PHYの送信/GoodCRC禁止→CC再接続 | PASS |
| PHY結合 IT-02 | PPS更新REJECT→PE停止→PHYの送信/GoodCRC禁止→CC再接続 | PASS |

M13g **20/20ホストケースPASS**。既存M9～M13fのホストシェル17系統とM13d Python 12件もPASS。
Clang UndefinedBehaviorSanitizerは追加テストでPASS。

**実機でREJECT/WAITを任意発生させて検証したわけではない。** UGREEN/ダイソーのM13g短時間通常PPS回帰は後続試験でPASS。ただしM13gのREJECT/WAIT異常応答に対する実機安全停止を証明するものではない。

## 実機Build・短時間通常回帰の実施結果（2026-11-08）

未接続状態の`PD17: alive, charger detached, state=0, lost=0`を確認した。M13gのBuild/Flashコンソール全文は今回の提供ログには含まれていないため、ビルド成功や書込時のFLASH/RAM使用量は独立には確認していない。以後の通常PPS実機ログはM13gを書き込んで行った試験として報告されたものである。

| 観測項目 | UGREEN 90551 USB-C1 | ダイソー GaN 20W |
| --- | ---: | ---: |
| セッション時間 | 26,977ms | 42,843ms |
| Ready時間 | 26,348ms（97.7%） | 41,962ms（97.9%） |
| PPS更新完了／開始 | 5/5 | 8/8 |
| Requests／PPS Ready | 6／6 | 9／9 |
| メインループ最大間隔 | 3ms | 1ms |
| ADC校正後／INA226 | 7.033V／6.996V | 6.997V／6.960V |
| `PD17 STOP` | reason=1、CC切断127ms前 | reason=1、CC切断123ms前 |
| `source_hr`／`adc_fault` | 0／0 | 0／0 |
| ADCタイムアウト／TRACE・イベント欠落 | 0／0・0 | 0／0・0 |

UGREENの`PD17 STABILITY`は`session_ms=26977 ready_ms=26348 refresh_started=5 refresh_done=5`、`PD17 STOP`は`first_rel_attach_ms=26850 detach_after_stop_ms=127`。ダイソーは`session_ms=42843 ready_ms=41962 refresh_started=8 refresh_done=8`、`first_rel_attach_ms=42720 detach_after_stop_ms=123`で、いずれも時刻整合を確認した。

**判定：通常PPS 7V/1A運用、周期更新、切断時の停止診断の短時間実機回帰は2機種PASS。** ダイソーはTRACE全体ではなく統計・診断の抜粋に基づく。M13gで追加したREJECT/WAITに対する安全停止処理はホスト試験のみであり、実機での異常応答注入は未実施。Source発Hard Reset、ADC故障注入、連続運転も保留。

**新しい停止理由の分類コードは追加していない。** WAIT/REJECTに起因するPEの異常停止は従来の`PD17 STOP reason=9`で記録され、`aux`のイベント値（REJECT=5、WAIT=6）により区別可能。これらは実機で未確認。

## 残課題

- Source発Hard Reset、実機ADC故障注入、長時間連続運転はユーザー方針で保留。
- 任意のREJECT/WAITを出力できるSource試験環境がないため、M13gガードの**故障時実機テストは未実施**。
- 連続WAIT回数3という上限は試作機のフェイルクローズ方針であり、規格上の最大待ち回数として主張しない。
- 既存のPPS更新・Fixed5Vフォールバック以外の製品機能を拡大しない。
