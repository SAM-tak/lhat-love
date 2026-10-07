# lhat 側への報告事項

lhatove の移植中に見つかった、lhat 本体で直すべき事項。解決したら「解決済み」へ移す。
基準: lhat HEAD `808dbb9`（2026-09-22）。

## 未解決

（なし）

## 提案

（なし）

## 解決済み

- **弱参照キャッシュが削除済みキーによって拡大し続ける**（2026-10-06、未コミット）
  → lhat `src/vm_machine.c` の再ハッシュで、実際の登録件数に空きがあれば容量を維持するよう修正。
  従来は削除済み枠で占有率が上がった場合にも常に倍増していた。
  LÖVEをリンクしないCの再現で、登録→即削除を10万回行うと生存0件でも表が1,572,864 bytesへ増大。
  修正後は384 bytesで一定。`test_hostdata_base` に削除反復と実際に64件保持した場合の拡大・参照保持を追加。
  Vanguard Princess `tests/repro-weak-cache.c` と `TechnicalDocuments/0064-memory-heap-audit.md` を参照。

- **複数 task ワーカーによるメンバー誤読・二重解放**（2026-10-04）
  → lhat `56cf7cf fix: transfer task results once and isolate member caches per VM`。
  共有 chunk 上のメンバーキャッシュを VM ごとに分離。
  LÖVE 不要の20行の再現では、修正前に2ワーカーで10/10回別ワーカーの値を読み、
  修正後は10/10回正常。Vanguard Princess の読み込みキャンセル→再入場→試合開始も
  2ワーカーで3/3回通過。詳細とログは同プロジェクトの
  `TechnicalDocuments/0029-core-member-race-repro.md`。
  以前のゲームの ASAN 二重解放は通常ビルドで再現しなくなった。修正後の ASAN は未再実施。
  ローカルビルドはこの修正版を使用。修正コミットが origin に未公開のため、
  `lhat.rev` はまだ更新していない。CI / 配布用の固定先は公開後に更新・再検証する必要がある。

- **親モジュールを import した後に子を import すると SEALED**（`995d0e8` で直った件の、`d7dc455` による再発）
  → `b10f3fe fix: nobody writes the program's shared tables, and the host gets a root`。
  原因は `src/compile.c` の `LHAT_NODE_IMPORT_STMT` で、`import^love.event` が読んだ子を接頭辞
  `love` のローカル（`import^love` の束縛 = 共有・封印された表）へ SETINDEX で書き戻していた。
  書き戻す先に同じ物が既にあれば書かない形（GETINDEX / SAME / NOT / JUMP_FALSE）になった。
  子を先に取り込んだ時の仮置きを親の表で置き換える場合と、何も無い場合は従来どおり書く。
  lhatove は `testing/lh/suite/tests/love.lh`（`import^love` + `import^love.system`）が通るように
  なり、suite は 472/472（検出器として書き換えずに待っていた）
- **`lhat_machine_register` が封印を素通りしていた** → 同コミット。共有表に `shared` の印が新設され
  （登録済みモジュールの表と enum の members 表）、書き込みが必ず通る `vm_set_key` の入口 1 か所で断る。
  `lhat_machine_register` は false、`lhat_machine_table_set` は true + `refused`。既存の `sealed` では
  区別できなかった（L^ 環境表・enum の members 表はホストが正当に書く）し、色でも見分けられない
  （機械自身のオブジェクトもマーク中は黒）。そして**ホストの根**
  `lhat_machine_host_root(machine)` ができた — 機械が根として持ち、L^ から名前で辿れない表
  （Lua の registry）。lhatove が `love.registry` / `love.boot.*` に係留して physics の接触
  コールバックを失った（clang 版）／segfault した（MSVC 版）件は、いったん `L^.modules.lhatove.*`
  （どの登録も作らない背骨）へ逃がして直し、この根ができてからそちらへ移した（`lh::park`）

