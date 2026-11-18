# 完了 IRQ 後の platform 条件再確認

今回の PR #1 読取りでも HEAD は 627d0ec82c4d33fad252547bc65462e4d2d3effd、Draft/open。別 SD 担当へ対象範囲を連絡した。APT、kconfig-frontends、実機、製品変更は対象外。

既存試験と重ならない境界として、claim/setup/command/正常 IRQ の後で、finish が chip ID・revision・D-cache 条件を再確認することを実関数接続で試験した。SCR 8 B / sector 512 B × 7 ID 値 × cache on/off の 28 例が UBSan で成功。source/template/extract 対応検査も成功した。

実装の許可する revision 0x2001/0x2003/0x2007 と device 0x450、cache off では規定の 8 B または 512 B だけをコピーした。その他の組合せは EAGAIN で QUARANTINED となり、出力全体を保持。IDMAEN は 1 のまま、追加 barrier も発生しないため、拒否に伴って停止を装う操作はない。拒否後に ID/cache の入力を戻しても成功へ復帰せず EAGAIN を保持した。新たな修正を要する問題は再現しなかった。

ID 値の変更は getter に対する故障注入であり、実 silicon revision が動的に変化することを想定してはいない。また、実装が許可する revision の試験であり、公式 errata の安全性を新たに認証するものではない。MMIO、IRQ、critical section と cache getter はホスト double。実際の cache 遷移・物理 IDMA 停止・ARM 命令・割込み競合・電源起動は未検証。対象 H753 設定の採用と電源条件 HOLD は未解消。

再現: python3 finish-platform-recheck/run.py。test.c と result.log を同フォルダに保存した。
