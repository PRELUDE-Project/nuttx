# 所有タスクの直接終了に対応する設計の残条件

対象PR #1のHEADをbe86b5194ed8f88009c3f867bda71420a5b9d161と再確認。直前に追加したeventwait負値時の隔離は、待機関数が呼出元へ戻る経路だけを対象とする。この回では別の仮定を増やしたhost試験や推測的な製品変更はしない。

## 固定原典で確認した挿入点

- 外部task_delete: nxtask_delete → nxtask_terminate → nxtask_exithook → nxtask_recover / group_leave → nxsched_release_tcb。
- 通常の最終終了: nxtask_exit本体 → nxsched_release_tcb。architecture側の_exitからの全経路は未確認。
- task_restart: nxtask_restart → nxtask_reset_task → nxtask_recover / group_kill_children / signal cleanup → up_initial_state → nxtask_activate。TCBをreleaseせず同じPID・TCBで再開する。

したがってrelease_tcbだけに通知を加えてもrestartを覆わない。recoverだけでは通常終了全体の保証がない。pthread cleanup、groupのatexit、schedulerのstop記録だけに依存した修正も、これら全経路を覆う根拠が必要。

## 次に実装する場合の具体的な契約

終了通知方式を採るなら、driverがCPUコピーを許可する前提を失効させるだけに限定する。通知はstatic controller領域と固定世代の登録情報を参照し、終了対象taskのstack上のkey/outputは参照しない。taskを削除した別taskの文脈でも処理するため、現ownerのPIDでfinishを呼ぶ方法は使わない。旧登録世代が新しい要求を隔離しない条件が必要。

登録とlease取得は同じ同期境界内で完了させ、通知できない場合はDMA開始前に失敗させる。通知処理は待たず、メモリーを解放せず、IDMAENを変更せず、現在の世代をQUARANTINEDへ移してCPU destinationとproducerを失効させる。物理停止・復旧の許可は与えない。登録解除と実行中通知の寿命を保証する仕組みも必要。

この契約を満たす公開済みの汎用task終了登録APIは、今回読んだ固定sched.hと終了原典では確認できなかった。新しいOS hookの導入はdriver内の小修正ではない。最低でもrelease前とrestart再初期化前の2境界、およびARM _exitの実到達経路を点検する必要がある。フックの順序・同期・重複呼出し・呼出しcontextが未確定のまま実装を公開しない。

## 採らなかった代案

- PID/TCBアドレスの照合強化: 同一TCBのrestartを識別できない。
- cancel無効化: 直接restart、自己_exit、forced経路の保証にならない。
- ownerを検出した時点でarenaを解放: 物理IDMA停止が未証明なので禁止。
- 成功IRQまでACTIVEを保持する既存方式を停止完了と呼ぶ: CPUコピーの権限と転送完了・物理停止は別。
- 固定時間でsealする新watchdog: 所有者終了の即時通知ではなく、正当な処理の許容時間・claim前後の適用範囲を新たに定める。既存の転送タイムアウト値を所有者寿命へ流用する根拠はない。

## 実行すべき対象試験

正式構成と対象ビルドを用意した後、別のcontroller-free OS試験でdelete/restart/通常returnをそれぞれ実行し、通知のtask文脈、TCBの生存範囲、critical状態、重複回数を確認する。その後にdriverへ接続し、登録前、claim直後、wait中、DATAEND後finish前、HANDOFF、RETIREDの各境界で、CPUコピーなし・同じbootで再利用なし・旧世代による現要求変更なしを確認する。物理IDMA停止はこれらと別に測定する。

実機の安全条件HOLDは維持。正式H753 config・ARM対象ビルド・実OS試験・実機は未実施。今回の成果は固定コードを根拠とした設計条件の具体化であり、task_delete/restart問題を解消した製品修正ではない。

原典は ../task-exit-original-431fd0ea/ と ../cli-owner-lifecycle-431fd0ea/ に保存済み。be86b519の変更はeventwait限定で、これらOS終了原典は変更していない。