- 弱参照キャッシュ（復活付き）が欲しい → `78b72ec feat: the collector keeps the cache a host cannot keep for itself`。`lhat_machine_weak_cache_get` / `_put` / `_forget`（05 の 8.12、`vm.h`）。鍵はホストのポインタ（番地で比べるだけで指す先は読まない）、値は弱く、マシンごとの表。保証は `gc.c` の `atomic()` — 最後の `follow_gray` の後・白の入れ替えの前で、到達しなかったエントリをコレクタが外す。`get` は `LHAT_GC_PROPAGATE` の間に答えた値を灰にする（**求めること自体が再到達**）。lhatove の `pushObject` はこれに載っている（下の項の続き）

- `a8d91a1` が physics を壊す（`reach_table` が狭い読みの上に作る／`m->modules` が根でない）→ `20b1cbe fix: a walk that creates over what it did not find asks the real read first`。**ただし physics はこれでも落ち続け、真因は lhatove 側だった** — 下記
- **ホスト側のラッパキャッシュは漸進 GC の下で不健全だった**（lhatove `pushObject` の `lh::WrapperCache`。上の弱参照キャッシュに載せ替えて解決）。
  「ホストの map は根でないから、包みは今までどおり回収され、`dispose^` がエントリを外す」
  という理屈は各文は真だが結論が偽。lhat の収集は**漸進的**（`gc.c`「Lua's incremental
  collector, borrowed」）で、マークとスイープの間でプログラムが走る:

  1. マークが終わり、誰も名指していない包みは白
  2. スイープが届く前に `pushObject` がキャッシュからそれを答え、L^ が生きた場所へ入れる
  3. スイープは 1 の判定どおり解放する

  Lua の弱値表が与えているのは「根にしない」ではなく「**コレクタがマークの終わりに
  エントリを自分で消す**」こと。ホストの map には知らされる口も復活させる口も無い。
  症状は physics の約半分で、Body のメンバが self 無しで届く／instruction given the wrong
  type — 解放済みを読んだ形。**キャッシュを外して 20/20 通過**（有りは 13/20）。
  いったん毎回新しい包みを作る形に戻し、lhat に弱参照キャッシュが入ってからそちらへ載せ替えた
  （physics 20/20、churn は 120 フレームで collected 1,582 → 1,164 / live 8,939 → 7,643）。
  suite の同一性チェックは `is^` をやめ `=` を訊く形のまま — `is^` はキャッシュの当たり外れ次第で、
  lhat が約束しているのは `=`（tag + ポインタ）だけ

- 呼び出し文の直後の `try^{ }` が命令モードの呼び出しに読まれる → `f43f8b1 fix: a call statement no longer swallows the word that opens the next one`。`test.begin("std.channel")` を `try^{ }` の外へ戻した（`testing/lh/suite/tests/thread.lh`）。`try^{ }` 自体はその後 `db2a7b9` で廃止され、`catch^` の腕はブロック一般の節になった（`do^{ … catch^: … }`）
- ワーカーの失敗文が panic の中身を落とす → `stdlib/thread.c` の `failure_text` が `fault_text` と `fault_line` を綴るようになった。`threaderror` が `panic^: "boom from a thread" (line 75)` と言う（以前は `panic^` の 1 語）

- DAP のヘッダに `extern "C"` ガードが無い／パス照合がディスク上の実パス前提 → `781b2cb fix: the adapter speaks the host's paths, and the headers speak C++`。ガードは `dap/*.h` に加え `transport/transport.h`・`port/socket.h`・`port/thread.h` にも入った（lhatove 側の `extern "C" { #include }` 包みは撤去）。照合は `DapPathMap`（`to_unit` / `to_editor` の双方向、NULL で従来どおり）になり、`_fullpath`/`realpath` を通らなくなった。lhatove は PhysFS のマウント（`getRealDirectory`）で写像を書き、VS Code が送る絶対パスのままブレークポイントが結ばれ、スタックの `source.path` も同じ綴りで返る。`.love` の中の単位はディスクに無いので `to_editor` が false を答え、単位名のまま報告される

