# 実NuttXのタスク終了と診断leaseの責任境界

対象: PRELUDE-Project/nuttx PR #1、今回の読取りは Draft/open、HEAD 431fd0ea0f2a4f9a178fa46d9a53c46235a2433f。固定版8原典を全文取得しGit blob SHA-1を照合。sources.jsonに固定URLとblobを保存した。

## 確認した終了経路

1. sched/task/task_delete.c の nxtask_delete は別タスクについて nxnotify_cancellation を確認し、保留できなければ nxtask_terminate を呼ぶ。自分自身の削除はこの確認より前に _exit に進む。
2. task_terminate.c は EXIT_PROCESSING を設定、実行リストから除外し、nxtask_exithook を経て nxsched_release_tcb へ進む。呼出元のreadonceに戻ってfinishを実行する構造ではない。
3. task_exithook.c は nxtask_recover、親への通知・終了待機解除、group_leave、signal cleanupを実行する。削除した別タスク側の文脈で動く場合がある。driverのowner照合をそのまま流用する終了callbackは対象ownerと一致しない可能性がある。
4. task_recover.c はmutex、TCB watchdog、semaphore、message queue等を扱うがSDMMC専用のfinish/sealを呼ばない。sem_recover.c は待機リストから対象TCBを除去し、count等を回復する。これをSDMMC transfer停止と読み替えられない。
5. wd_recover.c の対象は &tcb->waitdog のみ。driver構造体の waitwdog / fifowdog とは別。タスク削除でdriver watchdogが自動cancelされるという前提は成立しない。
6. pthread_cancel.c はTLS cleanupを明示的に呼ぶが、task_delete.cは同じ経路ではない。pthread cleanup登録だけを通常taskの非同期削除保証にできない。

## 固定設定との照合・実行

参考 candidate.config は CONFIG_CANCELLATION_POINTS 無効、pthread有効。task_cancelpt.c の実 nxnotify_cancellation と wd_recover.c の実 wd_recover を抽出し、noncancelable有無×FORCED_CANCEL有無の4例をUBSanで実行した。noncancelableかつ非forcedのみ通知保留、FORCED_CANCELではnoncancelableが迂回される。critical depthは元に戻る。TCB watchdogのcancel対象も確認。

TLS/TCB/IRQ/wd_cancelはホストdouble。driver watchdogを別変数にした確認は引数の対象区別のみで、実watchdog queueを動かしたものではない。これはOS上の削除・signal配送・PID再利用の実行証明ではない。上位CLIの起動文脈、FORCED_CANCELを設定する全呼出元、group退出処理の全体と正式H753 configはこの回では未確定。

## 修正の判断

単純なtask_setcancelstateによる非cancelable化だけでは、自己_exitとforced経路を含む所有者寿命を保証できない。したがってそれを安全修正として製品へ追加しなかった。消失したタスクのstackにあるkeyや出力を終了hookから参照しないこと、削除文脈で元ownerとしてfinishしないこと、物理停止が不明なarenaを解放しないことが必要。

次の具体的工程は通常CLIの起動経路とFORCED_CANCEL設定元を固定Apps/NuttXで追跡し、単発診断を終了させない対象運用の範囲を確認すること。より一般的な中断対応には、永続要求を所有する専用実行主体とその終了通知の設計が必要であり、PIDやTCBアドレスだけの判別へ置き換えない。

製品変更・公開・追加インストール・実機操作なし。電源HOLDとIDMA物理停止未証明を維持。参考構成は正式構成ではなく、ARMビルドも未実施。
