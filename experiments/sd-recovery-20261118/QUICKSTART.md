# 利用者向け: 保存済み SD 調査・回帰の試し方

## 過去の実機試験と今回の未検証範囲（2026-11-18 訂正）

SD の実機試験は実施済みです。2026-11-13、NUCLEO-H753ZI Rev V、電源3.28 Vで `sdwriteprobe --write-restore-once 0` を実行し、512 B の書込み・読戻し・元データ復元に成功しました。一次ログ確認担当の照合結果は `pattern_verified=1 restored=1` です。使用 BIN は153832 B、SHA-256 は `92df6f0ecc664e6cf1b53abd26094e39058f6e73ef18a7f125ae9722012279aa`。

根拠は保存済み `sd-h753-write-evidence-20261113.zip`。確認担当によれば、収録10ログは付属ハッシュと一致しています。ただし一覧にある `before-test.log` は未収録で、ログ内にソースコミットSHAはありません。このため、その BIN と現在の PR HEAD・終了通知候補との同一性は確認できていません。ここでは担当が確認した一次証拠の引継ぎとして記載し、アーカイブ全体をこのリポジトリへ転載したものではありません。

「実機未検証」は、この experiments の未統合候補や現在の差分に対する検証が未完了という意味です。過去の実機試験を実施していないという意味ではありません。過去の正常な書込・読戻し・復元成功から、今回の task_delete/restart、timeout競合、物理IDMA停止まで検証済みとは判断しません。


