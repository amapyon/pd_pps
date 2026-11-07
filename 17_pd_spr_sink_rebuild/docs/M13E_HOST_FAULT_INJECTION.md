# PD17 M13e — ソフトウェア故障注入・安全停止の結合テスト

更新日：2026-11-08

## 目的・範囲

Source発Hard Resetの実機試験環境を用意せず、長時間連続運転も実施せずに、**M13c-2の既存ファームウェア**に備わるADC監視異常・通常PD交渉タイムアウトの安全停止をホスト上で検証する。

- **本番ファームウェアの変更はゼロ**。`main.c`、`Makefile`、PHY/PE/ADCの製品コードを改変しない。
- Cホスト試験 `tests/test_m13e_fault_injection.c` で、**製品版の** `pd_vbus_monitor.c`、`pd_policy.c`、`pd_phy_ch32x035.c`、`pd_stop_diag.c`、`pd_stability.c`を一緒に動かす。
- ADCのハードウェアドライバーとUSBPDレジスタ/IRQだけをモックで代替する。`ADC EOC`が到来しないケースとADC開始失敗、PEの期限切れ、TX_FAILEDを故障として注入する。
- `src/main.c` の分岐と同じ順序で安全停止API、PEイベント、最初の停止診断および統計を呼ぶ。ただし**実際の `main()` の無限ループを実行するE2Eテストではない**。
- 外部充電器、物理ADC、実VBUS変化、USB PD Sourceからのメッセージは試していない。安全性・規格適合性・電力遮断能力を保証するテストではない。

## 故障注入ケース（計8ケース）

| ID | 注入条件 | 停止原因 `PD17 STOP reason` | 主な検査 |
| --- | --- | --- | --- |
| FT-01 | 10ms ADC監視でEOC未到来、3msタイムアウト | 2 = ADC_MONITOR | `timeouts=1`、再測定停止、契約失効 |
| FT-02 | ADC変換開始APIが失敗 | 2 = ADC_MONITOR | 直ちに停止、`timeouts=1` |
| FT-03 | CC接続後、SourceCapsなしで650ms期限到達 | 5 = PE_TIMEOUT_CAPS | `PE_WAIT_CAPS`から停止 |
| FT-04 | SourceCaps受信後、Request未送信で開始待機15ms上限到達 | 6 = PE_TIMEOUT_ACCEPT | コマンド破棄・再送しない |
| FT-05 | Request送信後、Acceptを受信せず45ms期限到達 | 6 = PE_TIMEOUT_ACCEPT | 結果待ちのままでは残らない |
| FT-06 | Accept受信後、PS_RDYを受信せず500ms期限到達 | 7 = PE_TIMEOUT_PS_RDY | 未完了契約をReady扱いしない |
| FT-07 | ReadyでPPS更新開始後、Accept期限切れ | 6 = PE_TIMEOUT_ACCEPT | 既存の有効契約も失効する |
| FT-08 | PHY送信失敗イベント `PE_EV_TX_FAILED` | 4 = PE_TX_FAILED | イベント起点の停止も分類する |

### 全ケース共通の合格条件

1. PEが `PE_RECOVERY` となり `contract_valid=false`、`pps_active=false`、`command=PE_CMD_NONE` を保持。
2. PHYのTX待機・GoodCRC待機・再送回数がクリアされる。モックでSOP Acceptを後着させても**GoodCRCを返さない**。
3. 物理CC接続を維持したまま新しいSourceCaps/PS_RDYを入れてもReadyへ復帰しない。無関係なVBUS回復を仮定しても停止ラッチが解除されない。
4. `PD17 STOP`相当の最初の原因・PE遷移・相対時刻は後続イベントで上書きされない。
5. 物理CC切断イベント→`phy_reset_after_detach()`→新しいCC接続を模擬した後にのみ、監視器・診断・統計・PHYがリセットされる。再交渉し、新しいRequestを送信できる。
6. 期限直前は停止せず、期限到達時に停止する（FT-03～FT-07）。

## 実行方法

POSIXシェルとC11ホストコンパイラー（gccまたはClang）がある環境で、リポジトリルートから：

```sh
sh 17_pd_spr_sink_rebuild/tests/run_m13e_host_test.sh
```

このスクリプトはホストコンパイル、8ケース実行と、`main.c`/Makefileにベンチ専用刺激を追加していない静的チェックを行う。Windowsの`riscv-none-elf-gcc`はMCU用クロスコンパイラーであり、PCホストテストを直接実行する代替ではない。PowerShellだけで実行することを要求しない。

## ホストでの実行結果（2026-11-08）

- FT-01～FT-08：**8/8 PASS**。
- 既存 `tests/run_*_host_test.sh`：**15系統すべてPASS**（M12～M13c-2と本M13eを含む）。
- M13dのPython判定：**12/12 PASS**。
- Clang UndefinedBehaviorSanitizer：**PASS**。
- Windows/xPack Build・Flash：**今回未実施（本番ファーム変更なし）**。
- **実機でADC変換故障／PDタイムアウトを注入する試験は未実施**。実物のVBUSを遮断する機能ではない。

## 未検証／保留

- Source発Hard Reset：ユーザー判断により試験機材がないため保留。M13dの計画・検証ツールは保存済み。
- 数分～10分以上の連続運転：ユーザーが別途実施するため保留。
- 実機ADC故障注入と異常時電源の挙動：安全な治具・条件が整うまで実施しない。
- 本番`main()`の実時刻競合、USBPDハードウェアのGoodCRC抑止、規格試験：ホスト模擬結果だけでは保証できない。
