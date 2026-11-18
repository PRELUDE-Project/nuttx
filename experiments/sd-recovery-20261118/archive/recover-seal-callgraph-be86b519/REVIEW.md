# 終了通知内の driver 隔離と watchdog cancel の実関数接続

対象 PR #1 は照合時 HEAD be86b5194ed8f88009c3f867bda71420a5b9d161、open / draft。製品変更・公開・実機操作なし。

## 今回進めた点
前回の通知は順序確認用callbackだけだった。今回は提案通知→実 scr_seal→実 scr_close_producers→固定 wd_cancel を接続した。wd_cancelの従来bool doubleを実本体へ置換し、タイマー有効/無効とリスト先頭/非先頭の4条件をUBSanで確認。提案通知を挿入した実 nxtask_recover 本体を経由するが、それ以外のOS cleanup、spinlock、リスト、MMIOはdouble。timerは手動seedで、wd_startは接続していない。

結果は4条件成功。通知後 QUARANTINED/ECANCELED、CPU destinationとIRQ maskの失効、遅延DATAEND後の出力不変を確認。IDMAENは1のまま保持することをassertした。物理停止ではない。最初のcompileは旧WDOG_ISACTIVE macro重複で失敗し、bool用macroを除去して再コンパイル成功。

## 再入経路の限定
固定 wd_cancel はtimer callback自体を呼ばず、spinlock下でリストから外しfunc=NULLとする。ただし sched_note_wdog と nxsched_reassess_timer を呼び得る。固定 sched_note.h:666–670 と sched/sched/sched.h:332–336を取得・blob照合した。

参照configは SCHED_TICKLESS無効、SCHED_INSTRUMENTATION無効、SMPなし、HIPRI無効。その条件ではnote/reassessは空macroとなり、今回のnotify_sealから見える経路に task_delete/restart/終了通知再入はない。これは**このcallbackと参照configに限る静的結論**。実採用configは未確定で、一般的な通知APIの再入安全性を意味しない。

通常終了 _exit→exithook→recover、外部delete→terminate→exithook→recover、restart→reset_task→recoverは既存原典を再読し、既知の各入口のcritical区間を確認した。ただし部分復旧リポジトリ内のgrepは全callerの網羅証明ではない。GitHub code searchはdefault branch検索で結果0件だったので、対象固定commitの『他のcallerなし』の根拠にはしない。実OSの全経路、ARM割込排他、既に実行中のcallbackとの関係は未検証。

## 独立レビュー
別担当が生成ソースを読み既存binaryの4例を再実行し成功。限定callbackでは再入到達経路未発見まで狭められるが、製品適用不可を維持するとの結論。

未完は、正式構成と逸脱防止のbuild guard、実IRQ/OS同期、固定device・owner・generationを結び付けた通知、実行中callbackの寿命、通知失敗時の扱い。試案のmutable global targetは単一target試験専用で、複数instanceへ一般化しない。汎用APIのassert依存は残る。OS新規hookを公開していない。正式H753 config・対象ARMビルド・実機・物理IDMA停止は未確認、電源HOLD維持。

再実行: `python3 run.py`。原典blobは source.json / additional-sources.json、試験は run.py / connected.c。製品fixtureは ../eventwait-error-seal-431fd0ea/product/ に固定保存されている。
