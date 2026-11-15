# M11b — Ready中の同一SourceCaps再受信（1回のみ）

目的: Ready中にSourceから新しいMessage IDのSource_Capabilitiesを受信した際、
PDO内容が同じでもRDOを作り直してRequestし、Accept/PS_RDY/Readyに戻ることを実機で確認する。

## 設計

- M11のSoft Reset自動実験を無効化（M9版mainを基礎にM11b専用処理へ置換）。
- 初回のPE_READYに到達してから1,000ms後、Sinkから`Get_Source_Cap`（Control Message Type 7）を1回だけ送信する。
- PHYは既存のTX MessageID / GoodCRC / bounded retryを利用する。
- Sourceが新しいSource_Capabilitiesを返した場合だけ、従来のPolicy Engineが再評価してPPS Requestを送る。
- Get_Source_CapがGoodCRCされた後、SourceCapsが来なければReadyを維持する。GoodCRC自体が得られない場合は現行PHYのTX_FAILED→RECOVERYとなり得る。再要求はしない。
- USB-C接続中にprintfしない。M9 RAM traceは切断後500msから出力。
- 既存のWCH-LinkE、Windows/xPack/ch32fun/Makefile/開発スクリプトは無変更。
- `16b_source_caps_integrity`に変更なし。

## 操作

まずUSB-C未接続でBuild/Flash/Log、heartbeatを確認する。
次の実験でUSB-Cを直接接続して約3秒後に切断し、TRACE BEGIN〜ENDを提示する。
電力計・負荷は挿入しない。ログが得られなくても同期ログを追加しない。

## 期待ログ（説明であり実測ではない）

- 既存: 初回SourceCaps（code=7 detail=30）、Request（code=12 detail=1）、Accept/PS_RDY、Ready。
- 新規: Get_Source_Cap送信（code=12 detail=4; auxヘッダの下位5bitが7）、GoodCRC（code=10、valueのbit8=1）、新MessageIDのSourceCaps（code=7 detail=30と6件のcode=8）、Request（code=12 detail=1）、Accept/PS_RDY/Ready。
- 初回と再送信時の6 PDO値が一致、かつそれぞれのSourceCapsのMessage IDが違うこと。

## 判定上の注意

`Get_Source_Cap`への応答はSource実装に依存する。Sourceが応答しない場合、
Ready中の同一SourceCaps再受信を実機で検証できなかった、と扱う。
M11bはプロトコル適合性試験でも、独立した外部PDアナライザでの証明でもない。
M9 traceイベントが127件に達すると後続分はtrace_droppedで欠落が分かる。
