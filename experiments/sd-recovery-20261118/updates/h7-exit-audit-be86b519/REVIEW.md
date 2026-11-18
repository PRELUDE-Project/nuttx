# STM32H7 固有コードの終了入口確認

固定原典: PRELUDE-Project/nuttx be86b5194ed8f88009c3f867bda71420a5b9d161。
2026-11-18 に GitHub の arch/arm/src/stm32h7 直下を列挙し、全 C ソースと Make.defs、CMakeLists.txt の計59ファイルを取得した。取得したパスと Git blob SHA は sources.json に保存した。

up_exit、nxtask_exit、_exit、nxsched_release_tcb、nxtask_recover の識別子を照合した結果、この59ファイルには一致がなかった。前回の ARM common/armv7-m 調査と合わせ、この範囲では task_exit への別入口を確認していない。ただし、全リポジトリの呼出し網羅や実リンク結果ではない。boards、アプリ側、他アーキテクチャ、生成コードは今回の範囲外。

task_exit.c が TCB を直接解放する点は変わらない。通常の _exit 経路では先行する exit hook の recover が候補の通知位置となるが、task_exit に単純に通知を追加する修正は行っていない。通知時点の排他、対象 TCB の寿命、再入制約、既存診断APIとの統合は引き続き製品適用条件である。

共有手順の確認として、保存済み replay.py を be86b519 の製品ソースと既存 cc/Python に接続し、15件の host UBSan 回帰を再実行して全件成功した。ARM ビルド、実OS上のタスク終了、物理 IDMA 停止、実機安全性を示す結果ではない。runtime ソース変更、マージ、実機操作、新規ソフト導入は行っていない。
