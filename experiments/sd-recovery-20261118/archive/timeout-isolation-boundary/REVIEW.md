# 実timeout入口を通した隔離境界の回帰

固定対象: PRELUDE-Project/nuttx 627d0ec82c4d33fad252547bc65462e4d2d3effd。直近PR読取りも同HEAD。製品変更・実機操作なし。

既存driver fixtureはtimeout経路でstm32_endwaitを直接呼んでいた。本回帰では製品からstm32_eventtimeout関数を無変更で抽出し、既存の実endwait、scr_seal、scr_close_producers、IRQ、SCR/sector finish、claimへ接続した。元fixtureのsource/template対応検査も通過。

8 B SCRと512 B sectorの2経路それぞれで、実claim/setup後にwaitwdogを有効化し、DPSM activeを模擬した状態から実timeoutを呼ぶ。7種の遅延IRQ status（DATAEND、DPSMACT、CPSMACT、IDMATE、DTIMEOUT、DCRCFAIL、全bit）×2経路×100回、計1400例をUBSanで実行し成功した。

確認結果:
- timeoutからQUARANTINEDへ遷移し、-ETIMEDOUTを保持する。
- watchdog無効、wait/xfr mask 0、buffer NULL、remaining/receivecnt 0、unaligned_rx falseとなり、待機通知は1回。
- 閉鎖後のtimeout再呼出しとIRQは、状態・MMIO配列・通知数を変更しない。
- IDMAENは1のまま残ることを明示的にassertした。これは停止ではなく隔離である。
- 隔離後にarenaへ残留DMA相当の変化を注入してもfinishは-ETIMEDOUTを返し、呼出元512 B全体のsentinelを保持する。
- SCRとsectorの再claimはどちらもEBUSYで拒否される。

適用判断: 現行単発診断は異常時のIDMA強制停止を装わず、永続arenaを再利用せずに隔離する。その条件下では本試験で新たな修正を要する誤コピー・遅延通知を再現しなかった。IDMAENを無条件に落とす、arenaを解放する、同一boot再試行を許す変更は加えない。

限界: IRQ/WDOG/critical sectionはホストdouble。実IRQ nesting、watchdog dispatchとcancelの真の同時実行、OSタスク終了、デバイスによる実DMA書込みは未実行。残留DMA相当の注入はmemsetであり、バスやデバイス停止を証明しない。前段の2worker seed試験は別の同期API境界であり、この診断のcallback受付は遮断されたまま。H753正式SD config、ARM対象ビルド、電源安全条件と実機での停止観測は引き続き必要。

再現: python3 timeout-isolation-boundary/run.py。test.cは固定driver.cをincludeして使用し、抽出したeventtimeoutを追加する。出力はresult.log。製品fixtureには書き戻していない。
