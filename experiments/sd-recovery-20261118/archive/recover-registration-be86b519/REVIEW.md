# 所有者終了通知の登録・寿命設計試案

## 対象と結果
PR #1 は照合時点で open / draft、HEAD be86b5194ed8f88009c3f867bda71420a5b9d161。製品変更・公開・実機操作なし。固定原典の nxtask_recover 本体を抽出し、先頭へ**新規提案** recover_notices の呼出しを追加した設計試案をホストでコンパイル・実行した。これは NuttX に既存の通知登録 API があるとの主張ではない。

`cc -std=c11 -Wall -Wextra -Werror -fsanitize=undefined prototype.c -o prototype && ./prototype` が成功。実行結果は recover ordering, duplicate/reentrant binding, restart reuse, HANDOFF が PASS。OS側 cleanup 関数は順序確認用の double。実 scheduler / driver / IRQ / IDMA は接続していない。従来の実 driver 回帰件数へ加算しない。

## 設計で絞った条件
- 通知レコードは controller と同じ静的寿命とし、終了する stack のキーや出力を保持しない。対象 TCB を比較するが、それだけで世代を保証せず、登録を recover で同期的に消費する。
- claim の critical 区間内で世代取得と登録を完了する。登録失敗なら DMA setup 前に fail-closed。PID だけの登録は不可。
- 通知はまず登録リストから unlink してから同じ critical 区間で実行する。callback を work queue へ積まず、通知のコピーを終了後へ残さない。重複 recover は消費済み登録を再実行しない。
- recover の既存 mutex recovery より前に隔離する案。待機資源回収や再初期化より先に CPU コピー権限を失効させる。通知が task 削除要求者の文脈でも動くため、owner を this_task に置き換えてはならない。
- SCR が正常 HANDOFF に達した場合、同じ critical 区間で旧所有者の登録を解除する。そうしないと、別 task が sector を引き継ぐ正当な経路で旧 task の通常終了が誤隔離を起こす。新しい sector claim は新世代・新 owner で登録する。
- QUARANTINED/RETIRED の終了通知は arena 解放や IDMAEN 書換えを行わない。登録の解除と arena の再利用許可は別物。
- 通知中の再登録を拒否することで同じ通知ループに callback が登録を足し続ける動作を避ける。試案は single CPU / ordinary IRQ critical のみ。SMP、zero-latency IRQ、非同期 callback 対応を主張しない。

## 製品実装への未完条件
この試案は新規 OS API の候補であり、既存 PR へそのまま反映できない。公開ヘッダ・Kconfig・登録関数の自己 task 制限・終了処理中の登録拒否・エラー unwind を設計し、全 nxtask_recover 呼出元の同期条件を確認する必要がある。driver と通知の両方が同じ同期境界を使うこと、成功 HANDOFF / sector RETIRED / 異常隔離で登録を適切に解除すること、旧世代の通知が新世代へ作用しないことを実ソース接続で検証する。

現状の prototype は caller が critical を保持する前提を assert で表現し、driver の実 seal を呼ばず、通知中の世代確認のみ行う。通常終了・restart・external delete の各 real OS 試験が必要。対象ビルド・正式 H753 設定・物理 IDMA 停止・電源安全条件は未確認のまま。実機 HOLD 維持。

固定原典の詳細は ../normal-exit-path-be86b519/ と ../task-exit-original-431fd0ea/。抽出元 SHA256 は source-sha256.txt。recover-body.inc の挿入行に PROPOSED NEW integration と明記してある。

## 独立レビュー後の修正
レビューで旧世代の遅延解除が新登録を外す欠陥を検出。試案の unbind を owner と generation 一致時だけに変更し、旧 token の解除は ESTALE、新登録が残ることを回帰で確認した。bind の NULL node を EINVAL に変更した。通知なし回の stage を明示的にリセットし、前回値に依存した試験を修正した。UBSan を含む再コンパイル・実行は PASS。

通知内から recover に再入する場合の禁止が assert のみという指摘は未解消。実 callback の禁止操作契約・release時にも成立する失敗処理と全呼出元の同期証明がないので、製品適用不可を維持する。単に戻って cleanup 続行すると隔離漏れとなり、無根拠な panic 挿入も採らない。今回の成果は新規通知APIの設計欠陥を製品導入前に削減したもので、所有者終了問題を修正済みとはしない。
