# M13d：Source発Hard Reset実機試験計画（安全・再現性重視）

**作成日：2026-11-08**　**対象：PD17 / M13c-2 (`20981cb`)**

## 1. 目的・対象範囲

- 対象は**SourceがHard Resetシグナリングを送信し、SinkであるCH32X035が受信するケース**。SinkからのHard Reset送信 (`code=19/20`) やUSB-Cの抜去では代替できない。
- 今回は**試験方式・手順・ログ判定を準備**する段階。実物のSource発Hard Resetは**未実施**。Sinkファームウェアは現行M13c-2を維持し、まず5V固定PDOで単回復旧を観察する。
- 現行仕様：物理CC接続あたりSource発Hard Resetからの復旧は**1回限り**。同一接続の2回目は意図的にfail-closed。これは製品の完全なUSB PD適合判定ではない。
- 今回のテストは**電力供給や充電器のUSB-IF認証試験ではない**。独立したCC/PD波形確認にはプロトコルアナライザ、VBUS過渡波形確認には適切な計測器が別途必要。

## 2. 事前に用意するSourceの条件

必須条件： (a) USB-C **Source役**であること、(b) 少なくとも固定5Vを供給し、通常のPD通信でSinkをReadyまで進められること、(c) **物理CC接続を維持している状態で、SourceのPEからHard Resetを意図的に1回発生させる操作**があること、(d) 保護されたVBUSスイッチ・放電経路を持つこと。最初の試験ではPPS供給能力は不要。

| 候補 | 利点 | 制約・事前確認 |
|---|---|---|
| **NXP MCUXpresso USB PDデモ＋対応開発基板/USB-C PDシールド** | NXP公開デモに長押しのHard Reset操作が明記され、検証しやすい | ドキュメントの例 `OM13588` は**生産終了**。中古・手持ちで構成できる場合に限る。Source側で発生させられることを単独で事前確認 |
| **ST NUCLEO-G0B1RE＋X-NUCLEO-SRC1M1** | 現行のSource評価ボード。UCPD、ソフトウェアスタック、VBUS/CC保護を活用可能 | 別途5V等の給電系・Sourceファームを構成。**任意のSource発HR送信コマンドは標準サンプルで確認できていない**ため、実装・検証が必要 |
| Source発Hard Resetを明示的に生成できる専用PD Sourceエミュレータ | Source側のメッセージと電圧を制御しやすい | 「PDロガー」「Sinkテスター」「USB電流計」だけでは不足。**Source役と送信トリガの両方を仕様書で確認してから選定** |

参考：
- NXP MCUXpresso：`https://mcuxpresso.nxp.com/mcuxsdk/latest/middleware/usb/docs/MCUXSDKUSBPDUG/topics/demo_setup.html` （「Power change switch」の長押しとHard reset test）
- NXP OM13588：`https://www.nxp.com/design/design-center/development-boards-and-designs/OM13588` （Archived / No Longer Manufactured）
- ST Source構成：`https://wiki.st.com/stm32mcu/wiki/STM32StepByStep:STM32_Advance_USB-Power_Delivery_Source`
- ST Source拡張ボード：`https://www.st.com/en/evaluation-tools/x-nucleo-src1m1.html`
- 技術根拠：添付 `USB_PD_R3.2_V1.2_2026_08_19_redline.pdf` の4.4節「Response to Hard Resets」、4.4.1節「Hard Reset Sequence」。SourceはvSafe0Vへの遷移後、復旧時間を経てvSafe5Vに戻す。**ソフトウェアADCの閾値とUSB規格の絶対的電圧・時間測定は同義ではない。**

## 3. 接続と保護条件

```text
適切な低電圧DC電源
    ↓（Source評価ボード指定の電源入力・保護回路を経由）
[制御可能なPD Sourceボード] ── USB-Cデータ兼電源ケーブル ── [PD17 M13c-2 Sink]
                                                              │
                                                            WCH-LinkE
                                                              │
                                                            Windows PC
```

- **UGREENやダイソーのUSB-C出力をSource評価ボードのUSB-C出力へ直結しない。** Source-Source接続、PC USB-Cからの直接VBUS印加、CCへの外部電圧印加は避ける。
- 電源はSource評価ボード指定の**電源入力**から給電し、過電流保護・電流制限を有効にする。USB-Cライン上に独自スイッチ/短絡器を挿入しない。
- WCH-LinkEでSink MCUを外部給電・ログ取得し、VBUSが下がってもSinkの処理を継続させる。
- 負荷抵抗・外部電子負荷・インラインUSB電力計なし。配線変更は必ず無通電時に実施。
- 異常な発熱、ケーブル損傷、意図しないVBUS電圧があれば試験を停止し、電源を外す。

## 4. 実験ゲート（必ず1つずつ）

### M13d-T00：Source側の事前検証（PD17は未接続）

1. Source側に**既知の正常なPD Sinkまたは評価用PD Sink**を接続してPD契約を確立する。Source単体・未接続の状態ではHard Reset交換やVBUS復帰を検証できない。
2. Source側の送信トリガ操作が**SourceからのHard Reset**であることをSource自身のログまたはアナライザで確認。
3. Source側の通常5Vの立ち上がり・Hard Reset時のVBUS 0V→5V動作を、適切な測定器で確認。
4. Sourceの過電流保護およびソフトウェア初期化手順を確認。
5. T00合格までPD17へ接続しない。Source側のボタン操作がPR_SWAPまたはSink発HRになる構成では不合格。

