# lhat 側への報告事項

lhatove の移植中に見つかった、lhat 本体で直すべき事項。解決したら「解決済み」へ移す。
基準: lhat HEAD `88996f1`（2026-09-18）。

## 未解決

- **親モジュールを import した後に子を import すると SEALED で止まる**（`995d0e8` で直った件の再発。
  `d7dc455 feat: the registrations are the program's, not every machine's` 以降）。
  自身に登録を持つ親（lhatove の `love` — `love.getVersion` 等）を `import^` した後、その下の
  **どの子**（`love.system` でも `love.event` でも）を `import^` しても、その行で実行時に
  `this table belongs to the machine; what it holds is written by the host, not from here`
  （`LHAT_RUN_SEALED`）。子どうし（`love.system` + `love.event`）は通る。

  ```lhat
  module^imp
  import^love          # 自身に登録を持つ親
  import^love.event    # ← ここで SEALED
  public^let^load = p^{ love.event.quit(0) }
  ```

  `d7dc455` が登録済みモジュールの表を program 共有にして封印したので、子を親の表に結ぶ
  書き込み（`import^` の束縛）が封印に当たっていると読める。

  書いているのは `src/compile.c` の `LHAT_NODE_IMPORT_STMT`（`db2a7b9` で 6882–6887 行）:
  経路がメンバの形（`love.event`）だと、`L^.modules.love.event` を読んだ値を、接頭辞 `love` の
  ローカル（先の `import^love` の束縛）へ **`SETINDEX owner["event"] = slot`** で書き戻す。
  その `owner` が共有・封印された `love` 表なので SEALED になる。しかも書き込みは**冗長** —
  `slot` はまさにその表から読んだ `event` なので、`owner["event"]` は最初から `slot`。
  接頭辞の根が `import^` の束縛したローカルなら SETINDEX を出さない（コンパイラ側）か、
  封印された表への同じ値の SETINDEX を何もせず通す（VM 側）かで直る。子どうしが通るのは、
  `love` がローカルに無く、書き戻す相手が機械自身の背骨になるから二分探索で `12a4641`（直前）は
  通過、`d7dc455` で発生を確認。lhatove は `testing/lh/suite/tests/love.lh` がこれで落ちる
  （`import^love` と `import^love.system`）— テストは回帰の検出器なので書き換えていない

- **`lhat_machine_register` が封印を素通りする。** 封印を見るのは VM の SETINDEX だけで、
  `lhat_table_set` は `table->sealed` を見ない。だからホストが `lhat_machine_register` で
  登録済みモジュールの下に書くと、**program 共有の黒い表**に書き込みが通り、続く
  `lhat_gc_barrier_back` がその共有表を**書いた機械の gray リストへ繋ぐ** — `d7dc455` が封印で
  避けたはずの状態そのもの。黒い表の下に置かれた機械自身の表はコレクタに二度と見られないので、
  そこに係留した物が名指されたまま回収される。

  lhatove はこれで踏んだ: `ParkingLot` の表を `love.registry` に、Boot の値を `love.boot.*` に
  係留していたため、physics が**接触コールバックを全部失う**（clang 版）か**segfault**（MSVC 版）。
  lhatove 側は係留先を登録の外（`L^.modules.lhatove.*` = 機械自身の背骨）へ移して直した
  （`lh.h` の `privateRoot`）。lhat 側では、登録済みモジュールへの `lhat_machine_register` を
  **false で断る**のが良いと思う — 黙って壊れるより、ホストが最初の一回で気づける

## 提案

（なし）

## 解決済み

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
- スカラー数学関数が無い → `c7b49bf feat: std.math`（sin/cos/tan/asin/acos/atan/atan2/deg/rad/sqrt/cbrt/exp/log/log2/log10/hypot/fmod/min/max/lerp、**度数法**）、`number^` に abs/sign/clamp と `number^.pi/tau/e/inf/nan`。`Vector3` は `std.math.vector3` へ分離（`2a90e2b`）。lhatove は `std.math` のみ登録
- 公開メンバの型を C から照会 → `763c137 feat: a unit's exports answer their types`（`lhat_unit_export_type` / `lhat_unit_export_conforms`）。起動時のコールバック型検査に使用（`testing/lh/badcallback`）