- 親モジュールとその子モジュールを同じスコープに import できない（`import^ love` と `import^ love.graphics`）／ 同じ位置に異なる hostdata 型を置いたアームが登録で「重なる」と拒否される → `995d0e8 fix: a namespace and one under it import together; registered types are disjoint`。lhatove 側: `newThread` を `p^File;` / `p^FileData;` の 2 アームへ戻した（実行時もタグで解決 — File・FileData 双方からスレッドが起動するのを確認）、`testing/lh/suite/tests/love.lh` は `import^ love` と `import^ love.system` を同居させた綴りに戻した。`love.event.restartValue` は移さない（`love` 直下を薄く保つ方針は変わらず妥当）
- `lhat_program_install` のランタイム型爆発（相互参照する hostdata 型をメンバ付きテーブルとして再帰展開）→ `fea90e4 fix: a registered type lowers to one nominal node, not its members`（hostdata_tag を持つ型は `LHAT_TYPE_RT_HOSTDATA` の葉 1 個。5 型×8 メンバ相互参照が test_program に pin、live < 1000）。lhatove: 起動直後 live 2,651,187 → 2,477、`testing/lh/physics` 完走 ≈10 分 → 5 秒、`m3` の mapPixel も即時。副産物: hostdata 引数のオーバーロード解決がタグ比較で効くようになった。再現 [repro/install_blowup.c](repro/install_blowup.c)
- `$"..."` 補間スロットのタプル → 同コミットで検査器が `TUPLE_MISPLACED` を静的に報告（02 の 13.8改「置けない」一覧に補間の穴を追記）。lhatove 側は `let^ gx, gy = world.getGravity()` で受ける綴りのまま
- `lhat_machine_call` の「ハング」は上記の遅さの誤認。[repro/call_arity.c](repro/call_arity.c) は 3/7 引数・コルーチン内・registry 経由で 20 万回通過する確認として残置
- `lhat_program_install` の無限ループ（自型を返すメンバ + 自型を取るメンバ、実態は 3^32 歩）→ `3a4376c fix: a host type that answers and takes itself installs`（program.c の独自型下ろしを `lhat_machine_rt_from_checked` に一本化、`lhat_machine_make_type` は廃止）。再現 [repro/install_loop.c](repro/install_loop.c) は通過。`Transform.apply` を型付きに戻した
- ホスト登録署名の `Self^` → 同コミットで `lhat_register_member` / `_hostvalue_member` の署名中の `Self^` が登録先の型に解決される。lhatove の Transform メンバは `p^self^, Self^ -> Self^;` 綴り。モジュール関数 / global では従来どおり誤り
- 可変長アームと他アームの重複判定 → 同コミットで「書かれた位置で型が交わらない、または片方に置き場の無い個数がある」なら別アーム。`print` は `p^string^;` + `p^string^, number^, ...;` + `p^string^, love.graphics.Font, ...;` の3アーム。`f(string^, ...)` と `f(string^, Font, ...)` の組は引き続き拒否（2引数の呼び出しが両方に収まるため）— 尾の前に型の交わらない位置を置く
- `lhat_type_of_text` の heap-use-after-free（構造型メンバ名がソースバッファを指したまま解放）→ `10e810e fix: a type's member names are the arena's own, not the source's`
- `stdlib/*.h` に `extern "C"` ガード無し → 追加済み。lhatove 側の包み込みは撤去
- `stdlib/math.h` のインストール漏れ → 追加済み
- ホスト関数から panic を起こす経路 → `bdf8fb9 feat: lhat_machine_panic`。`lh::raise` が `lhat_machine_panic_text` を呼ぶ（`testing/lh/raise`）
- スカラー数学関数が無い → `c7b49bf feat: std.math`（sin/cos/tan/asin/acos/atan/atan2/deg/rad/sqrt/cbrt/exp/log/log2/log10/hypot/fmod/min/max/lerp、当時は**度数法**。`abe8c9b` で Lua・LÖVE と同じ弧度法になり、`deg` / `rad` は変換として残った）、`number^` に abs/sign/clamp と `number^.pi/tau/e/inf/nan`（`743cf41` で pi/tau/e は `std.math` へ移り、`number^` に残るのは inf/nan）。`Vector3` は `std.math.vector3` へ分離（`2a90e2b`）。lhatove は `std.math` のみ登録
- 公開メンバの型を C から照会 → `763c137 feat: a unit's exports answer their types`（`lhat_unit_export_type` / `lhat_unit_export_conforms`）。起動時のコールバック型検査に使用（`testing/lh/badcallback`）