### M13d-T01：通常の5V PD Source契約（**Hard Resetは送らない**）

1. M13c-2をFlashしたPD17をWCH-LinkEで維持し、5V固定PDOの制御可能SourceにUSB-Cで接続する。
2. SourceCaps、Request、GoodCRC、Accept、PS_RDY、Readyの順序を確認する。
3. 10～20秒後に**人手でUSB-Cを取り外す**。500ms後の診断が終了するまで再接続しない。
4. `source_hr=0`、`hr_rx=0`、`PD17 STOP reason=1`、`trace_dropped=0`、`event_lost=0`を確認する。

### M13d-T02：Source発Hard Reset 1回 → 契約復旧（**第一の本試験**）

1. T01成功後、**新しい物理CC接続**を開始。5V固定PDOでReadyまで待つ。
2. **Source側だけ**の操作でHard Resetシグナリングを1回発生させる。CC接続とWCH-LinkEを維持する。ケーブル抜去・再挿入で代用しない。
3. 制御されたVBUS 0V→5Vを経てSourceCapsが再提示されるまで待つ。Sinkが固定5V/1A Readyへ再到達したか確認する（SourceがPPSを提供していればPPS Readyでもよい）。
4. 10秒程度後、ケーブルを抜いて診断を保存する。最初のHard Reset以外を意図的に送らない。
5. **期待される主なログ**：
   - `PD17 TRACE ... code=3`が1回。`code=19/20`は0回。
   - `PD17 STABILITY ERR source_hr=1`、`PD17 STOP ... hr_rx=1`。
   - `PD17 HR VBUS low_seen=1 high_seen=1 error=0 recovery_handoff=1 auto_recovery=1`。
   - 1回目の`code=3`の**後**に`code=15 detail=4`（Ready）が現れる。
   - 最後に人手で抜いた結果の`PD17 STOP reason=1`は**正常**。
   - `trace_dropped=0`、`event_lost_session=0`、`adc_fault=0`。
6. Source側のログ/アナライザのHard Reset送信時刻とSinkの`code=3`を関連付ける。**Sinkのログだけでワイヤ上の信号品質を立証したことにはしない。**

### M13d-T03：同一CC接続の2回目Hard Reset → 安全停止（**T02成功後の別実験**）

1. 新規CC接続を開始し、1回目のSource発HRでT02と同じ復旧を確認。
2. **一度もケーブルを抜かず**、Sourceから2回目のHard Resetを発生させる。
3. Sinkが**2回目のReady復帰を試みない**こと、`PD17 STOP reason=10`、`source_hr=2`、`hr_rx=2`を確認。
4. 手動でケーブルを抜いて診断を取得。通常の新規CC接続で枠がリセットされることは別途T04として検証可能。
5. 2回目のHard ResetによりSourceはVBUSをリセットする場合があるため、**VBUSの物理遮断は保証しない**。本機のfail-closedはPD通信停止・契約失効を意味する。

### M13d-T05：Source発Hard Reset後のPPS 7V/1A（**対応Sourceのみ・後回し**）

T02が5V固定PDOで合格し、SourceのPPS供給条件が検証できた場合だけ実施。HR前後にPPS 7V/1Aの契約が成立すること、`pps_recovery=1`、Ready後のPPS更新が続くことを確認する。Source側がPPS APDOを再提示しなければ、固定5V復帰は許容し、PPS復帰を強制しない。

## 5. ログ判定の使い方

**実機でT02が成立する前に、スクリプトのPASSをSource HR実機PASSと扱わないこと。**

Windows PowerShell（`C:\pj\pd_pps\src\pd_pps`）で、試験ログをUTF-8形式で `C:\pj\pd_pps\source_hr_T02.txt` 等に保存する。USB-C切断後の`PD17 TRACE BEGIN`から`PD17 VBUS END`までを含める。

```powershell
# T01 通常接続
py -3 .\17_pd_spr_sink_rebuild\tests\check_source_hr_log.py `
  C:\pj\pd_pps\source_hr_T01.txt --mode baseline

# T02 Source発HRを1回、その後固定5V復帰
py -3 .\17_pd_spr_sink_rebuild\tests\check_source_hr_log.py `
  C:\pj\pd_pps\source_hr_T02.txt --mode first --expected-ready fixed

# T03 Source発HRを同じCC接続で2回
py -3 .\17_pd_spr_sink_rebuild\tests\check_source_hr_log.py `
  C:\pj\pd_pps\source_hr_T03.txt --mode second

# ツール自体のテスト（12ケース）
py -3 .\17_pd_spr_sink_rebuild\tests\test_source_hr_log_checker.py
```

判定は`PASS`（観測ログの整合）、`FAIL`（明確な不一致）、`INCOMPLETE`（出力不足・TRACE欠落）を区別。ログ末尾の`PD17 VBUS END`まで取得する。**省略・切断後の再接続・TRACE容量超過で必要なイベントが失われた場合、成功とは判定しない。**

## 6. 未検証として残る項目

- Source発Hard Resetの実機波形、実際のCC維持、源のメッセージ送信確証。
- Hard Reset後の0V/5V監視での時間・電圧の規格適合性（ADC10msのサンプリングだけでは不十分）。
- 20ms復旧ゲートの故障注入、ADC故障注入、再交渉タイムアウトの実機停止。
- PPS対応Sourceでの実機再交渉と、長時間連続運転（長時間試験は利用者が別途実施）。

次のゲート：**Sourceを任意にHard Reset可能な試験環境を確定し、M13d-T00を実施する。**
