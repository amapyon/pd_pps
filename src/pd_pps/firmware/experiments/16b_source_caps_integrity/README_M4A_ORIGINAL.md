# 15_oled_encoder_ui — M4 スタンドアロンUI（初回プレビュー版）

更新日：2026-11-02 | 状態：**PCホストテスト合格、CH32X035向け実機Build/Flash未確認**

## 出発点

M3で実機動作確認した`14_encoder_ec11`を丸ごと複製し、下記のファイルを**追加**しました。元のPD/PPS・INA226・GPIO・ソフトウェアI²Cは変更していません。

- `m4_ui.h/.c`：画面状態遷移・PDO情報・V/I編集・キャンセル・明示的確認（MCU非依存）
- `m4_screen.h/.c`：SSD1306 8行×21文字の画面描画（既存M2 SSD1306へ出力）
- `tests/test_m4_ui.c`、`tests/test_m4_screen.c`：PC上の状態遷移・描画テスト
- `pps_oled_encoder_ui.c`：M3本体にUIイベント処理・画面更新を統合（元は`pps_encoder_ec11.c`）
- `Makefile`：ターゲット名およびM4の2ソースを追加

M3本体を参照：`README_M3_ORIGINAL.md`。実機Build/Flash・入力テストとコミットに使ったM3フォルダーは独立して保全してください。

## 安全上の重要事項

**初回版の`M4_PPS_APPLY_ENABLE`は`0u`です。** EC11でPPS値を編集して確認画面でPUSHしても、**実際のUSB PD/PPS要求は変更されません**。結果画面には`DEMO - NO PPS CHANGE`と表示され、デバッグ端末へプレビュー値を出力します。**シリアルの `v`、`i`、`set` 操作は従来と同じく実要求を変更するため、混同しないこと。**

実要求変更は、メニュー操作、キャンセル、PD切断、対応APDOのチェックが実機で通過した後に別途有効化します。

## GPIO/配線（M3で実機検証済みを維持）

| 機能 | MCU | 補足 |
|---|---|---|
| INA226/OLED I²Cクロック | PA2 | 既存のソフトウェアI²C。Highは入力に切り替えて解放 |
| INA226/OLED I²Cデータ | PA3 | `0x40`、`0x3C`（M3ログで確認） |
| EC11 S1 / S2 | PA4 / PA5 | 右5クリック→+5、左5クリック→−5を検証済み |
| EC11 KEY | PA6 | 3回押下イベント確認済み |
| BACKタクトSW | PA7 | 3回押下イベント確認済み |
| EC11電源（基板表記5V） | 3.3V | **実際は3.31Vで使用。5V/VBUSは接続しない** |
| OLED電源 / INA226電源 | 3.3V | 共通GND、既存配線を維持 |

外部負荷なしから試験を開始してください。PD出力とWCH-LinkE電源を停止してから配線を変更します。

## 画面遷移

```text
MONITOR --PUSH--> MAIN MENU
                      |> MONITOR  --PUSH--> MONITOR
                      |> SOURCE PDO --PUSH--> PDO一覧 --BACK--> MENU
                      `> PPS EDIT  --PUSH--> EDIT VOLTAGE --PUSH--> EDIT REQ CURRENT
                                                               --PUSH--> CONFIRM
                                                               --PUSH--> DEMO RESULT
BACK: EDIT VOLTAGEからMENU、EDIT REQからEDIT VOLTAGE、CONFIRMからEDIT REQ。
```

- 監視画面：`PPS`は要求電圧、`REQ`は要求電流（PD側の要求）、`BUS/LOAD/PWR`がINA226実測値。
- SOURCE PDO：充電器から取得したPDO/APDOを最大8件表示。FIXED/PPSの種別、最大電流、PPSのmin/max電圧を表示。
- PPS EDIT：PD READYと、現在の要求値を包含するPPS APDOがなければ編集に進まない。回転で電圧100mV、要求電流50mAずつ変更。
- CONFIRM：確定ボタンでプレビュー結果を表示。**初回版ではPD要求送信は行わない**。
- シリアルコマンドで元の設定値が変更された場合、編集中の値はキャンセルされる。PD切断でも同様。

## ビルド・試験

Windows PowerShell（ユーザーが使用している既存ワークスペース）：

```powershell
cd C:\pj\pd_pps
.\src\pd_pps\dev-workspace\pd-pps.ps1 -Action Build -Project 15_oled_encoder_ui
```

**まずBuildのみ実施し、ログを確認してからFlashする。** 新しいフォルダーを`C:\pj\pd_pps\src\pd_pps\`に置く。`14_encoder_ec11`は上書きしない。

PC上のテスト（gcc/clangがある場合）：

```bash
cd 15_oled_encoder_ui
make test
```

`make test`は既存10種にM4の2種を追加し、計12種を確認する。既存の`ch32fun`SDKとRISC-Vクロスコンパイラがこちらの作成環境にはないため、CH32X035実機ビルドはWindowsでの確認が必要。

## 将来の実PPS設定変更

`pps_oled_encoder_ui.c`の`M4_PPS_APPLY_ENABLE`を`1u`へ変更すると、明示的確定後にPD READY・元の要求値・充電器APDOの電圧/電流範囲を再検証してから、既存`send_and_wait_pps()`を呼ぶ実装を含む。ただし、**この有効化は今回行わず、実機UI試験終了後に別リビジョン・別試験として実施すること**。

## 完了判定

`M4_TEST_PLAN.md`の段階A/Bを順次合格し、実機でPDO閲覧・PPS値編集・実要求変更・実測表示・安全なキャンセルを確認するまではM4は「進行中」のまま。
