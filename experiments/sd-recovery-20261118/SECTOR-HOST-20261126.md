# 1セクター読出しのホスト回帰試験（2026-11-26）

H753ZI向けにビルドした候補のソースで、既存の1セクター読出し試験を実行しました。最初は、`c1fb8bed` が `stm32_scr_stop_status` の閉じ括弧を条件コンパイル内に移したあと、試験のソースハッシュ表を更新していなかったため、古い試験データの使用を防ぐ検査で停止しました。

既存の生成手順を実行し、抽出した関数、コマンド定数、各試験の内容が一バイトも変わらず、ソースハッシュ表の `stm32_sdmmc.c` 1行だけが更新されることを確認しました。検査を無効にせず、再生成後に回帰試験を実行しました。

- 正常時の512 Bコピー、コマンド・応答・データ・IDMAエラー、タイムアウト後に遅れて完了した場合の再コピー拒否、世代・所有者の照合、再試行しないことを確認しました。
- エラー待ちの6条件、抽出内容が古い場合の拒否、ソースから再生成せずハッシュ表だけを更新した場合の拒否も通過しました。

MMIO・タイマー・OSとカード操作は模擬です。実機のカード読出し、実機IRQの停止、IDMAの物理停止の証明ではありません。[対象ビルドの記録](QUICKSTART.md)とは検証範囲が異なります。

```text
PASS source/template/extract correspondence
PASS extraction parser, clean correspondence, stale fixture and hash-only source update rejection
PASS actual sector review claim/setup/CMD17/R1/ISR/endwait/finish:512 copy,IRQ errors,timeout+lateDATAEND,reentry,owner/generation,full SRAM bound. MMIO/timer/CPU mocked; not physical-stop or upper-card-validation proof.
PASS actual upper validate/readonce -> actual lower claim/setup/send/R1/ISR/endwait/finish normal, poll timeout, R1 error, data timeout, IDMA error; delayed DATAEND/no retry/no copy. Block/wait arm, poll timing/event dispatch and MMIO mocked; no hardware stop proof.
PASS actual upper validation/readonce control: CSD/type/bounds, output512, all failing stages preserve errno/no premature copy, attempt consumed/no retry. Lower/MMIO mocked; no physical-stop proof.
PASS candidate=1 6 negative wait models; no task deletion or physical stop proof.
SCR-only diagnostic result 0; no sector test or stop proof; no retry
Read summary: bytes=512 CRC32/ISO-HDLC=b2aa7578; not compared with expected data; no stop proof
Single-sector diagnostic result 0; no data displayed; no stop proof; no retry
SCR-only diagnostic result 0; no sector test or stop proof; no retry
Single-sector diagnostic result -5; no data displayed; no stop proof; no retry
Single-sector diagnostic result -110; no data displayed; no stop proof; no retry
PASS pinned host tests. MMIO, timers and OS/device APIs are mocked; no physical-stop proof.
PASS candidate=1 6 negative wait models; no task deletion or physical stop proof.
PASS extraction parser, clean correspondence, stale fixture and hash-only source update rejection
```
