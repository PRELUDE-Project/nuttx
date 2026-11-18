# SD所有者終了・隔離の実験記録

**未完成の試案です。ファームウェアへ組み込まず、実機へ書き込まないでください。**

基準は PRELUDE-Project/nuttx PR #1、コミット `be86b5194ed8f88009c3f867bda71420a5b9d161`。現行driverとは分離して、ローカルにあった試案、固定原典の抜粋、試験、条件、結果を保存しています。通常のMake/CMake/Kconfigからは参照しません。

## 再実行

同じリポジトリのチェックアウトで `python3 experiments/sd-recovery-20261118/replay.py --repo .` を実行します。Python 3、既存のCコンパイラ、UBSanが必要です。追加ソフトは自動導入しません。生成バイナリは一時ディレクトリだけへ作ります。これはhost回帰で、ARMビルド・実OS・実機試験ではありません。

`archive/`には各工程の経過と当時の試験ソースがあります。古いrun.pyには当時の復旧ディレクトリ構成への依存が残るため、現時点の再実行入口はreplay.pyです。古い報告の「未公開」はその報告時点を示します。この保存後も、試案は製品実装へ統合していません。

## 最新の設計

- `recover-split-host-be86b519`: 固定通知をscheduler/driverの別host翻訳単位へ分離。任意callbackと任意node登録を認めません。
- `recover-fixed-hook-be86b519`: 単一のstatic登録枠と固定SD callback。汎用callback案を置き換える候補。
- `recover-os-source-candidate-be86b519`: 旧汎用通知API案と構成gate。汎用再入の問題があり、そのまま採用しないでください。
- `recover-export-integration-be86b519`: 既存4API名への登録/解除統合をhostで検討。
- `recover-reset-connected-be86b519`: 実reset_task本体の抽出によりTCB再初期化前の隔離順序を比較。

## 未完了

実OSの全終了経路とTCB再利用前の解除、正式同期API、構成制約、OS/driver分割リンク、正式H753生成設定、ARM対象ビルド、電源起動条件、物理IDMA停止は未証明です。hostでの成功を製品適用・実機安全の証拠としません。現在PRはdraftで、これらの保存だけを理由にマージしません。

## 保存範囲

所有者終了、IRQ/IDMA隔離、エラー伝播、callback/worker、参考構成および関連OS原典のローカル記録を対象にしています。既にPRへ反映済みの製品コピー、再取得可能な巨大NuttX/Appsの部分復旧ツリー、コンパイラ/設定ツール本体、バイナリ、私的環境の導入操作記録、別プロジェクトのH753コードコピーは含めません。したがって「ローカルディスクの全ファイルを複製した」とは扱いません。
