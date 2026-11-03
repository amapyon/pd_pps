# 16_oled_encoder_pps_control — M4-B 実PPS制御（安全ガード付き試験版）

更新日：2026-11-02  | 状態：**ホストテスト13種類合格／CH32X035実機Build・Flash・出力変更は未検証**

## 目的と元バージョン

M4-Aで実機検証した`15_oled_encoder_ui`の独立コピーです。`15`は保全し、UIの確認（PUSH: APPLY）後にだけUSB PD/PPS要求を送れる`16`を作成しました。入力GPIO/ソフトI²C/INA226換算/PD基本通信は変更しません。

- 元の`15`は**UI編集デモ専用** (`M4_PPS_APPLY_ENABLE=0`)。ログとOLEDで「DEMO - NO PPS CHANGE」を確認済み。
- 今回の`16`は`M4_PPS_APPLY_ENABLE=1`で**実際のPD要求変更が可能**。十分に検証されていない新しい試験ファームウェアです。
- 既存のシリアル`v`/`i`/`set`コマンドも引き続き実PD要求を変更します。UIモードだけで安全を保証できません。

## 変更点

| ファイル | 内容 |
|---|---|
| `pps_oled_encoder_pps_control.c` | M4-Aメインの複製。確定時だけ送信。PD/CC状態を確認し、元設定・現在の同一APDOを再検証。`PS_RDY`確認後のみ新しい目標値を保存。失敗時は旧目標のままPD再交渉へ。 |
| `m4_ui.c/.h` | 編集開始時のAPDO情報を保持し、Source Capabilitiesが変更されたら編集中の操作を取り消す。 |
| `m4_screen.c` | APDO表示には保持した編集時スナップショットを使用。確認画面は`LIVE PPS CHANGE`/`PUSH: APPLY`と表示。 |
| `m4_apply_guard.c/.h` | PD状態、旧目標維持、Source APDO一致、V/I範囲と20mV/50mA単位、低電流、power-limited APDOの送信禁止を純粋関数で確認。 |
| `tests/test_m4b_apply_guard.c` | 追加安全ガードのホストテスト。M4-Aの12テストも継続。 |
| `Makefile` | ビルドターゲットを`pps_oled_encoder_pps_control`に変更。 |

## 今回の安全上の制限

- **初回は必ず外部負荷を接続しないこと。** 先にBuildログを確認し、次のステップでFlashする。初回の実要求変更は7.00V → 7.10V、要求電流1000mAでのみ試す。
- APDOの**同一ソースPDO番号・最小/最大電圧・最大電流**が編集開始時から変わっていないか、送信直前に再確認する。
- **PD ready / CC接続 / 元の要求値一致**が必要。該当APDOが消えたり変更された場合は送信しない。
- `PPSpowerLimited=YES`のAPDOは初回版では送信禁止。最終製品としては別途評価。
- 要求電流`200mA`未満は初回版の実送信で拒否。これは**充電器への要求値**であり、負荷電流のハードウェア制限ではない。
- 失敗・タイムアウトでは、新しい要求値を保存せず旧目標を保持して既存の再交渉処理へ移行。PD接続の回復を保証するものではない。
- `PS_RDY`はPDプロトコル上の応答確認であり、**実際のVBUS電圧と安全性の保証ではない**。INA226 BUSとDMM/USB計測器による確認が必要。
- ブレッドボードとPD充電器の接続は確実にし、初回の試験中はUSB負荷・外部機器・抵抗負荷を接続しない。
- 充電器を挿したままジャンパ線に触れない。配線変更はPD/WCH-LinkEとも無通電で行う。
- 本版は実験用。過電流/短絡/逆流のハードウェア保護は未実装であり、販売品仕様には使えない。

## 接続ピン（M3/M4-A実機で確認済み）

| 機器 | CH32X035側 |
|---|---|
| OLED SSD1306/INA226 SCL/SDA | PA2/PA3（共通のソフトI²C、0x3C/0x40） |
| EC11 S1/S2 | PA4/PA5 |
| EC11 KEY | PA6 |
| 戻るボタン | PA7とGND |
| OLED/INA226/EC11モジュール電源 | 3.3V（EC11端子表示「5V」でも5Vは供給しない） |

## インストールと実機検証

`16_oled_encoder_pps_control`フォルダーを`C:\pj\pd_pps\src\pd_pps\`に置きます。`15_oled_encoder_ui`は上書きしないこと。

**まずBuildだけ**実行します。

```powershell
cd C:\pj\pd_pps
.\src\pd_pps\dev-workspace\pd-pps.ps1 -Action Build -Project 16_oled_encoder_pps_control
```

Buildが合格したら、`M4B_TEST_PLAN.md`に従い、無負荷でFlash→起動値を確認→7.10V/1000mA要求を明示的に確定→OLED/シリアル/INA226 BUSで検証します。まだFlashしないでください。

**ホストテスト**（この作成環境で合格済み）：

```bash
cd 16_oled_encoder_pps_control
make test
```

## 試験完了の意味

現時点で確認できたのはソース統合とPCホストテストのみです。**CH32X035向けクロスビルド/Flash成功、実PPS交渉、切断・失敗時の動作はまだ確認していません。** M4全体は`ROADMAP.md`に記載したゲートを通過するまで進行中です。
