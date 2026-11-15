# M43B-7 P2 実機回帰検証記録

- 作成日：2026-11-15
- 管理区分：**実機試験結果の要約**。実験KIT内の`tests/TEST_PLAN.md`は計画の正本。
- 対象基準版：`src/pd_pps/firmware/experiments/43b6_p0p1_refactor`（P0/P1）
- 検証版：`src/pd_pps/firmware/experiments/43b7_p2_session_refactor`（P2）
- ハード：WeActStudio CH32X035F8U6 CoreBoard、WCH-LinkE、CC1、外部負荷なし。PC CDCの物理接続ON/OFFを試験ごとに指定。
- **重要：リファクタリングの回帰PASSと、PD/PPS通信の正常動作PASSは異なる。**

## 1. 変更対象と保護対象

| 段階 | 変更 | 保護 |
|---|---|---|
| P0/P1（M43B-6） | PDポリシー異常状態クリア処理、CDCの10進インデックス解析を共通化。旧ソース比較テスト追加 | M43B-5、PD PHY、USBFS ISR、既存CDC応答、SRAM形式 |
| P2（M43B-7） | `main.c` 398→40行。`pd_session.c/h`と`pd_session_diag.c/h`へ責務抽出、回帰テスト追加 | M43B-6、PD PHY、USBFS ISR、GoodCRC・PPSの時間条件、SRAM形式、コマンド応答 |

**実施したホスト確認：** P0/P1は59件実行・失敗0・スキップ1、P2は67 PASS／1 SKIP。変更CユニットのRISC-V32向けコンパイルはPASS。ただし、これはWindows公式ツールチェーンによる最終ELF/BINの独立再生成を意味しない。実機でのコード識別は現PCのBuild/Flash記録とBIN SHA-256で照合すること。

## 2. 変更前の物理USB切り分け（M43B-5）

| 試験 | PC USB | 実測結果 | 限界 |
|---|---|---|---|
| M43B5_05_OFF_CC1 | OFF | PPS READY約31.9秒、GoodCRC一致7回、PPS更新6/6、再送0、TX_FAILED0 | 単独の正常例 |
| M43B5_06R1_ON_CC1_IDLE_PNP | ON・CDCアイドル | GoodCRC一致0、CRCタイムアウト3回、再送2、TX_FAILED。USBFS ISR0。PnP監視120秒中COM16/WCH-LinkEが認識されたまま | Windows PnP安定はCC信号品質の証明ではない |
| M43B5_07_OFF_CC1 | OFF | 初回Request GoodCRC一致、Accept/PS_RDY/READY、READY約30ms、約590msでVBUS_FAULT、更新0 | USB切断が長時間安定を保証するわけではない |

## 3. M43B-6 P0/P1：実機回帰

| 試験 | 結果 | 判定範囲 |
|---|---|---|
| M43B6_R01_OFF_CC1 | 初回Request GoodCRC一致、Accept/PS_RDY/READY、READY25ms、VBUS_FAULT | 旧M43B5_07と同じ成功→安全停止経路を確認 |
| M43B6_R02_ON_CC1_IDLE | GoodCRC一致0、CRCタイムアウト3、再送2、TX_FAILED、USBFS IRQ0 | USB ONでの既知失敗経路と整合 |
| M43B6_R03_CDC_INDEX | 正常値／境界値／不正入力を含むCDC24/24 PASS | CDCインデックス解析の互換性 |

## 4. M43B-7 P2：実機回帰（今回の完了条件）

| 項目 | R01 OFF CC1 | R02 ON CC1 IDLE |
|---|---:|---:|
| GoodCRC一致 | 1 | 0 |
| 初回Request再送 | 0 | 2 |
| CRCタイムアウト | 0 | 3 |
| TX_FAILED | 0 | 1 |
| Accept／PS_RDY | 受信 | 未到達 |
| PPS READY | 到達 | 未到達 |
| READY時間 | 24ms | 0ms |
| 周期更新 | 0 | 0 |
| Source Hard Reset受信 | 1 | 1（TX_FAILEDの後） |
| USBFS IRQ総数 | 14 | 0 |
| GoodCRC窓中USBFS IRQ | 0 | 0 |
| SRAM freeze | VBUS_FAULT | TX_FAILED |
| SRAMイベント保存／欠落 | 50／0 | 43／0 |
| 対応する旧基準 | M43B6 R01 | M43B6 R02 |
| **判定** | **既知挙動への回帰PASS** | **既知挙動への回帰PASS** |

