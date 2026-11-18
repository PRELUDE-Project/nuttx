# 単発読取りの呼出し中断と再入の境界

PR #1 は今回の読取りで Draft/open、HEAD 431fd0ea0f2a4f9a178fa46d9a53c46235a2433f。製品の抽出済み mmcsd_sector_readonce と mmcsd_sector_validate を変更せず、下位呼出しのホスト double に検査を追加した。

参考 candidate.config は SDMMC1 のみ有効、SDMMC2 無効。正式 H753 生成設定は未確認。このため複数 controller の仮想網羅は追加しなかった。

前回の PID 再利用モデルは、旧キーが保存され、下位 finish を直接呼べる場合の結果である。通常の単発診断で同じ API に入り直す場合とは区別する。今回、claim、DMA setup、send/poll、response、eventwait、finish の6境界から readonce を再入呼出しし、さらに各境界から longjmp で呼出しを放棄するモデルを比較した。12例すべて UBSan 成功。再入と放棄後の呼出しは EBUSY、下位呼出し回数は不変、出力も変更しない。最初の正常呼出しを継続した6例は成功し512Bをコピーした。

実上位コードは下位 claim より前に g_sector_attempted を critical section 内で確定する。終了時にこれを解除しない。readonce 自体は priv->lock を取得しないため、この入口の放棄によって通常ブロックドライバーのmutexを残すという説明は当てはまらない。CLIの output は static 512B、下位arenaも永続領域であり、通常CLIの出力領域がそのタスクのstackと共に失われる構造ではない。一方、readonce の key はローカルで、強制終了を検知する終了処理は追加されていない。

この結果は再試行禁止が残る証拠であり、放棄後に ACTIVE lease が自動隔離されること、実OSの task_delete・PID再利用・多CPU同時実行、物理IDMA停止を証明しない。longjmpはホスト上の呼出し放棄モデルでありOS終了試験ではない。下位6呼出しはdoubleで、既存driver試験との完全接続ではない。実機・対象ビルドは未実施。電源安全条件 HOLD を維持し、製品修正・公開・新規ソフト導入・実機操作はしていない。

この範囲で新しい製品不具合は再現しなかったため、推測によるowner解除・PID変更・自動復旧は追加しない。残る必要条件は、診断実行タスクが終了させられない運用または正規OS終了機構と永続操作所有者の設計を固定構成で検証すること、および物理停止の別検証である。

再現: python3 readonce-abandon-431fd0ea/run.py。test.c と result.log を同じディレクトリに保存。
