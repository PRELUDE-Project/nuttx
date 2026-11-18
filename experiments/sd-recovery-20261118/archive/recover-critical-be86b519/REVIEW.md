# 参照設定で実際に選択される IRQ 排他処理

PR #1 HEAD be86b5194ed8f88009c3f867bda71420a5b9d161 を再確認。製品変更なし。

今回、前回のcritical doubleを実実装に置き換える前に設定分岐を調査し、重要な限定を確認した。参照 candidate.config に CONFIG_IRQCOUNT=y は存在しない。固定 sched/Kconfig はIRQCOUNTのdefault nを指定する。include/nuttx/irq.hはIRQCOUNTなしでは enter/leave_critical_sectionを直接up_irq_save/restoreへ展開する。したがって irq_csection.cのTCB irqcountを増減する実装を、参照構成で実行される処理として試験してはならない。

ARMv7-M irq.hの実Cラッパーは、コメントにprimaskと書かれていてもgetbasepri/raisebasepri/setbasepriを使用する。参照構成のBASEPRI_WARはoff。chip.hのdefault priorityは0x80、nvicpri.hはdisable priorityをdefaultへ定義する。HIPRIはoff。stm32_irq.cの初期化はNVIC優先度をdefaultへ設定する一方、現SDMMC driverのattachはirq_attachを呼び、独自priority変更は行わない。実採用環境で別コードがpriorityを変更しないことは未確認。

実際のirq.hの条件分岐2ブロックとARM Cラッパーを抽出してcheck.pyでコンパイル。ARMのregister操作だけをhost変数へ置換し、最初のBASEPRIが0/0x80の双方で二重enter→内側leave→外側leaveを実行。内側leaveまで0x80を保持し、最後に元の値へ復元することをUBSanで確認した。chip/nvicの定数は固定原典から抽出する。実ARM命令、TCB切替、割込配送、DMAを実行した結果ではない。

この結果で前回の単CPU通知案の入れ子排他を具体化できたが、製品化の承認材料はまだ不足。全recover呼出元の網羅、実採用H753 config、priority変更の有無、登録レコードとdevice/owner/keyの完全な紐付け、実OS終了/再起動試験が必要。SVC/NMI/HardFaultをBASEPRIで止められるとは扱わない。実機の電源安全と物理IDMA停止は未証明のまま、HOLDを維持。

保存: irq_csection.c、include/nuttx/irq.h、ARM irq/nvicpri、stm32_irq.c、chip.h、check.py、selected.c。取得blobはsource.json/additional-sources.json（chip.hは本文取得後にblob一致検証済み）。同じ既存driver試験件数に加算しない。
