# 正常SCRのHANDOFFからsectorへ進む失敗境界

最新PR1を再照合しHEAD 627d0ec不変。前回要約の「IDLEへ戻す処理なし」を訂正した。fault/retire/endには戻す処理がないが、sector_claimは成功SCRのHANDOFFから一度だけIDLEへ戻して再claimする。隔離状態QUARANTINED/RETIREDを戻す処理ではない。前回arena-reuse-boundaryにも補足を保存した。

本回帰では、実SCR claim→8B setup→CMD55/R1→ACMD51/R1→IRQ DATAEND→finishを接続してHANDOFFまで進め、その後12種類の拒否条件を与えた。DPSMACT、CPSMACT、IDMATE、IDMAEN、cache on、未知chip、remaining、xfrmask、waitevents、waitmask、初期化中、generation UINT64_MAX。sector_claimは前11件EBUSY、overflowだけEOVERFLOWで拒否。全件で出力arena/key不変、cpu_destination撤回、sector_attempted=true、RETIREDを確認した。原因を除去しても2回目claimはEBUSY。UBSan成功。

ホストのMMIO/IRQ/CPU条件はdoubleで、実ARM・実機やDMA停止証明ではない。原典の一度だけHANDOFF再利用を実関数で確認する試験であり、強制解除の修正は不要。失敗時の状態を再開可能にする変更は加えない。製品編集・公開・ハード操作なし。

保存: run.py、test.c、result.log。H753正式SD生成configと対象ビルドの新しい証拠は本工程では得ていない。
