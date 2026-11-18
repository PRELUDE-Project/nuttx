# 通知解除失敗前の出力保持（未公開の設計試案）

PR #1 を今回再取得し、open/draft、HEAD be86b5194ed8f88009c3f867bda71420a5b9d161 と確認した。製品変更はない。

先行 recover-atomic-binding 試案では、finish が出力をコピーしてから unbind_notice がリスト不整合を発見すると、EIO でも出力が変更される。今回、登録後に head を NULL にする人工的な故障注入でこれを再現した。これは実OSで起きた故障ではなく、未公開API案の契約検証である。

candidate.c は同じ外側 critical section 内で、実 finish 前に登録リストへの所属と登録generationを照合する。不整合なら seal(EIO) してコピーせず返す。baseline.c は EIO と出力変更、candidate.c は EIO と出力不変をそれぞれassertし、両方 UBSan で通過した。既存の正常SCR HANDOFF、別所有者sector claim、recoverによる隔離、遅延IRQと出力保持の回帰もそのまま通過した。

この検査はリストの循環・任意メモリ破損の検証ではない。通知登録API自体が製品に存在せず、実OS TCB・スケジューラ・割込みはhostモデル。外側排他が全経路に適用される証明と既存export APIへの統合、独立レビューは未完。IDMA物理停止、正式H753生成設定、ARM build、実機確認を証明しない。製品反映・実機操作なし。

ビルド環境再確認: cloud/ThinkPadX1Gen13 は connected、X1Gen11 は offline。cloud標準PATHと /opt・/usr/local/bin の限定探索では ARM GCC と kconfig-tweak/conf は見つからず、gcc のみ。全ディスク不存在の断言ではない。承認済みkconfig-frontends導入は管理者経路不足のまま。APT拒否・sudo失敗は再試行せず、迂回していない。

## 既存環境の追加確認
ThinkPadX1Gen13 の Get-Command では ARM GCC、kconfig-tweak/conf、cmake、ninja が見つからず、wsl.exe のみ確認した。`wsl.exe --list --quiet` は Wsl/E_ACCESSDENIED。これを再試行・迂回せず、通常の環境attachで cloud に戻った。STM32CubeProgrammer ディレクトリの存在は確認したが、ビルドコンパイラや実機安全性の証拠には使わない。PCへの導入やハード操作なし。

## 独立レビューと追加回帰
独立担当は baseline/candidate の既存binaryを再実行しPASS、限定されたhead欠落条件への対策としてblockingなしと評価した。ただし post-finish unbind失敗fallback 自体は残り、全経路排他・出力非alias等の条件なしに一般的出力不変とは言えない。追加推奨された notice.generation の不一致を直接注入する baseline-generation/candidate-generation もコンパイル・UBSan実行した。基準はEIOと出力変更、候補はEIOと出力保持でPASS。両方とも人工的不整合で、実OSの故障発生証拠ではない。
