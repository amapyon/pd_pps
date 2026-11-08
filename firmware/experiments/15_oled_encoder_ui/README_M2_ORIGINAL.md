# 13_oled_ssd1306 — M2統合版ファームウェア

作成日：2026-11-02  |  対象：WeActStudio CH32X035 CoreBoard（CH32X035F8U6）  
ベース：提供された `11_pps_ina226_monitor.zip` のソース一式

**本フォルダーは、元の `11_pps_ina226_monitor` にSSD1306表示を実際に接続した統合ソースです。**
主プログラムは `pps_oled_ssd1306.c`、MakefileのTARGETも `pps_oled_ssd1306` に変更しています。
元ファームウェアの`11_pps_ina226_monitor`フォルダーを上書きしないでください。

## 実装された機能

- 既存：USB PD/PPSの交渉・再接続・周期更新、`v`/`i`/`set`/`m`/`s`/`scan`等のコマンド、PA0 ADC校正、INA226計測。
- 追加：SSD1306 128×64 I²Cの`0x3C`→`0x3D`自動検出、1秒周期の英数字表示。
- 追加：既存のPA2(SCL)/PA3(SDA)ソフトウェアI²CにOLEDを相乗りさせるアダプター。
- 追加：INA226を約800ms周期で別途読み、最新値をOLEDへ渡す（既存`m`/`s`コマンドは変更なし）。
- 追加：1画面1024バイトを16バイトずつ分割、1メインループ当たり最大1回のOLED I²Cトランザクション。
- 追加：PDの能動交渉中はOLEDアクセスを避ける。OLED未接続／転送エラーでもPD/PPSとINA226は継続。
- **電流の絶対精度はM1で未確定**：画面のLOAD、PWRは暫定値。PPS要求電流(REQ)と実測電流(LOAD)を区別。

## 電気配線：OLED 4端子（端子ラベルを確認）

| SSD1306モジュール | CH32X035 / INA226バス | 注意 |
|---|---|---|
| VCC | **動作仕様を確認した3.3V** | 未確認のモジュールへ通電しない。PPS VBUSをつながない |
| GND | WeAct GND（INA226と共通） | 給電OFFで接続 |
| SCL | PA2 / INA226 SCLと同じ線 | 3.3Vプルアップのみ |
| SDA | PA3 / INA226 SDAと同じ線 | 3.3Vプルアップのみ |

モジュール側のSDA/SCLプルアップ、電源電圧、SSD1306/SH1106の識別は実機確認が必要です。
OLEDの**端子の物理的な並びは未確認**なので、ラベルを見てください。

## Windows 11 / ch32fun 環境でのビルド

この実験フォルダーを、既存の`11_pps_ina226_monitor`や`ch32fun`と同じ階層へ置きます。
Makefileはベースと同じ `CH32FUN := ../ch32fun/ch32fun` を維持します。

```powershell
cd C:\pj\pd_pps
.\pd-pps.ps1 -Action Build -Project 13_oled_ssd1306
# 電源電圧・配線・無負荷を確認できた後だけ実施
.\pd-pps.ps1 -Action Flash -Project 13_oled_ssd1306
.\pd-pps.ps1 -Action Log
```

もしくはVS Codeの既存の実験選択手順を使用します。ローカルのツール設定により実験の登録が必要な場合があります。

### 起動時に期待するログ

```text
BOOT: CH32X035 PPS + calibrated ADC + INA226 + SSD1306 (PA2/PA3)
M2: SSD1306 ready at 0x3C (PA2 SCL / PA3 SDA)
```

OLEDが検出されなかった場合は、`M2: no SSD1306 at 0x3C/0x3D (PD/INA226 continue)` と表示します。
OLEDのアドレス判定は**新ファームの起動ログ**で確認します。元の`scan`コマンドはINA226用の`0x40..0x4F`しか探索しません。

### 画面例（表示は英数字）

```text
PD/PPS EXPLORER
PPS 9.00V
REQ 1000MA (PD)
BUS 8.85V
LOAD 34.00MA
PWR 0.301W
PD READY INA OK
REQ != LOAD CURR
```

## コンパイル・実機検証状況

- **PASS**：既存7種類のCホストテスト（コマンド、再接続、PDO/APDO、ADC校正、INA226換算）。
- **PASS**：M2のCホストテスト2種類（SSD1306初期化・表示分割送信・I²C NACK時STOP）。
- **未実施**：CH32X035向けクロスビルド、実機Flash、OLEDのACK・描画、共存・負荷評価。
- この実行環境には`ch32fun` SDK / RISC-Vクロスコンパイラーがないため、ELF/BIN/HEXは生成していません。古いバイナリーも混入させていません。

ホストテストは実験フォルダーで`make test`。これはコンパイル時のI²Cロジック検証であり、実機検証を代替しません。

## 重要な変更点

| ファイル | 変更内容 |
|---|---|
| `pps_oled_ssd1306.c` | 旧`pps_manual_voltage.c`から改名。M2アダプター、画面初期化、PD/INA226値の更新、メインループ後段で描画 |
| `ina226.c/.h` | 既存ビットバンギング関数のラッパー3つを**末尾に追加**。GPIO設定・ACKロジック・INA226校正は変更なし |
| `m2_bus_adapter.c/.h` | 元のSTART/ACK/STOPへOLEDトランザクションを接続 |
| `m2_ssd1306.c/.h` | SSD1306初期化、フレームバッファ、文字描画、16バイト分割送信 |
| `m2_dashboard.c/.h` | 計測画面。`ina226_units.h`と同じ換算を採用 |
| `Makefile` | 新ターゲットとCソース3点を追加、既存+OLEDホストテストを統合 |

- OLEDだけを一時停止して比較する際は`pps_oled_ssd1306.c`の`M2_OLED_ENABLE`を`0`にします。
- 画面バッファは静的領域に置き、スタックを圧迫しません。
- USB PD/INA226の割込み処理にはOLED操作を追加していません。
- 既存の `GPIO_CNF_IN_FLOATING` でSDA/SCLを解放する実装を**変更していません**。
- Flash前に`M2_TEST_PLAN.md` のT01（モジュール仕様確認）を実施してください。

元のM1説明は `README_M1_ORIGINAL.md`、試験計画は `M2_TEST_PLAN.md`、主要な差分は `DIFF_11_to_13.patch` に収録。
