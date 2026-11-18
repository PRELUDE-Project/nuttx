# 非搭載stubと搭載時の必須hook

前回c3bdd294へ保存したhost分割案を基に、診断OFF時にはdriver依存のないrecover通知stub、ON時には固定driver hookを必須とする候補を実装した。ON時のweak no-opは設けない。hookがないのに安全処理を実行したと誤認しないため。

host検証: OFFはscheduler+smokeだけでlink/run成功。ONでhook未搭載はsd_recovery_node/notify未解決としてlink失敗。必要な5設定欠落と未検証6構成を前処理errorで拒否。ONで固定driverを接続した分割host UBSan回帰は成功。

初回ON回帰では古いfixtureがSDMMC1/2両方を定義しており、新gateが正しく拒否した。参照構成はSDMMC1のみであるため、この候補のhost fixtureだけSDMMC2 defineを外して再試験した。製品の設定を書き換えたり、gateを無効化したりしていない。

正式H753設定ではなく、選んだmacroのhost試験。全終了経路・NuttXの実同期・targetビルドと実機停止は未証明。既存4APIへの仮統合も製品ソースへ反映していない。実機操作、拒否迂回、新規導入なし。

再実行: `python3 experiments/sd-recovery-20261118/updates/configuration-link/check.py --repo .`。既存cc/Python/UBSanを使用し、自動導入や実機操作はしない。