## いま共有されているもの
[PR #1](https://github.com/PRELUDE-Project/nuttx/pull/1) は2026年11月18日に `releases/12.10` へマージ済みです。下記のSCR専用ビルドは、その後の[作業ブランチの候補](https://github.com/PRELUDE-Project/nuttx/commit/ce3dc41a93893bbb8fba6f658265cdf070e39629)です。`experiments` 配下の終了通知試案・ホスト試験は製品実装ではありません。ドライバーへコピーして使わないでください。

## ホストで再現する
Python 3、C11 対応の cc、UBSan が既に使用可能な環境で、リポジトリのルートから実行する。次のスクリプトは一時ディレクトリに試験バイナリを作り、実機には接続しない。必要なツールを自動インストールしない。

```sh
python3 experiments/sd-recovery-20261118/replay.py --repo .
python3 experiments/sd-recovery-20261118/updates/configuration-link/check.py --repo .
```

最初のコマンドは15件の試案回帰。二番目は診断OFF時のstub、診断ON時の必須hook、不支持構成の拒否を確認する。二番目の検査では意図的なコンパイル・リンク失敗を検査の成功条件としている。全体の終了コードと出力で判断する。

失敗時は使用コミット、Python/cc の版、実行コマンド、終了コードと標準エラーを保存する。失敗を隠すために構成ガードを外さない。比較用の旧実装が含まれるため、個別バイナリが PASS と表示しても製品の安全性全体を意味しない。

## 実機ではまだ行わないこと
この手順からフラッシュ、カード書込み・消去、リセット、電源設定変更へ進まない。SCR専用と1セクター読出し診断付きのARMビルドは下記のとおり確認済みです。正式採用するH753設定、起動時の電源条件、配線・カード個体、実機でのIDMA停止と所有者終了後の安全性は未確認です。参考設定を正式設定の代用にしない。隔離済みバッファを停止完了の根拠なしに再利用しない。

ホスト回帰やSCR専用のビルドが成功しても、この候補の実機試験を開始できるとは限りません。コードと検証記録を確認し、実機でしか確かめられない条件を分けて扱います。

## SCR専用のH753ZI対象ビルド（2026-11-26 JST）

NuttX候補 `c1fb8bedc304eec12d063a2c5fe547b84bf28430` とNuttX Apps `8ff533beb37eeef1f288c467acd7c50b542464eb` を使いました。本コミットのボード定義2か所の修正を加え、Arm GNU 15.2.Rel1（GCC 15.2.1）とDebian kconfig-frontends-nox 4.11.0.1+dfsg-6で設定生成からリンクまで行いました。

- SCR専用構成でセクター読出しを無効にすると、SDMMC1の端子定義まで外れていました。SCR専用でも同じ端子を初期化するため、定義が有効になる条件を診断の共通設定に合わせました。
- 次に、ボードヘッダーに存在しないLEDの名称をユーザーLED処理が参照していたため、ヘッダーが定義する名称に合わせました。SDカードやバス制御の動作は変更していません。

設定例は [保存済みの参考設定](archive/REFERENCE-SD-CONFIG/candidate.config) を別の作業領域にコピーし、`CONFIG_SD_SECTOR_READ_REVIEW` だけを無効にしました。H743ZI2由来のボード用 `scripts/Make.defs` を使い、`tools/diagnostics/scrdiag/app` の4ファイルを別のApps作業ツリーの `system/scrdiag` に配置して、`tools/configure.sh -l -a <Apps> <設定ディレクトリ>` と `make -j4` を実行しました。生成後の設定でH753ZI、SDMMC1、IDMA、SCR診断、読取り専用を確認し、D-cacheと診断用のセクター読出しは無効です。ファイルシステム全体のマウント機能を無効にした構成ではありませんが、FATとNSHの `mount`／`umount` コマンドは無効です。

- 結果：ARM ELF32、未解決シンボル0件。`scrdiag_main` と `mmcsd_scr_diagnostic` を含みます。
- 対象ELFを逆アセンブルし、`scrdiag_main` が SDIO 初期化、メディア通知、`mmcsd_scr_diagnostic` の順に呼ぶことを確認しました。`mmcsd_sector_readonce` と `mmcsd_slotinitialize` は対象ELFにありません。この入口からはカードを初期化してSCRを読むところまでを試し、セクター読出しやファイルシステムのマウントを試しません。
- `nuttx.bin`：133,588バイト。SHA-256 `67fbbb95c45aae109fe502c71b91cb9d88286fd7ed41119ca638bea165d33ab6`。
- ELFのシンボルと型情報を照合し、SCR用の32バイト領域がAXI SRAMの `0x240004C0`〜`0x240004DF` にあり、先頭アドレスが32バイト境界にそろっていることを確認しました。これはリンク後の配置確認で、実機のIDMA動作を確認したものではありません。

このビルドはSCR診断の比較検証用です。元の実機用設定を復元したものではなく、配線、起動時の電源条件、IDMAの停止、カードとの通信は確認していません。実機への書き込みやカード操作は行っていません。

## SCRと1セクター読出しを含むH753ZI対象ビルド（2026-11-26 JST）

SCR専用とは別の作業領域に、同じNuttX候補 `c1fb8bedc304eec12d063a2c5fe547b84bf28430`、Apps `8ff533beb37eeef1f288c467acd7c50b542464eb`、上記のボード定義2か所を展開しました。今度は参考設定の `CONFIG_SD_SECTOR_READ_REVIEW=y` を維持し、H753ZI向けにビルドしました。AppsはNuttXディレクトリーからの相対パス（`-a ../apps`）で指定しました。GCC 15.2.1とkconfig-frontendsの版はSCR専用ビルドと同じです。

- ARM ELF32、エントリ `0x080002D9`。未定義シンボルは0件です。ELFには `scrdiag_main`、`mmcsd_scr_diagnostic`、`mmcsd_sector_readonce` が含まれます。IDMAとMMCSDの読取り専用設定は有効、D-cacheとFATは無効です。
- `nuttx.bin`：136,356バイト、SHA-256 `ffbb1f27f83b76adb4770d2ea3f9f7504ab322cd1501052a583bdf8ae1742145`。
- 生成した `.config` のSHA-256：`3842f9e12cedbcd098087d7c97652ad8688c2445a79bc85207ece833e85dd93c`。

ELFのシンボルと型情報も照合しました。SCRと1セクターで順に使う共通バッファはAXI SRAMの `.data` 内、`0x240004C0`〜`0x240006BF` の512 Bです。先頭は32 B境界にそろい、バッファの次（`0x240006C0`）から管理状態が始まります。この確認はリンク後の配置だけを示し、タイムアウト後にIDMAのアクセスが止まることや、安全に再利用できることは示しません。

このビルドでは、SCR初期化後に指定した1セクターを読む診断コードまでリンクされます。カードから実際に読めることや、読出し失敗後にIDMAが停止していることを示す結果ではありません。書込みやファイルシステムの試験用ではなく、実機にファームウェアを書き込んだりカードを操作したりしていません。

## 配線を照合する際の資料

NuttXの設定ではD0〜D3がPC8〜PC11、CKがPC12、CMDがPD2です。[STの評価ボードマニュアル UM2407 Rev.6](https://www.st.com/resource/en/user_manual/um2407-stm32h7-nucleo144-board-stmicroelectronics.pdf)の表19では、対応するCN8端子は順に2、4、6、8、10、12です。MorphoのCN12を使う場合、D0とD1はそれぞれSB14とSB15が閉じた個体でのみCN12につながります（表14）。試験に使うコネクターと導通は未確認です。カードが応答しない場合、ファームウェアだけを原因と判断しません。
