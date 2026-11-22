# SCR診断：ACMD51のR1エラーと遅延DATAEND

NuttX 7965c77ad63db5ed14506ca19f112ac6df04fa7e の呼出しを既存ホストfixtureで接続します。CMD55のR1は正常で、ACMD51は下位で受理しますが、そのR1にカードエラーを注入します。応答フェーズが進んでも上位がエラーを拾ってキャンセルすること、後着DATAENDでコピー・再通知・再取得が起きないことを確認します。CMD55送信前タイムアウトや同コマンドのpoll失敗を扱った f567f8bd とは別の試験です。

入力元の7ファイルをGit blobハッシュで照合します。Linux、Python 3、GCC（UBSan対応、検証時14.2.0）で次を実行します。

```sh
python3 run.py /path/to/pinned-nuttx-source
```

結果は `results.log` です。割込みはキャンセル完了**後**の同期呼出しであり、実時間の競合、実機上のDMA停止、セクタ無効のARMイメージを検証していません。既存fixtureはセクタ有効設定ですが試験はSCRのみです。ハードウェア・製品ソース・CIは変更していません。
