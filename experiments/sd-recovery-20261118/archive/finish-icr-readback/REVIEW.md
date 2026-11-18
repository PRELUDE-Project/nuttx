# 最終 DATAEND クリア後の readback 回帰

今回 PR #1 の HEAD は 627d0ec82c4d33fad252547bc65462e4d2d3effd と確認。独立範囲を SD 担当へ連絡した。対象は同固定版の SCR/sector finish。source/template/extract 対応検査は成功。

正常 claim/setup/command/IRQ の後、最後の DATAEND クリアの MMIO double にだけ故障注入を追加した。実関数は変更していない。SCR 8 B / sector 512 B × 正常、DATAEND 残存、DPSMACT、CPSMACT、IDMATE、DABORT、DTIMEOUT、DCRCFAIL の16例が UBSan で成功した。

正常2例は規定長だけコピー。異常14例はコピー前の最終チェックで EIO/QUARANTINED となり、512 B sentinel 全体を保持した。注入条件を解除して status をクリアしても finish は EIO を保持してコピーしなかった。IDMAEN の模擬値はこの段階では既に0であり、それだけで成功・再利用を許さないことを確認した。

初回コンパイルは移動したfixtureが actual-command-constants.h を見つけられず失敗した。元fixtureディレクトリを include path に加えることで解消。製品ヘッダーを代造したり変更したりしていない。

これは最終 readback の分岐に対するホスト試験であり、MMIOの順序保証、CPU barrierの実効性、物理IDMA停止、読戻し後の遅延DMA、実IRQ競合を証明しない。生成された driver-injected.c は試験用 double 差分を持つだけで、製品適用対象ではない。製品修正は不要だった。

再現: python3 finish-icr-readback/run.py。成果は test.c、driver-injected.c、result.log。対象 ARM ビルドと実機は未実施。ソフト導入・APT再試行・権限回避・実機操作なし。電源条件 HOLD を維持。
