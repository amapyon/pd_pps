# M5 USB CDC-ACM 基本通信 検証記録

- 記録日: 2026-11-08
- 対象: PD PPS Explorer / WeActStudio CH32X035 CoreBoard V1.0
- 開発環境: Windows 11 / PowerShell / VS Code / ch32fun / riscv-none-elf-gcc / WCH-LinkE / minichlink
- 実験: `firmware/experiments/18_usb_cdc_acm_echo`
- 基準リポジトリ: `main` / 引継ぎ時点 `55c54e3`（本記録は新規コミットのハッシュを指定しない）

## 1. 目的と対象範囲

CH32X035内蔵USBFSでUSB CDC-ACM（仮想COMポート）を実装し、Windows 11から受信したデータを同一バイト列として返信する。

**対象外:** USB PD/PPSの同時動作、INA226/ADC取得、OLED/エンコーダUI、PCからの電圧・電流設定、長時間動作、規格適合性の確認。

## 2. 配線と重要な制約

| 信号 | CoreBoard | PC側追加USB-C基板 |
|---|---|---|
| D+ | TP1（PC17 / USB_DP） | D+ |
| D− | TP2（PC16 / USB_DN） | D− |
| GND | GND | GND |
| CC1/CC2 | — | それぞれ5.1kΩ（Rd）でGNDへ（追加USB-C基板側） |
| PC側VBUS 5V | MCUの給電回路には接続しない | 本実験では給電に不使用 |

- CH32X035は**別電源**で動作させる。CoreBoardの電源端子・レギュレータ・電圧・極性を回路図で確認する。
- PD/PPS電源側USB-C（J1）は**未接続**とする。J1のUSB D+/D−とTP1/TP2は基板上で同一ネットであり、同時接続は未検証。
- PC側VBUS検出は未実装。今回の構成をUSB規格準拠の完成品として扱わない。
- 製品統合前にUSBデータ配線の分離方式、USB側VBUSの検出、給電切替/逆流防止を設計する。

## 3. 実機確認した事実

| 項目 | 観測された結果 | 判定 |
|---|---|---|
| Build | `riscv-none-elf-gcc`によるBuild成功 | PASS |
| Buildメモリ（初期USBFS修正版） | FLASH 3,172 B / 62 KB（5.00%）、RAM 708 B / 20 KB（3.46%） | 参考値 |
| Flash | WCH-LinkE 2.18、CH32X035検出、`Image written.` / `Flash succeeded.` | PASS |
| Windows USB列挙 | 接続前にはなかった`USB シリアル デバイス (COM9)`が接続後に出現 | PASS |
| 識別子 | `USB\\VID_1209&PID_D035\\007` | 観測値 |
| CDC制御転送（初回） | COM9の`SerialPort.Open()`が失敗 | FAIL（修正前） |
| CDCエコーバック（制御要求修正後） | `PASS: 37 bytes echo-matched via COM9` | **PASS** |

補足: メモリ使用量は制御要求修正より前の成功Buildのログ。**最終修正版のサイズとしては扱わない**。最終修正後のBuild／Flash詳細ログは本引継ぎには含まれない。COM9での37バイト完全一致は実機で確認済み。

## 4. 修正履歴（今回の主な原因と対処）

1. Makefileの`ch32fun.mk`参照を、リポジトリ内の`../../../ch32fun/ch32fun/ch32fun.mk`に合わせた。
2. サンプル側`funconfig.h`による`FUNCONF_USE_USBPRINTF`の再定義を避け、M5ローカルの`usb_config.h`を使用。`_write`リンクエラーと重複マクロ警告を解消。
3. `fsusb.c`の`HandleSetupCustom()`戻り値を確認。正常なCDCゼロ長ステータス応答には負数、未対応要求のSTALLには0を返すよう変更。`SET_CONTROL_LINE_STATE`の応答を修正し、COMオープン後のエコーバック成功を確認。

## 5. 再現手順（既存スクリプトを利用）

```powershell
Set-Location 'C:\pj\pd_pps\src\pd_pps'
$Project = 'firmware/experiments/18_usb_cdc_acm_echo'

# Buildだけ実行する場合
.\dev-workspace\pd-pps.ps1 -Action Build -Project $Project

# Build＋Flash（ファームウェアを上書きする。内容を確認してから実行）
.\dev-workspace\pd-pps.ps1 -Action Flash -Project $Project

# まずCOM番号を確認
Get-PnpDevice -PresentOnly -Class Ports |
    Format-Table FriendlyName, Status, InstanceId -AutoSize

# エコーバック検証（COM番号は環境に合わせる）
.\firmware\experiments\18_usb_cdc_acm_echo\Test-M5-CdcEcho.ps1 -Port COM9
```

`dev-workspace/pd-pps.ps1 -Action Log`は既存のログ取得機能であり、CDC-ACMのエコーテストとは別。

## 6. 現時点の到達点・未完了項目

**到達点:** M5のUSB CDC-ACM基本通信（Windows 11側のCOM認識・ポートオープン・37バイト往復一致）を実機確認。

**未完了:**

- 1、63、64、65、127、128バイトの境界値、繰り返し送受信、COMポート再オープン
- 切断／再接続やバッファ過負荷時の復旧
- PCからのコマンドプロトコル／応答フレーム形式
- INA226、ADC、PD/PPS機能との統合
- PD側J1とPC側USBのD+/D−共有問題の解消、VBUS検出、規格適合性検討

## 7. 次の実験（ファームウェア変更なし）

