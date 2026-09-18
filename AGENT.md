# AGENT.md

このファイルは、lhatove リポジトリで AI アシスタントが守るべき実務ルールを定義します。

## プロジェクト概要

lhatove（lhat + löve）は [love2d/love](https://github.com/love2d/love) 12.0 のフォーク。
スクリプト言語を Lua/LuaJIT から自作言語 [L^ (lhat)](https://github.com/SAM-tak/lhat) に置き換える。

- エンジン本体: C++17 / CMake
- L^ ランタイム: C11 静的ライブラリ（`../lhat` を `add_subdirectory` で取込）
- 当面 Windows（megasource + MSVC）ビルドのみ対象

まだ実用に供されていないので、**後方互換性を保つ必要はない**。upstream love2d との互換も目標ではない。

移植の全体計画・設計判断は @docs/porting/lua-to-lhat.md、進捗は @docs/porting/status.md を参照。

## ビルド

megasource（love2d 公式の Windows 依存関係一括ビルドリポジトリ）を親として CMake を実行する。
`scripts/build.ps1` が megasource の取得・`megasource/libs/love` ジャンクション作成・CMake 実行まで行う。

```powershell
.\scripts\build.ps1              # Release ビルド（clang-cl、デバッガ入り）
.\scripts\build.ps1 -Config Debug
.\scripts\build.ps1 -Shipping    # 配布用: デバッガを外す → build-shipping\
.\scripts\build.ps1 -VmOnly      # front end を外す → build-vmonly\（-Shipping と併用可）
.\scripts\build.ps1 -Msvc        # cl.exe で → build-msvc\（他と併用可、名前に -msvc が付く）
```

**処理系は clang-cl が既定**（Visual Studio 同梱の `VC\Tools\Llvm\x64\bin\clang-cl.exe`）。lhat の VM は Clang で computed goto、MSVC で switch のディスパッチになり（lhat `src/vm.c`）、lhatove 上で測った VM 負荷はすべて同等以上に速い（中央値、PGO 無し）:

- while 3M 回 205.7 → 131.7 ms（0.64x）、for 3M 回 157.1 → 98.2（0.63x）
- メンバ呼び出し 1M 回 451.4 → 374.1（0.83x）、table 200k 80.4 → 67.3（0.84x）
- 再帰 fib(27) 77.5 → 72.4（0.93x）、ホスト関数 500k 回 169.3 → 164.9（0.97x。境界の向こうの C++ が支配）

**Clang は Ninja Multi-Config で組む**。VS ジェネレータの `-T ClangCL` は**別の Installer コンポーネント**（MSBuild 用の ClangCL プラットフォームツールセット）が要り、無いと `MSB8020` で止まる。コンパイラ自体はそれを要らず、要るのは MSVC の環境（ヘッダ・ライブラリ・rc.exe）だけなので、`build.ps1` が vswhere で `vcvars64.bat` を自分のプロセスに読み込み、ninja・clang-cl とも VS 同梱のものを使う。`CMAKE_NINJA_CMCLDEPS_RC=OFF` は `love.rc` のため — Ninja は資源スクリプトの include を C コンパイラで走査するが、clang-cl は UTF-16 の `love.rc` を読めない（rc.exe 自体は読む）。megasource の LuaJIT は `msvcbuild.bat` を vcvarsall の中で回す外部プロジェクトなので**常に cl.exe** — 誰もリンクしていないので害は無い。`/MP` が clang-cl で unused と警告されるのは megasource の `if(MSVC)`（clang-cl でも `MSVC` は真）由来で無害。

ジェネレータの違う木には組めない（CMake は後から変えられない）。`build.ps1` は既存の木のジェネレータを見て、違えば「消すか `-BuildDir` を」と言って止まる（自分では消さない）。

**VM のみビルドの生成物は処理系に依らない** — バイナリの指紋は構成上限・命令数・命令幅・`LHAT_VERSION` だけ（lhat `src/serialize.c`）。clang で作った `Signatures.h` の MSVC 版 VM も、その逆も動く（確認済み）。

L^ ランタイムの場所は CMake オプション `LHATOVE_LHAT_DIR`（デフォルト `../lhat`）。

**配布用ビルド**（`-Shipping` = `-DLHATOVE_WITH_DAP=OFF`）は L^ のデバッガを丸ごと落とす — DAP アダプタ（`lhatdap` は生成されない）だけでなく、`LHAT_WITH_DEBUGGER=OFF` で VM 側の line hook も。2 つは連動する必要がある（lhat 側で `LHAT_BUILD_DAP` は `LHAT_WITH_DEBUGGER` を要求する）。実測で love.dll が 34KB、`lhat.lib` が 16KB 小さくなる。`--dap` を渡すと「this build carries no debugger」と言って普通に走る。

**VM のみビルド**（`-VmOnly` = `-DLHATOVE_VM_ONLY=ON` → `LHAT_WITH_FRONTEND=OFF`）は字句解析器・構文解析器・検査器・コンパイラを落とす。**ゲームは先にコンパイルして配る**（下記）。`-Shipping` とは独立で、4 通りすべて動く（実測 `lhat.lib` / `love.dll`）:

- `build` 1,744,124 / 8,326,144
- `build-shipping` 1,550,352 / 8,210,432
- `build-vmonly` 804,266 / 8,160,768
- `build-vmonly-shipping` 792,464 / 8,130,048

`lhat.lib` は半分以下になるが `love.dll` は 196KB しか縮まない — 署名表（114KB）を埋め込むため。**利得はサイズではなく起動**: 約 800 の登録署名がテキストの解析から表引きになり、フル版 4.2ms → VM 版 0.0ms。ゲーム側の check / compile も消える（17 ユニットの suite で 7.6+4.3ms → 4.2+1.1ms）。

**VM 版で動かないもの**: `love.filesystem.load` / `std.load` のテキスト — 実行時に構文解析器が要る。**スレッドは全部動く**（`std.thread.spawn` の本体は同じユニットの閉包で、運ぶのは proto の参照と捕捉の写しだけ。`testing/lh/thread` を丸ごとコンパイルして VM 版で exit=8）。テキストのユニットを食わせると `this build has no front end; only a binary unit runs` と言って断る。

### 配布物を作る（VM 版に食わせるもの）

フル版が材料を作り、VM 版が読む。

```powershell
.\build\love\Release\lovec.exe --compile-game out\mygame game\    # ユニット→バイト列、conf.lton も、他のファイルはそのまま複製
.\build-vmonly\love\Release\lovec.exe out\mygame                  # 走る
```

`--debug-names` を添えるとローカル名と捕捉名（09 の 4 章）が残る。行番号は**どちらでも残る**ので traceback は常に読める。既定は落とす。

**エンジン側の生成物を作り直すのは、登録か埋め込みユニットを変えた時だけ**（3 つとも git に入っている）:

```powershell
.\build\love\Release\lovec.exe --dump-signatures sigs.bin testing\lh\hello
.\scripts\bin2header.ps1 -In sigs.bin -Out src\lh\Signatures.h -Name lh_signatures

.\build\love\Release\lovec.exe --dump-embedded emb\ testing\lh\hello   # Boot.lh
.\build\love\Release\lovec.exe --dump-embedded emb\                    # nogame（main.lh の名で出る）
.\scripts\bin2header.ps1 -In emb\Boot.lh.bin -Out src\lh\BootBinary.h -Name lh_boot_binary
.\scripts\bin2header.ps1 -In emb\main.lh.bin -Out src\lh\NogameBinary.h -Name lh_nogame_binary
```

署名表は登録と 1 対 1 で、噛み合わなければ VM 版が起動時に `the signature table this build carries does not fit its registrations` と言う。埋め込みユニットは VM 版が自分で持つので、`--compile-game` は書き出さない。

**fused はビルド構成ではない。** 同じ実行ファイルに `.love` を連結すると fused になる（`copy /b love.exe+game.love mygame.exe`）ので、配布物の土台に何を使うかは -Shipping / -VmOnly で選ぶ。実行時の fused 判定は別にあり、そちらは `--dap` を捨てる（両方効く）。`.love` は縮むとは限らない — PhysFS の zip は deflate を展開するので、既に密なバイナリユニットは圧縮が効かない（realgame 実測 4,496 → 5,862 バイト）。

## 移植規約

### 命名

- コアグルー層: `src/lh/`、namespace `love::lh`（LÖVE 旧来の `luax_*` ヘルパ層の役割を引き継ぐ。L^ 言語自体とは無関係の内部呼称）
- モジュール別バインディング: `src/modules/<mod>/lh_<Thing>.cpp`（`wrap_<Thing>.cpp` の隣に並置）
- ホスト関数: `lh_<name>`（static）、モジュールレジストラ: `lhopen_love_<module>(lh::Context&)`
- include: L^ のヘッダは `<lhat.h>` / `<lhat/xxx.h>`、グルー層は `"lh/lh.h"` 等

### 登録

- lhat の登録は check 前に完結が必須。レジストラは **2相**（TYPES 相で `lhat_register_type` / `lhat_register_hostdata_type`、MEMBERS 相でメンバ・関数登録）。シグネチャは既登録型しか参照できないため
- 登録 context はプロセス寿命のファイルスコープ static でよい（雛形: `../lhat/stdlib/io.c`）。lhat は「登録呼び出し＝宣言」として hostdata タグ・host value タグ・エラー種を**プロセス単位で intern** するので（`676b8d1`）、program をいくつ作っても identity は 1 つ。context の中身も program ごとに作り直す理由が無い。ただし program 固有のポインタ（`LhatProgram *` 等）を入れるなら、restart で再登録が必ず走って更新されることが前提。`lh::Errors` / `lh::TypeRegistry` も同じ理由で `Runtime` の値メンバをやめプロセス寿命にした（`lh.cpp` の `sharedErrors` / `sharedRegistry`）
- 登録が program に預けた state を返す口が `lhat_program_on_dispose`。lhatove では使わない — static context には返すものが無く、program 寿命の heap 資源は `ParkingLot` だけで `Runtime` のデストラクタが片付ける
- プロセス共有 registry は `lhat_registry_dispose()` で返す。**LhatProgram が 1 つも無い時のみ**呼べるので、呼ぶのは restart ループを抜けた後（`love_lh_shutdown()` ← `src/love.cpp`）
- C 側で保持する L^ 値は GC ルートにならない。永続値は `lh::park` で **`L^.modules.lhatove.*`** に係留する（`ParkingLot` の表も `lhatove.registry`）。**登録済みモジュール（`love.*` 全部、`std.*`）の下に置いてはいけない**: lhat `d7dc455` 以降それらの表は program の全機械で共有され封印されていて、`lhat_machine_register` は封印を素通りして書けてしまう（lhat 側の穴、報告済み）。共有の黒い表の下に機械自身の表を置くとコレクタが二度と見ず、係留した物が名指されたまま回収される — physics が接触コールバックを全部失う／segfault した（`lh.h` の `privateRoot`）。`lhatove` という名前はどの登録も作らないので、機械自身の背骨になる
- lhatstdlib は選別登録: `error` / `debug` / `regex` / `load` / `math` / `lton`。conf は **conf.lton**（LTON = テーブルリテラルの中身）で、check も compile もされず `lhatstdlib_lton_load` が program の loader 経由で読む — 本文は `f^` として読まれるので `p^` を呼べず、`love.*` はスコープにも入らない。ゲームも `std.lton.load` で同じ綴りのデータファイルを読める。`std.io`（love.filesystem が担当）と `std.math.vector3` は登録しない
- **並行処理は言語のもの**: `std.thread` / `std.channel` / `std.async` を登録し、**love.thread は廃止した**。ワーカーの本体は同じユニットの閉包（`std.thread.spawn(p^ ... { ... }, args)`）で、carry が proto の参照と捕捉のスナップショットを運ぶ。チャネルは `std.channel.new()` / `.named(name)`。エンジンが足すのは 2 つだけ — `threaderror` イベント（`lhatstdlib_thread_on_finish` がワーカーのスレッドから `event::Message` を積む。`p^string^`）と、`Runtime` のデストラクタが program 破棄前に呼ぶ `lhatstdlib_thread_join_all` → `lhatstdlib_channel_forget_named`
- **LOVE オブジェクトは機械を跨ぐ**: `Context::objectType` が全 hostdata 型に `lhat_register_hostdata_shared(retain, let_go)` を宣言する（`love::Object` の参照カウントは atomic）。だから ImageData も Channel も Texture も `spawn` の引数・チャネルの中身として渡り、**それを含む table** も運べる。Lua 版が任意の `love::Object` を Channel に通せたのと同じ範囲 — 別スレッドで**使って**よいかは Lua 版と同じく呼び手の責任
- **program の書込ロックは lhat が持つ**: `Runtime` のコンストラクタが `lhat_program_set_lock` に lhatove の mutex を渡す。だから check / compile / load / install / invalidate / reload の全部が覆われ、**std.thread のワーカーが自分でする install** も含まれる。対は**入れ子にならない** — 呼ぶ側が手でロックを取ってはいけない。危険な再入は 1 経路だけで、**ホストの loader はロック保持中に呼ばれる**（`lhat_program_check` の内側）ので、`PhysfsLoader` は program に触れてはならない
- エラー宣言はモジュールごと（04 の 2.4）。失敗しうるモジュールが TYPES 相で `ctx.errorKind(m, variants, n, out)` を呼び、`love.<module>.Error` を宣言する。variant は**何が起きたか**で命名する（`CouldNotLoad` / `ShaderFailed` / `Rejected`。「どの層が気づいたか」を表す `IO` / `Misuse` を全モジュールで共有しない）。1 variant なら葉を書かず宣言名だけでシグネチャに載る（`-> love.audio.Source|love.audio.Error`）。受け側は宣言名でも葉でも `fits^` で絞れる
- プログラマエラー（不正な enum 等）は `lh::raise` = `lhat_machine_panic_text`。失敗しうる API（IO 等）だけがエラー値をシグネチャに書く
- ホスト関数は `void`（`16caa92`）。答えは machine が渡す room に書く — `answers[0] = v; *answerCount = 1;`、タプルなら `answers[0..n]` と `*answerCount = n`。`*answerCount` は 0 で届くので `p^` と `dispose` は何もせず返る。`LHAT_MAX_TUPLE` より広い戻り値は登録が拒否されるので、room があふれることはない
- `lh::guard` / `lh::catchexcept` は void 本体を取る（答えは本体が room に書き終えている）。`catchexcept` だけ room を受け取る — 例外が起きたら書かれたものをエラー値 1 個に差し替えるため。`lh::raise` は panic なので、呼んで `return;` するだけ
- メインループは埋め込み `Boot.lh` の `run`（yieldable `p^`）。C++ は `lhat_machine_resume` を毎フレーム呼ぶだけ。optional なコールバックの解決は C++ 側の handlers 構築で行う（L^ では「あれば呼ぶ」を静的に書けない）
- 前提 lhat は HEAD `88996f1` 以降（`lhat_machine_panic`・`lhat_unit_export_conforms`・std.math・署名中 `Self^`・可変長アームの位置判定・登録型のランタイム型が葉 1 個・親と子の同時 import・登録型どうしは交わらない・登録の identity はプロセス単位で intern・`lhat_registry_dispose`・`lhat_program_on_dispose`・`lhat_program_invalidate`・std.lton と `lhatstdlib_lton_load`・ホスト型の親宣言・ホスト境界が count で答える・ホスト型の親宣言・DAP と `lhat_reload`・バイナリユニットと署名表と `LHAT_WITH_FRONTEND`・hostdata の共有契約・std.channel・ワーカーの完了通知・`lhat_program_set_lock`・弱参照キャッシュ・登録済みモジュールの共有と封印・`dap_session_begin` の program と言語）
- 自型を返す/取るメンバは `Self^` で書く（`p^self^, Self^ -> Self^;`）。オーバーロードは「書かれた位置で型が交わらない or 個数で分かれる」こと。`f(string^, ...)` と `f(string^, Font, ...)` は拒否される — 尾の前に交わらない位置を置く（`print` の3アーム参照）
- デバッガは lhat の DAP アダプタ（09 章）。`lovec --dap=PORT game/` でポートを開いて待ち、繋がってから起動列を進める。`LHATOVE_WITH_DAP`（既定 ON）で `lhatdap` をリンクし、OFF で VM 側の line hook ごと落とす（`scripts/build.ps1 -Shipping`）。**fused では実行時にも無効**（配布物がポートを開かない。`Boot.cpp` が `--dap` を捨てる）
- **ワーカーも対象**。バインディングは何もしない — `lhat_debug_watch_machines` が `lhat_machine_new` を拾うので、`std.thread` が作った machine がそのまま DAP のスレッドになる（確認: spawn した閉包の中にブレークポイントが効き、threadId 2 で止まる）
- ブレークポイントのパスは `DapPathMap`（09 の 5.2）で写す。`Boot.cpp` の `toUnit` / `toEditor` が PhysFS のマウント（`getRealDirectory`）を使い、エディタの絶対パス ↔ 単位の綴りを両方向に翻訳する。だから VS Code が送る絶対パスのままブレークポイントが結ばれ、スタックの `source.path` も同じ綴りで返る。`.love` や fused の中の単位はディスクに無いので `to_editor` が false を答え、単位名のまま報告される（埋め込みの `Boot.lh` も同じ）
- 起動がおかしい時: 環境変数 `LHATOVE_TRACE=1`（起動列トレース）、`LHATOVE_SKIP_REGISTRATIONS=love.x.f,love.y.T.m,love.z.*`（登録を外して二分探索。`*` で前方一致）、`LHATOVE_GC_STATS=<n>`（n フレーム毎に collected/live）
- 止まる・遅い時: `LHATOVE_WATCHDOG=<秒>` でフレームが止まった主スレッドのスタックを base+offset で stderr へ出力 → `scripts/symbolize.c`（`cl symbolize.c dbghelp.lib`）で `.pdb` から名前解決。symbols は `cmake --build build --config RelWithDebInfo --target lovec`（`SDL3.dll` / `OpenAL32.dll` を `build/SDL3/RelWithDebInfo` 等から `build/love/RelWithDebInfo` へコピー）。stderr を PowerShell のパイプに流すと書込で止まって見えるので、ファイルへリダイレクトする
- C 側が L^ の値を保持する時は `lh::Parked`（`lh::ParkingLot` の整数スロットに係留、`StrongRef<love::Object>` で持てる）。コールバックは `lhat_machine_call` をホスト関数の中から呼ぶ（fault は外側の run に伝播するので戻り値を捨てるだけでよい）
- `pushObject` は machine ごとに 1 オブジェクト 1 ラッパを返す — **lhat の弱参照キャッシュ**（05 の 8.12、`lhat_machine_weak_cache_get` / `_put` / `_forget`）に載せる。鍵は `love::Object` の番地。同一性は lhat の等値規則が既に持っている（hostdata の `=` は tag + 実体ポインタ）ので、キャッシュの目的は churn を避けることだけ — `world.getBodies()` を毎フレーム呼ぶ類が対象。**ホスト側に自前の map を置いてはいけない**: 収集は漸進的で、マークで白と決まった包みをスイープ前に配ると、L^ が生きた場所へ入れた物がそのまま解放される（実際に physics の約半分が落ちた。経緯は `lh.h`）。弱参照表はコレクタがマーク終端でエントリを外し、マーク中の get は答えた値を灰に戻す（復活）ので、その窓が無い。エントリは `dispose` でも外す — 鍵が番地なので、解放された `love::Object` の番地を次の確保が貰いうる
- ホスト型は親を宣言できる（`lhat_register_hostdata_subtype`、8.8改）。`lh::Context::objectType` の 5 引数版がそれで、TYPES 相で親付きに登録し MEMBERS 相は何もしない — `dispose` / `type` / `typeOf` は親から継承される。`love.graphics.Drawable` の 6 派生（Texture / Mesh / SpriteBatch / ParticleSystem / TextBatch / Video）がこの形で、`draw` のシグネチャは親 1 語。親が先に登録されている必要があるので、レジストラの中で順序を作る（`lhopen_love_graphics` が Drawable を登録してから `lhGraphicsMesh` 等を呼ぶ）
- 親の宣言は**ポインタについての約束**。lhatove では自明に成り立つ — hostdata が持つのは常に `love::Object *`（`pushObject` がそれを取る）で、ポインタは 1 種類しか無い
- physics も同じ形。Shape の下に CircleShape / PolygonShape / EdgeShape / ChainShape、Joint の下に 11 種。`pushShape` / `pushJoint` が `getType()` で実際の `love::Type` を選ぶので、L^ に届く値は種別そのもの。種別依存メンバはその型にだけ登録する — box2d は「クラスを共有せずメンバを共有する」（stiffness は distance/mouse/weld/wheel）ので、C++ の木が 1 度で言えないものは種別ごとに登録する（`jointMember` ヘルパ）

### 旧 Lua コード

`wrap_*.cpp` / `runtime.cpp` / `Reference.cpp` / `LuaThread` / `boot.lua` / `callbacks.lua` / `nogame.lua` と luasocket / enet / lua53 / luahttps は M6 で削除済み（`git log -- src/modules/*/wrap_*.cpp` で読める）。対訳元が要る時は upstream の love2d/love 12.0 か、このリポジトリの履歴を見る。

`testing/*.lua`（upstream のテストスイート）は L^ への移植元として残置。

### コメント・コミット

新しく書くソースコードのコメント（`.cpp` / `.h` / スクリプト / CMake）は英語。日本語で書かない。
既存の日本語・英語コメントはそのまま残す。

コミットメッセージは conventional commits（`feat:` / `fix:` / `refactor:` …）。

## その他のリソース

### lhat

L^ 言語本体。埋め込み API は `include/lhat.h`。最重要参照:

- `include/lhat/program.h` / `vm.h` — 登録・実行契約
- `tests/install_smoke/host.c` — 最小ホスト
- `godot/src/` — Godot GDExtension ポート（最良の参照実装）
- `stdlib/io.c` — C モジュール登録の雛形

@../lhat/

### megasource

love2d 公式 Windows 依存ビルド。`scripts/build.ps1` が `../megasource` に clone する。

> `git clone https://github.com/love2d/megasource.git`

## 動作確認（M6 時点）

lhat 側で直すべき事項は @docs/porting/lhat-issues.md に記録する。

```powershell
.\build\love\Release\lovec.exe                      # 引数なし: nogame 画面（物理で振れる n o g a m e。ゲームを窓へドロップで起動、Esc で終了）
.\build\love\Release\lovec.exe --dump-host-api      # lhat-host.json（LSP 用ホスト API）を書き出す
.\build\love\Release\lovec.exe --dap=41300 testing\lh\autoquit   # DAP: ポート 41300 で待つ。VS Code の絶対パスでブレークポイントが結ばれる（.love の中なら単位名 "main.lh"）
.\build\love\Release\lovec.exe testing\lh\realgame   # conf.lton・require^・画像・フォント・セーブ dir、exit=4（.love / fused exe でも同じ）
.\build\love\Release\lovec.exe testing\lh\m3         # audio/sound/data/math/system/touch/sensor/joystick/ImageData ピクセル、exit=6
.\build\love\Release\lovec.exe testing\lh\physics    # box2d: 接触コールバック4種・filter・query・rayCast・joint・userData、exit=7（約 5 秒）
.\build\love\Release\lovec.exe testing\lh\thread     # std.thread/std.channel: 閉包ワーカー3本・number/string/table/closure の carry・atomic・awaitable で待つ・threaderror、exit=8
.\build\love\Release\lovec.exe testing\lh\shader     # Canvas 読み戻し・Shader uniform・Quad/Mesh/SpriteBatch/ParticleSystem/TextBatch・状態系・Video、exit=9
.\build\love\Release\lovec.exe testing\lh\restart    # love.event.restart + restartValue、2 周目で exit=10
.\build\love\Release\lovec.exe testing\lh\suite      # L^ 版テストスイート（バインド済み全モジュール、451 チェック）、pass=0 / fail=1
.\build\love\Release\lovec.exe testing\lh\hello      # 矩形が動く。Esc で終了
.\build\love\Release\lovec.exe testing\lh\autoquit   # 90 フレームで自動終了、exit=3
.\build\love\Release\lovec.exe testing\lh\customrun  # run オーバーライド、exit=5
.\build\love\Release\lovec.exe testing\lh\panic      # update 内 panic^ → traceback
.\build\love\Release\lovec.exe testing\lh\raise      # 不正な draw mode → ホスト発 panic
.\build\love\Release\lovec.exe testing\lh\badcallback # update の型違い → 起動前に診断
$env:LHATOVE_GC_STATS=120; .\build\love\Release\lovec.exe testing\lh\customrun # 120 フレーム毎に GC 統計（collected / live）
# VM のみビルド（front end 無し。ゲームは先にコンパイルする）
.\build\love\Release\lovec.exe --compile-game out\realgame testing\lh\realgame
.\build-vmonly\love\Release\lovec.exe out\realgame              # exit=4。テキストのまま食わせると断られる
.\build-vmonly\love\Release\lovec.exe --dap=41300 out\realgame  # 行番号は残るのでブレークポイントは効く
```
