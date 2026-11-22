# SCRの連続処理と機能無効時の確認

固定版 [NuttX `28b37ee3`](https://github.com/PRELUDE-Project/nuttx/tree/28b37ee3e661d5ab66dc2f0b5e56440b55a8f3d8) を使う、Linuxホスト上の2つの確認です。実機、カード、製品ソースには変更を加えません。

## 前回の処理キーは次の処理に使えない

SCRを有効、セクター読み出しを無効にし、同じタスクがSCRを2回連続して実行します。1回目は正常に終了し、待機タイマー・割り込みマスクが解除されます。2回目の待機を設定した後、1回目のキーで完了・停止状態照会を呼ぶと、どちらも `-ESTALE` を返します。2回目の状態・出力・通知は変わらず、正規の完了だけが新しい8バイトを渡し、通知を1回出します。

既存の `scr_containment` ホストスナップショットを再利用します。取得・完了・停止状態照会の部分は、照合した現行ドライバーのソースから取り込みます。MMIO、割り込み、カード、OSは模擬です。これは正常終了から次の処理に移る試験であり、エラーで隔離された領域や実機のDMAを再利用できるという意味ではありません。

## SCRを無効にしたときの構文エラー

現行H7ドライバーでは、`stm32_scr_stop_status()` の閉じ波括弧より前に機能フラグの `#endif` があります。SCRを無効にすると閉じ波括弧だけが残り、該当箇所を使った構文試験は失敗します。修正候補は `#endif` を閉じ波括弧の後ろに移すだけです。Linux上で該当箇所を抜き出した試験では、修正候補がSCR有効・無効の両方で通りました。修正候補は製品ソースには適用しておらず、NuttX全体のビルドも未実施です。

```sh
python3 run.py /path/to/fixed-nuttx
LC_ALL=C python3 check_guard.py /path/to/fixed-nuttx
```

GCC 14.2.0を使用しました。連続処理の試験は `-O0`・`-O2`、UBSan有効で成功しました。結果は `results.log` と `guard-results.log` にあります。

## 実機で記録するMCUの版と通信の確認範囲

NUCLEOとBBMは、同じ型番でもMCUのリビジョンが同じとは限りません。実機で試験したときは、基板名・基板版とともに、`DBGMCU_IDC` から読み取った `REV_ID` をそれぞれの結果に残します。

- [ST ES0392 Rev. 16の§2.8.3](https://www.st.com/resource/en/errata_sheet/es0392-stm32h742xig-and-stm32h743xig-device-limitations-stmicroelectronics.pdf#page=26)はY/W（`0x1003`）が対象です。SDから全バイトを受信した後でも、最後の3バーストのいずれかでIDMAのAHBエラーが記録されると、次回の受信でオーバーランが起き得ます。[今回のSCR診断候補](https://github.com/PRELUDE-Project/nuttx/blob/c1fb8bedc304eec12d063a2c5fe547b84bf28430/arch/arm/src/stm32h7/stm32_sdmmc.c#L977)はY/Wを受け付けず、[`IDMATE` が残る場合もデータをコピーしません](https://github.com/PRELUDE-Project/nuttx/blob/c1fb8bedc304eec12d063a2c5fe547b84bf28430/arch/arm/src/stm32h7/stm32_sdmmc.c#L1139)。このエラッタを根拠に、他の版へ一律にリセットを追加しません。
- [§2.8.6](https://www.st.com/resource/en/errata_sheet/es0392-stm32h742xig-and-stm32h743xig-device-limitations-stmicroelectronics.pdf#page=27)は同資料に記載された全リビジョンが対象です。SDMMCはコマンド応答と受信データの終端ビットを検査しないため、そこだけが誤っていても受信した値とコントローラーの状態からは分かりません。取得値と状態はソフトウェア側の結果として記録し、配線上のフレーム全体が正しいという判定には使いません。終端ビットまで確認が必要なら、試験の対象に含めたうえで波形を確認します。

この追記は仕様書とソースの照合結果です。リビジョンの実測、波形、異常後のIDMAの物理停止は確認していません。