- **R01:** Source Caps→Request→GoodCRC一致→Accept→PS_RDY→READY。約590msでVBUS低電圧停止。長時間PPS契約や周期更新の安定性を合格としない。
- **R02:** CDC接続・コマンド停止状態で初回RequestがCRC_TIMEOUT3回、再送2回、TX_FAILED。USBFS割り込みがなくても失敗。P2固有の新しい通信失敗とは判定しない。
- **R03 CDC:** `PING`、`STAT`、`COUNT`、`VBLOG n`、`PDTRACE n`の正常値・境界値・不正入力を含む24ケースすべてPASS。`VBLOG`/`PDTRACE`が空のケースの解析結果は確認できるが、非空の実データ取得経路の全網羅ではない。

### 提出されたログの識別と整合性

| 試験 | 元データ／ファイル名 | SHA-256（生データ） | 結果 |
|---|---|---|---|
| R01 | `M43B7_R01_OFF_CC1_pd.log` | `543298b921c0152adb7a41491b1634d9f856e8c4ece71f1b0b85777bb5abfaff` | TRACE／STABILITY確認 |
| R01 | `sram_20k(20261115-141157).bin` | `3e6120fa6f22217b735fc1c9cea80cca09427086cd3760996560e51ff916dc2f` | CSV・JSONデコード一致、freeze VBUS_FAULT |
| R02 | `M43B7_R02_ON_CC1_IDLE_pd.log` | `d3099debb01c66b12fe72082fa3646e942608bb3ea78d1ba510b60b1d9cb2ea3` | CRC_TIMEOUT3、TX_FAILED |
| R02 | `sram_20k(20261115-141954).bin` | `023384995a35b201f6a55ac4852c14736e8fbda23b5b9d898403f2dea575b704` | CSV・JSONデコード一致、freeze TX_FAILED |
| R03 | `M43B7_R03_CDC_INDEX_20261115_231918_972.log` | `104489c6385cc6dac3624c77394fc4249c356249c5c7a671dd831a267dcebe3d` | PASS24 FAIL0 |

ログはユーザーから提出されたコピーのSHA-256。ユーザーPC側のファイル名やコピー方法で異なる可能性があるため、**ソース/ビルドBINのSHA-256とは混同しない**。生ログ・SRAM・CSV/JSONは`diagnostics/`内のGit対象外に保持し、コミット用文書には要約と識別ハッシュだけを登録する。**元ログを編集・移動・上書きしない。**

## 5. 判定と制約

- **P0/P1/P2の主要回帰ゲート：PASS。** ソース分割前後のPDイベント系列、停止条件、CDC解析応答について現時点の実機条件で重大な差分は見られない。
- **未解決A：** PC通信用USBの物理接続ON時にGoodCRCを認識できず、初回RequestがTX_FAILEDとなる場合がある。USBFS ISRゼロやPnP列挙安定だけでは電気的原因は特定できない。SourceがGoodCRCを送信したかは未確認。
- **未解決B：** USB OFFでもM43B5_07以降、契約後約24～30msでVBUSが低くなり安全停止する場合がある。VBUS実電圧とADC値の概ねの一致は確認済みであり、ADC精度校正はPCB後工程とする。供給低下の原因は未解決。
- **未評価：** PPS長時間更新、P3の実装、全故障条件、アナライザー／CC波形、BINハッシュと実機FWの独立照合。
- **次のステップ：** 検証済みM43B-7を保全し、P3として診断イベント定義とPC側時系列表示を別の変更で行う。通信不具合修正・ハード改造は混ぜない。

## 6. Gitでの保存対象

コミット対象は根本的にソースと検証記録であり、実験用の生ログはGit除外を維持する。コミット直前に現PC上で`git status --short`と`git diff --cached`を確認し、既存の未コミット変更を無断で巻き込まない。**この文書はコミット実施の証拠ではない。**