次のようにPC側から小さなパケットを順次送信し、サイズ境界・内容一致・ポート再オープンを確認する。

```powershell
Set-Location 'C:\pj\pd_pps\src\pd_pps'
.\firmware\experiments\18_usb_cdc_acm_echo\Test-M5-CdcStress.ps1 -Port COM9 -Rounds 10 -ReopenCycles 3
```

エラーが出た場合はそのサイズ・周回・受信バイト数と、USB再接続前後の状態を記録し、次の修正対象を決定する。受信リングバッファは256バイトで実効容量255バイトのため、現段階のテストは最大128バイトの逐次送受信に限定する。長時間・高負荷テストはこの結果の後に設計する。

## 8. リポジトリ保護

- 実験16bの未コミット`pps_oled_encoder_pps_control.c`、`usbpd_current.h`は変更・ステージしない。
- 未追跡の引継ぎ／ROADMAP文書は変更・ステージ・削除しない。
- 実験01〜17とch32fun、`dev-workspace`、`.vscode/tasks.json`は今回のドキュメント更新の対象外。
- ビルド生成物（ELF/BIN/HEX/LST/MAPなど）はコミットしない。

## 9. 通信安定性検証の更新（2026-11-09）

<!-- M5_CDC_STRESS_DIAGV1_2026-11-09 -->

### 9.1 対象と実施条件

- 対象：実験18の診断版（`M5-CDC-Diagnostic-v1`）。2026-11-08の基準コミット `09ea8d2` から、未コミットのM5限定修正を適用した状態。
- 環境：Windows 11、CH32X035、COM9（`VID_1209&PID_D035`）、追加したPC通信用USB-C。PD側J1は接続しない。
- エコー用ファームウェアは、USB EP2 OUTで受信し、EP3 INへ最大63バイトずつ送信する。`USBFS_SendEndpoint()`の割り込み保護を使用し、RAM診断カウンターを追加している。
- 試験スクリプト：`Test-M5-CdcStress.ps1`。送信長1、7、31、63、64、65、127、128バイトを反復する。`-PauseMs 25`で逐次送受信し、各ケースのバイト列を照合する。

### 9.2 実機で確認した結果

| 試験 | 実測結果 | 判定 |
|---|---|---|
| 基本エコー | `PASS: 37 bytes echo-matched via COM9` | PASS |
| 短い確認試験（`-Rounds 2 -ReopenCycles 1 -PauseMs 500`） | 16ケース、972バイト、COM開閉1回 | PASS |
| 長い確認試験（`-Rounds 10 -ReopenCycles 3 -PauseMs 25`） | 240ケース、14,580バイト、COM開閉3回 | PASS |
| 上記の長い試験をケーブル抜き差しなしで追加3回連続 | **720ケース、43,740バイト、COM開閉9回**。3回ともPASS | **PASS** |
| 長い試験の合計（最初の1回＋追加3回） | **960ケース、58,320バイト、COM開閉12回** | PASS |

上記は**診断版ファームウェアにおける、指定されたデータ長・試行回数での成功実績**である。連続稼働時間の耐久試験や、製品環境での通信保証を意味しない。

### 9.3 RAM診断の結果

同じELF・ソースを添付した2つのSRAMダンプ（20KiB）を比較した。後者は収集時に `-State Failed` が指定されているが、実際の直前の通信試験は **PASS** している。ファイル名の`Failed`を通信障害の事実と解釈しない。

| 診断項目 | 試験前（00:59） | 試験成功後（01:00） |
|---|---:|---:|
| EP2受信バイト数 | 1,009 | 14,580 |
| EP3送信受理バイト数 | 1,009 | 14,580 |
| EP3送信開始・受理回数 | 29 | 420 |
| EP3 IN完了割り込み回数 | 29 | 420 |
| 送信拒否 | 0 | 0 |
| RX未処理データ | 0 | 0 |
| RXオーバーフロー | 0 | 0 |
| 取得時点のEP3 `busy` | 0 | 0 |

### 9.4 過去の障害と、現時点の解釈

以前のM5修正版では、63、65、127、128バイトで送信停止が断続的に発生し、RAMにEP3 `busy=1`とRX未処理データが残っていた。CH32X035のUSBFSレジスタをWCH-LinkEから直接読み出した値は、正常通信中にもUSB無効に見える値を返したため、停止原因を特定する証拠としては扱わない。

診断版では再現していないが、**診断処理によるタイミング変化の可能性が残るため、過去の停止の根本原因が確定・解消したとは断定しない。** 現時点では診断版をM5エコーバックの実機検証済みベースラインとして保全し、次に診断なし版とのA/B比較、USB再接続、負荷試験を検討する。

### 9.5 再現コマンド

```powershell
Set-Location 'C:\pj\pd_pps\src\pd_pps'
.\firmware\experiments\18_usb_cdc_acm_echo\Test-M5-CdcStress.ps1 `
  -Port COM9 -Rounds 10 -ReopenCycles 3 -PauseMs 25
```

### 9.6 残作業

- 診断計測なし／ありのA/B再現試験、USB再接続、継続時間を測る耐久試験、バッファ過負荷試験。
- PC向けコマンドプロトコルの実装（候補：`INFO`、`STATUS`）。**まだ実装済みとは扱わない**。
- INA226やPD/PPSとの統合、およびPC通信用USB-CのVBUS検出・PD側J1とのデータ線共有問題の設計解決。
- 診断版ソース・ホストテストと本ドキュメントをコミットして実機確認基準点を固定する。
