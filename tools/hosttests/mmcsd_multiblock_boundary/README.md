# CMD25書込み後のエラーと完了待ちを確認するホスト試験

[既存のPIO試験](../scr_sector/multiblock-write-stop-precedence.md)は、通常のCMD23に対応しないSDカードの条件で、上位ロックとCMD12応答の順序まで確認しています。こちらは**通常のCMD23に対応する場合と、DMA用設定を含む場合**を追加で照合します。ACMD23と通常のCMD23は別のコマンドです。

`7965c77ad63db5ed14506ca19f112ac6df04fa7e` の `drivers/mmcsd/mmcsd_sdio.c` をSHA-256で確認し、`mmcsd_writemultiple` の定義をそのまま抜き出します。512バイト×2セクタ、データ転送の成功・タイムアウト・入出力エラー、停止処理の成功・失敗を組み合わせます。媒体や下位処理の応答はホスト用の代役です。

- 通常のCMD23に非対応なら、データ転送のエラー後にもCMD12の処理を1回呼びます。対応する条件ではCMD12は呼ばず、事前にブロック数を指定します。
- データ転送とCMD12の処理が両方失敗した場合は、データ転送のエラーを返します。転送成功後にCMD12の処理だけが失敗した場合は、そのエラーを返します。
- どちらの失敗でも、試験開始時に偽だった `wrbusy` は偽のままで、追加の書込み完了待ちを設定しません。成功した場合はブロック数2を返し、`wrbusy` を設定します。これは実媒体に保存されたことの確認ではありません。

Python 3とUBSan対応のCコンパイラーで、リポジトリのルートから実行します。別の固定ソースを使う場合は `--repo` でそのルートを指定します。試験ではDMA用コンパイル設定、書込み完了通知設定の有無、コマンド前後のデータ設定順を切り替えます。**DMAの動作、割込みの排出、媒体の回復、実際の保存・書込みは未確認**です。製品コードは変更しません。

```sh
python3 -B tools/hosttests/mmcsd_multiblock_boundary/run.py
```

```text
cc (Debian 14.2.0-19) 14.2.0
dma=False, notice=False
CMD25 boundary PASS cases=24 hardware=none
dma=False, notice=True
CMD25 boundary PASS cases=24 hardware=none
dma=True, notice=False
CMD25 boundary PASS cases=48 hardware=none
dma=True, notice=True
CMD25 boundary PASS cases=48 hardware=none
```
