# M12b: GoodCRC timeout / RetryCounter 異常系ホスト試験

対象: `17_pd_spr_sink_rebuild`、2026-11-07

## 目的・変更内容

M12aで実装した「送信 TX_END を起点とする CRCReceiveTimer」の異常系を確認する。
M12aからの **デバイス向けコードの変更は `src/pd_phy_ch32x035.c` の1箇所だけ**。

- `nRetryCount=2` に達して `TX_FAILED` が確定したら `MessageIDCounter` を1進める。
- 同じイベントを Policy Engine に引き渡して、現行プロジェクトの安全側動作（`PE_RECOVERY`／契約無効化）を維持する。
- M11bの Ready中 `Get_Source_Cap` 実験、M9のRAMトレース、M8の未接続heartbeatは変更しない。
- IRQ内やCC接続中の`printf`は追加しない。

根拠：USB PD 3.2 Release 3.2 v1.2 (2026-08-19) の §7.31.1（CRCReceiveTimer）、§7.32.1～7.32.4（Message ID/Retry）、Table 7.9（tReceive 900～1100µs）、Table 7.11（nRetryCount=2）、§9.1.2.2.1.9（送信失敗時のMessageIDCounter更新）。

**重要：** §7.32.4 の最終失敗後のSoft Resetによるリンク復旧は現行v0.1では未実装。現在は意図的に `PE_RECOVERY` でfail-closedとし、M13で自動復旧を検討する。このホスト試験のPASSはUSB PD規格への完全準拠を意味しない。

## ホストテスト

実行する本体：`src/pd_phy_ch32x035.c`を`tests/test_phy_crc_retry.c`から直接includeしてコンパイルする。再送アルゴリズムのコピーや擬似実装ではない。USBPDレジスタ、SysTick、NVIC、CC状態だけを`tests/mocks/ch32fun.h`で模擬する。

Linux/macOSなどホストGCC環境：

```sh
cd 17_pd_spr_sink_rebuild
sh tests/run_m12b_host_test.sh
```

テスト内容（6シナリオ）:

1. `IF_TX_END`が届くまでは時間が経過しても再送を始めない。
2. EOP起点から47,999tickで再送なし、48,000tickで再送。2回の同一フレーム／同一MessageID再送。3回目の期限で`TX_FAILED`を通知し、MessageIDを進め、Policyが契約を無効化する。32bit SysTick折り返しも含む。
3. 正しいGoodCRCを受けると期限が解除されMessageIDが進み、次の送信へ移れる。
4. 不一致MessageIDのGoodCRCは期限を解除せず、再送中の正しいGoodCRCで回復する。
5. MessageIDの7→0折り返し、Hard Reset受信によるタイマー停止。
6. CC切断確定後に再送しない。

`-Wall -Wextra -Werror`でホストGCCコンパイル済み。UBSanでも6/6 PASS。既存`tests/test_policy.c`もPASS。

## 実機でまだ確認していないこと

- GoodCRCの実際の欠落、CH32X035のTX_END信号と物理的なEOP最終ビットの関係
- tRetry（タイマー期限から再送Preamble開始までの時間制約）
- Hard Reset/Soft Resetを含む規格準拠の自動復旧
- 実機でTX_FAILEDになったときのMessageID更新の観測
- USB PDコンプライアンステスト全般

現段階ではUSB-C上に意図的なエラーを挿入しない。

## Windows 11での次の実機操作

ZIPは既存リポジトリのルートに展開する。既存の`dev-workspace/pd-pps.ps1`は変更しない。

1. 充電器を外す。LogをCtrl+Cで終了する。
2. 配布されたM12b差分ZIPを `C:\pj\pd_pps\` に保存。
3. 既存のPowerShellで展開し、従来スクリプトでBuild/Flash/Logする。
4. USB-C未接続時のheartbeatを確認する。この段階では充電器を接続しない。

M12bの実機Buildはまだ実施されていない。
