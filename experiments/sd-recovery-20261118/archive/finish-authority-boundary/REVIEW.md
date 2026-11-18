# finish の所有権とエラー伝播の独立回帰

今回の PR 読取りで PRELUDE-Project/nuttx PR #1 は Draft/open、HEAD 627d0ec82c4d33fad252547bc65462e4d2d3effd を確認した。対象は復旧済みの同コミットの製品関数。元 fixture の source/template/extract 対応検査も成功した。

SCR 8 B と sector 512 B の実 claim/setup 後、ACTIVE または実 eventtimeout による QUARANTINED 状態を作った。generation 不一致、key owner 不一致、呼出タスク不一致の三条件と、入力 result が 0 / EBUSY / EIO、出力が通常領域 / NULL / 内部 arena の三条件を組み合わせ、108 例を UBSan で実行して成功した。

不一致 finish はすべて ESTALE。device 全体、arena、MMIO 配列、通知数、barrier 数、出力 sentinel が不変だった。不正な error finish から別の所有者を seal することもなかった。その後、正しい所有者の finish を呼ぶと、timeout 済みは ETIMEDOUT を保持し、ACTIVE は指定した EBUSY で隔離された。

この追加試験は、古い要求の終了処理が現要求の隔離理由を上書きしないという境界を確認する。新たな製品修正は不要だった。ただし成功転送のコピー先容量を実行時に検証する API ではなく、固定長の有効な caller buffer が必要という契約は残る。本試験で過小バッファを安全に扱えるとは主張しない。

IRQ/タスクID/critical section/MMIO は既存ホスト double。実タスク消失、ID 再利用、同時実行、バス上の IDMA 停止、ARM 対象ビルドの証拠ではない。閉鎖後に IDMA が物理停止したとはみなさない。電源安全条件 HOLD、実機操作禁止は維持した。

再現: python3 finish-authority-boundary/run.py。test.c、result.log を同ディレクトリに保存。APT 操作、追加インストール、公開、実機操作は行っていない。
