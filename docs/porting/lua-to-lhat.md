# Lua → L^ 移植 設計判断記録

lhatove の Lua/LuaJIT を L^ (lhat) へ置き換えるにあたっての確定事項と対応表。
進捗は [status.md](status.md)、ゲーム作者向けの書き方は [main-lh.md](main-lh.md)。
前提 lhat: HEAD `9a5d49a` 以降（列挙は AGENT.md の「登録」節）。

## 確定した方針

1. **lhat 取込**: 兄弟ディレクトリ参照。CMake `LHATOVE_LHAT_DIR`（デフォルト `../lhat`）+ `add_subdirectory`。`LHAT_BUILD_TESTS/CLI/LSP=OFF`、`LHAT_BUILD_STDLIB=ON`、`LHAT_WITH_COMMENTS/RESOLUTIONS=OFF`
2. **Lua 撤去**: クリーンカット。wrap 層はビルドから外し参照用残置 → 移植完了後に削除
3. **バインディング様式**: 型ごと hostdata。多態はシグネチャ union、コンストラクタ多重定義は再登録=オーバーロード
4. **スクリプト形**: main.lh は `module^` + `public^let^` コールバック
5. **メインループ**: L^ 側 run コルーチン。埋め込み Boot.lh が既定 `run`（yieldable `p^`、毎フレーム `yield^`）を提供、ゲームは `public^let^ run` でオーバーライド可。C++ は毎フレーム `lhat_machine_resume`
6. **lhatstdlib**: 選別登録 — `error` / `debug` / `regex` / `load` / `math`（スカラー、度数法）/ `lton`（conf.lton とゲームのデータファイル）。`std.io`・`std.math.vector3` は非登録（love.filesystem が担当 / LÖVE API に用途なし）。`std.thread` / `std.async` は M5 で判断

## 対応表

| Lua 時代 | L^ 移植後 |
| --- | --- |
| `luax_*`（src/common/runtime.h、約60関数） | `love::lh` 名前空間（src/lh/lh.h） |
| `Proxy` userdata + 弱値テーブル同一性 + `__gc` | 型ごと hostdata + `dispose` メンバ（`tag->release` → `Object::release()`）。同一性は lhat の等値規則が持つ（`=` は tag+実体ポインタ、テーブルキーも畳まれる）ので、Lua のようにキャッシュで保証する必要は無い。churn 避けのキャッシュは lhat の**弱参照表**に載せる（05 の 8.12。ホスト側の自前 map は漸進 GC の下で不健全 — M6 後の項） |
| `Reference`（luaL_ref） | `lh::Parked`（`L^.modules.love.registry` テーブル + 整数フリーリスト、RAII） |
| `love::Variant`（スレッド間輸送） | Channel は `lhat_carry/uncarry`（テーブル・閉包可）。love オブジェクトは C ポインタ再鋳造で拡張。Variant は SDL イベント/restart 用に縮退 or 廃止（M5 で判断） |
| `luaopen_love_<mod>` + `luaL_Reg` | `lhopen_love_<mod>(lh::Context&)` 2相登録（TYPES → MEMBERS） |
| boot.lua の coroutine 駆動 `love.run` | Boot.lh の `run`（yieldable p^）を `lhat_machine_call` でコルーチン化、C++ が毎フレーム `lhat_machine_resume`。終了は `return^`（nil/number = 終了コード、"restart" = 再起動） |
| `love.handlers` + `love.run` 内のイベント分岐 | C++ が handlers テーブルを構築（ゲーム公開メンバ or Boot.lh の no-op 既定）、`love.event.dispatch(handlers)` ホスト関数がネスト `lhat_machine_call` で型付き配送 |
| conf.lua | conf.lton（LTON = テーブルリテラルの中身。love.conf スキーマを 1:1 写像）。データなので check も compile もされず、`lhatstdlib_lton_load` が読む。本文は `f^` として読まれ `p^` を呼べず、`love.*` はスコープにも入らない |
| `lua_newstate` per love.thread | 1 Program 共有 + OS スレッド毎 machine（`std.thread` 方式）。thread ユニットの check/compile はメインスレッド |
| pcall / error / errorhandler | エラーは値。宣言はモジュールごと（`love.audio.Error` 等。04 の 2.4）。C++ 例外は `lh::catchexcept` でエラー値化。fault/panic は `lhat_machine_traceback` → ネイティブエラー画面 |
| package.loaders + PhysFS | `LhatProgramLoader` 実装（src/lh/PhysfsLoader.cpp）。PhysFS は 1 名前空間なので 1 program = 1 loader 制約と適合 |
| `love.filesystem.load` / loadstring | `lhat_program_load_text` + `lhat_machine_adopt_script`。スクリプトからは `std.load.file/text` も可 |
| 多値戻り (`getPosition() -> x, y`) | 型付きタプル `-> (number^, number^)`。ホスト側は machine が渡す room に `answers[0..n]` と書き `*answerCount` を立てる（8.7、lhat `16caa92`） |
| `string.*` / `table.*` | 組込メンバ（`s.find/replace/split/…`、`t.sort^/push^/join^/…`）。`string.format` → `$""` 補間。Lua パターン → `std.regex`（実正規表現サブセット） |
| enum 文字列（"fill"/"line"） | 当面 `string^` のまま既存 StringMap で実行時検証 |
| LuaJIT FFI 高速化 shim（wrap_*.lua） | 廃止。ImageData = hostdata + ピクセル host value 型（フィールド直読み書き）+ C 側 `mapPixel` 一括メンバ |
| luasocket / enet / luahttps / lua53 backport | 恒久廃止（将来 std.* 代替を検討） |
| nogame.lua (+ auto.lua hex ヘッダ) | 埋め込み nogame.lh（loader の hold パターンで供給） |

## 重要な制約（lhat 側の性質）

- **登録**（型・メンバ・関数・エラー種・アノテーション・global）は `lhat_program_check` 前に完結必須。検査器がシグネチャの意味を知る必要があるため、check 後の登録は false を返す → 全モジュール API を無条件登録し、conf はインスタンス生成のみ制御。**ユニット**の追加は別物で後からできる（`check` → `compile` は実行中 machine の下でも反復可。`std.load` / `love.filesystem.load` はこれ）
- 登録の identity（hostdata タグ・host value タグ・エラー種）は **program ではなくプロセスのもの**。「登録呼び出し＝宣言」として lhat が intern するので、program をいくつ作っても同じタグが返り、食い違う宣言（別サイズの host value 型、別の variant 並び）は拒否される。restart で program を作り直しても型 identity は変わらない。返すのは `lhat_registry_dispose()`（LhatProgram が 1 つも無い時のみ）
- GC ルートは `L^` と実行フレームのみ。C 保持値は非ルート → `lhat_machine_register` で係留。到達済みテーブルへは `lhat_machine_table_set`（write barrier）
- 「全引数変換 → 呼ぶ → 答え変換」規律（変換と実行を交錯させない）
- Boot.lh は「あれば呼ぶ」を静的に書けない（`t.foo` は型に無ければ静的エラー、`t[k]` は合併|nil^ で絞れない）→ optional コールバックは C++ 境界（handlers 構築、欠けた分は no-op ホスト関数）で解決。**`?.` `?[` `?(`（02 の 11.7改）では解けない** — あれが外すのは対象の側の `nil^` であって、型に無いメンバには届かない（`t?.missing` は今も `no such member`）。handlers の型を `update : p^number^;|nil^` の形にして `h.update?(dt)` と書く道はあるが、「update が無いかもしれない」がゲーム作者に嬉しい状況は無く、no-op を挟む今の形と実質同じ。与え忘れは検出しない（draw だけで回すのも正当な使い方）が、**与えたものの型違いは起動前に落ちる**（`lhat_unit_export_conforms`、`testing/lh/badcallback`）
- 弱参照は**ホストの側にだけある**（05 の 8.12 の弱参照キャッシュ。L^ の綴りには無い）。ホストが持つ参照は GC ルートでもないが、**それは自前キャッシュを組める理由にならない** — 収集が漸進的なので、マークで死と決まった包みをスイープ前に配ると解放済みの物が生きた場所に入る。ラッパのキャッシュは弱参照表に載せる。バイト配列型無し
- エラー値は traceback を自己記録しない（fault/panic のみ）。末尾呼び出しは痕跡なし、fault 時 `finally^` 不実行
- `std.load` は同時 1 machine（スレッドからの load 禁止）。carry 産閉包は proto を借用 → program 破棄をまたぐ restart ペイロードに閉包不可
- 純インタプリタ（JIT 無し）
- machine 破棄は program 破棄・モジュール shutdown より先（GPU リソースのデストラクタは Graphics 生存中に実行必須）
- dispose コールバック内での lhat API 再入は禁止

## 未解決事項（M0/M1 で検証）

- **U1 解決**: シグネチャに宣言したエラー種をホストが `lhat_machine_make_error` で返し、L^ 側 `catch^` で受けられる。**エラー宣言はモジュールごと**（04 の 2.4「宣言が種別を決める」）— 失敗しうる 5 モジュールが `love.audio.Error{CouldNotLoad}` / `love.sound.Error{CouldNotLoad}` / `love.image.Error{CouldNotLoad}` / `love.graphics.Error{CouldNotLoad, ShaderFailed}` / `love.filesystem.Error{IO, Rejected}` を自分の registrar で宣言する。variant は**何が起きたか**で命名する（どの層が気づいたかではない）。1 variant のモジュールはシグネチャに宣言名だけ書けば済む（`-> love.audio.Source|love.audio.Error`）。受け側も宣言名で絞れる（`fits^ love.filesystem.Error`）し、葉でも絞れる（`fits^ love.filesystem.Error.IO`）— `testing/lh/suite/tests/filesystem.lh` が両方を確認。未宣言のエラー値を返す経路は試していないので、各バインディングは返しうる種をシグネチャに必ず書く
- **エラーの粒度は「読めなかった」まで**。`CouldNotLoad` を `NotFound` / `Unsupported` / `Corrupt` に割らないのは、割る材料が LÖVE 側に無いため — `love::Exception` は 1 種類で、区別は `what()` の文字列にしかない。filesystem/image/sound/audio だけで `throw love::Exception` が 155 箇所、「開けない」の綴りだけでも `Could not open file at path %s` / `Could not open file %s. Does not exist.` / `Could not open file %s: %s`（SDL の文言が埋まる）と複数あり、`PHYSFS_getErrorByCode` の戻り値が入る箇所もある。文字列で振り分けると upstream が 1 行変えただけで黙って未分類に落ち、テストは通ったままになる。種別 1 つは嘘をつかない（「読めなかった、理由は message」は常に真）し、Lua 版は `return nil, "文字列"` で種別が型として存在すらしなかったので後退でもない。判別が要る場面は `it^.message` を読む
- **U2 解決**: `testing/lh/physics` で 120 フレーム 15,018 回収（live 2,316）。ラッパを machine ごとにキャッシュして 7,162（live 2,248）へ半減。同一性も `is^` まで一致するようになった（`=` は元から真）
- **U3**: `require^` のパス正規化と PhysFS 区切り・`..` の整合
- **U4**（M0 で確認）: `f^ -> t^{ update : p^number^ -> nil^;, draw : p^ -> nil^; };` は登録・検査・呼び出しとも動く（`lovec --probe`）。lhat 側の構造型メンバ名 use-after-free は `10e810e` で修正済み

## M0 で確定した実装事項

- lhatstdlib ヘッダは lhat 側で `extern "C"` ガード済み（そのまま include）
- 当時は `love.Error{Misuse,IO,NotSupported}` を 1 つだけ TYPES 相の最初に登録していた。M6 後にモジュールごとの宣言へ分割（04 の 2.4）
- `lovec` は `LOVE_LH_CONSOLE_EXE` 定義付きでビルドし、`love_lh_boot(argc, argv, console)` にフラグを渡す。console=true では診断・エラーは stderr のみ、false（love.exe）ではメッセージボックスも出す
- 末尾呼び出しは traceback に残らない（`outer = p^ { inner() }` は表示されない）— 仕様

## M1 で確定した実装事項

- 起動列: 登録 → check(Boot.lh, main.lh) → compile → main.lh 実行（公開テーブル）→ モジュール生成（timer/event/keyboard/mouse/font/window/graphics、800x600）→ handlers 構築 → run 選択 → `lhat_machine_call(run)` でコルーチン → 毎フレーム `lhat_machine_resume`
- handlers: `src/lh/Boot.cpp` の `callbacks[]` からシグネチャ文字列を生成し `love.boot.handlers()` に登録。欠けたコールバックは variadic no-op のホスト関数（quit は `false^` を返す）
- `love.event.dispatch(h) -> number^|nil^`: C++ がイベントを poll し `handlers.<name>(typed args)` をネスト `lhat_machine_call`。quit は `handlers.quit()` が `true^` なら拒否、さもなくば終了コードを返す
- `lh::raise`: プログラマエラー用。`lhat_machine_panic_text` で run を `panic^` として終える（traceback は呼び出し箇所から）。戻り値の nil は捨てられる
- `lh::guard`: `love::Exception` → `raise`。`lh::catchexcept`: `love::Exception` → 宣言済みエラー値（fallible API 用）
- 可変長の末尾引数（描画の transform 9 数値など）は `...`、少数の省略可能引数は同名再登録のオーバーロード（`clear` 0/3/4 引数）
- enum 文字列（draw mode、align）は `string^` + `getConstant` で実行時検証、不正なら `raise`
- `src/lh/lh.h` に TypeRegistry（`love::Type* ↔ LhatHostDataTag*`、isa によるダウンキャスト）、`pushObject`/`checkObject`、`pushVariant`、`park` を実装済み（オブジェクト型の初使用は M2 の image/font）
- 起動時に `lhat_unit_export_conforms` でゲームの公開コールバックの型を `callbacks[]` の署名と照合。違えば診断を出して起動しない（`testing/lh/badcallback`）
- `std.math` を登録（度数法。`love.graphics.rotate` 等ラジアン API へは `std.math.rad(a)`）。`std.math.vector3` は非登録
- 未対応: conf（当時は conf.lh、M6 後に conf.lton へ）、Window の settings テーブル、restart、nogame、`love.graphics` のオブジェクト類

## M2 で確定した実装事項

- `Loader`（src/lh/PhysfsLoader.cpp）は held ユニット → `love.filesystem` の順に読む。boot が最初に `physfs::Filesystem` を作り `init(argv[0])` → fused 判定（`setSource(exepath)`）→ ゲームの `setSource` → identity（boot.lua と同じ導出）→ `setIdentity(identity, true)` → check
- **U3 解決**: `require^ "lib/vec.lh"` はそのまま PhysFS に届く（ディレクトリ・zip・fused で確認）
- conf は check 後・compile 後に読み、モジュール生成と window 設定を制御。`modules.*` の既定は全 `true^`。無ければ既定値（当時は conf.lh を `lhat_run`。M6 後に conf.lton + `lhatstdlib_lton_load` へ）
- 起動列: filesystem → 登録 → check(Boot.lh, main.lh) → callback 型検査 → compile → conf 読み → モジュール生成 + window → main 実行 → handlers → run
- nogame: 引数なし or 無効パスは埋め込み `nogame_lh`（簡素版。アニメ版は M6）
- エラー画面: `src/lh/ErrorScreen.cpp`。実行時 fault/panic は `reportRuntime` → 青画面（Escape/閉じるで終了）。check 失敗は `report`（コンソール + love.exe はメッセージボックス）— window 生成前のため
- オブジェクト: File / FileData / ImageData / Drawable / Texture / Font を `ctx.objectType` で登録（dispose/type/typeOf 自動）。`draw` は当時 `love.graphics.Texture` 引数（M5 で 6 型の union、M6 後に `love.graphics.Drawable` の親子宣言へ）
- 失敗しうる API（read/write/newImage/newFont(path)/newFile/newFileData(path)）は自分のモジュールのエラー種を宣言し `catchexcept`（当時は共通の `love.Error.IO`）。それ以外のプログラマエラーは `raise` = panic
- **可変長オーバーロードの制約**: `print(text, ...)` と `print(text, Font, ...)` は重複とみなされ登録拒否 → 1 本にして可変長尾の先頭が Font かを実行時判別（`fontInTail`）。同様の「任意位置のオブジェクト引数」は同方式
- `Runtime::failedRegistrar()` でどのレジストラが拒否されたか分かる
- 未対応: window の `getMode` settings 返却、`love.filesystem.mount/lines/enumerate`、restart、`--game` 等 arg.lua のオプション群、URI 引数

## M3 で確定した実装事項

- モジュール生成順は boot.lua どおり: filesystem → timer → event → keyboard → joystick → mouse → touch → sound → system → sensor → audio(OpenAL、失敗時 null) → image → data → font → window → graphics → math。conf の `modules.*` で個別に切れる
- コールバック追加: joystick/gamepad 系（`love.joystick.Joystick` hostdata が引数）、touch 系（id は整数）、`sensorupdated`。`Variant::LUSERDATA` は整数へ変換（touch id）
- lhat `3a4376c` で解消: 自型メンバは `Self^` 綴り（`Transform` の全メンバ）、可変長アームは位置で区別（`print` = `string^` / `string^, number^, ...` / `string^, Font, ...` の3アーム）。当初の回避（`apply` の `any^`、固定アーム化）は撤去 or 任意
- ImageData のピクセル: `getPixel -> (r,g,b,a)` タプル、`setPixel(x,y,r,g,b[,a])`、`mapPixel(f^x,y,r,g,b,a -> (r,g,b,a))`（ピクセル毎にネスト呼び出し。host value 色型は見送り — タプルで足りる）
- `love.data` は文字列 in/out（ByteData 等のコンテナは必要になった時点で）
- デバッグ補助: `LHATOVE_TRACE=1`（起動列をトレース）、`LHATOVE_SKIP_REGISTRATIONS=a,b`（登録を外して二分探索）、`Runtime::failedRegistrar()` は拒否された登録名と署名を含む

## M4 で確定した実装事項

- box2d コアの脱 Lua: `World`/`Body`/`Shape`/`Joint`/`Contact` から `lua_State` と `Reference` を除去。多値返しは out 参照 or `std::vector<float>`、列挙は `std::vector<T*>`（`getContacts` は retain 済みを返す）、userData は `StrongRef<love::Object>`。World のコールバックは言語非依存の listener 型（`ContactListener::onContact(a, b, contact, impulses)` / `ContactFilterListener::shouldCollide` / `ShapeVisitor::onShape` / `RayCastVisitor::onHit`）、`rayCastAny/Closest` は `RayHit` 構造体
- `lh::ParkingLot` / `lh::Parked`（lh.h）: `Reference` の後継。`L^.modules.love.registry` テーブルの整数スロットに値を係留し、`Parked` は `love::Object` として `StrongRef` で保持できる。解放は `releaseLater` → 次の `park()`/`sweep()`（safe point）で nil 書込（dispose は collector 内で走りうるため）。`Runtime` が lot を所有し、`compile()` 後に `attach`、破棄時に `detach`
- L^ 側の型は 5 つ: `love.physics.World/Body/Shape/Joint/Contact`。Shape の種別（circle/polygon/edge/chain）と Joint の種別（distance/revolute/…）は **1 つの hostdata 型に全メンバを平坦化**し、種別外のメンバは `lh::raise`（`getType()` で判別）。理由: L^ の hostdata 型に部分型が無く、`Body.getShapes -> t^{Shape[]}` のような戻りを合併で書く負担を避ける
- コールバック: `World.setCallbacks(begin[, end[, presolve[, postsolve]]])`（引数個数のアーム。省いたものはクリア、`setCallbacks()` で全解除）、`setContactFilter(p^Shape, Shape -> bool^;)`、`queryShapesInArea(..., p^Shape -> bool^;)`、`rayCast(..., p^Shape, x, y, nx, ny, fraction -> number^;)`。callback の型は `f^` ではなく `p^`（外の変数を書く用途のため）。postsolve は `(a, b, contact, n1, t1, n2, t2)` 固定 7 引数（足りない点は 0）
- `lhat_machine_call` は `World.update` の中（b2 の step 内）から呼ぶ。fault は外側の run に伝播するので host 側は戻り値を捨てるだけ
- 可変長の座標は `...` で受け、結果の列は `t^{number^[]}`（`getPoints`、`getWorldPoints`、`getPositions`、`getCategory`）。`love.graphics.polygon` は `p^string^, ...;` に緩和（`body.getWorldPoints(shape.getPoints()...)...` の展開は固定引数を満たさないため）
- Shape の `rayCast` は `(hit:bool^, nx, ny, fraction)` タプル（タプル|nil^ の合併を避けた）。`World.rayCastAny/Closest` は `t^{ shape, x, y, nx, ny, fraction }|nil^`
- 非対応（12.0 で deprecated）: body 無しの `newCircleShape(x, y, r)` 系、`newFixture`、`Fixture:getShape`、`ChainShape:getChildEdge`、`MouseJoint` の setFrequency/setDampingRatio（コアに実装が無い）
- デバッグ補助を追加: `LHATOVE_WATCHDOG=<秒>`（フレームが止まると主スレッドのスタックを base+offset で stderr へ書き、`scripts/symbolize.c` で .pdb から名前解決。RelWithDebInfo 推奨）、`LHATOVE_GC_STATS=<n>`（n フレーム毎）、`LHATOVE_SKIP_REGISTRATIONS=love.physics.*`（前方一致）
- lhat `fea90e4` で解消: install が hostdata 型をメンバ付きテーブルとして再帰展開していた型爆発（physics の相互参照 5 型で live 265 万 → 2,477、nested call 370ms → 即時）。副産物として hostdata 引数のオーバーロード解決がタグ比較で効く。補間スロットのタプルも静的エラーになった

## M5 で確定した実装事項

- love.thread: `LhThread`（`LuaThread` の後継、`Threadable`）が OS スレッド上に専用 `LhatMachine` を作り（`lh::Runtime::spawnMachine` = `lhat_machine_new` + `lhat_program_install` + 専用 `ParkingLot`）、Program を共有して走る（std.thread と同形）。スレッド本体はファイル（`lhat_program_check` で unit 化 → 診断は newThread 時に panic）か文字列コード（`lhat_program_load_text`、Thread が proto を所有）。引数は script の `...` で受ける
- Program への書込（check / compile / load / install）は `lh::programMutex()` で直列化。実行中の machine は proto を読むだけ
- Channel の値は `lh::variantOf`: nil/bool/number/string はそのまま、hostdata は `Variant::LOVEOBJECT`、table/closure は `lhat_carry` の複製を `lh::Carried`（love::Object）で包んで `Variant` に載せ、取り出す側の machine で `lhat_uncarry`。LOVE オブジェクトを含む table は運べない（carry が拒否 → panic）
- `ParkingLot::lotOf(machine)`: 値を係留する binding は machine から lot を引く（physics のコールバック等）。`physicsBinding.lot` は廃止
- `threaderror` コールバック追加（`p^love.thread.Thread, string^`）。thread の fault は `lh::describeRun` でメッセージ化
- `performAtomic(fn, ...)` は extra 引数の個数でアーム分け（0〜3、`any^`）。可変長型の手続き型に固定引数の閉包は適合しないため
- 同位置に異なる hostdata 型を置くアームは登録で拒否される（File vs FileData）→ `p^File|FileData` の合併 1 アーム（[lhat-issues.md](lhat-issues.md) 提案）
- `std.thread` / `std.async` は登録しない
- graphics: Canvas は独立型にせず `newCanvas` が render target 付き `Texture` を返す（12.0 と同じ）。`setCanvas(...)` は 1 本の可変長アーム（Texture 列 + 省略可の depth/stencil bool）— `p^Texture, ...` と `p^t^{Texture[]}, ...` は登録で重なると判定されるため。`getScissor` は未設定時 0 の 4 組、`rayCast` 系と同じ「nil^ の代わりに値」方針
- `draw` は `p^D;` / `p^D, number^, ...;` / `p^Texture, Quad, ...;` の 3 アーム（D = Texture|Mesh|SpriteBatch|ParticleSystem|TextBatch|Video の合併）。`add(x, y, ...)` と `add(quad, x, y, ...)` も同様に先頭位置で分ける
- Shader.send は uniform の型に従う: float（数値列 or 要素ごとの number 表）、int/uint、bool、matrix（列優先の平坦表 / 行の表 / mat4 は Transform）、sampler（Texture）
- Mesh は標準頂点形式（x, y, u, v, r, g, b, a）のみ。頂点は 8 数 or 表で指定
- Video: `love.graphics.newVideo(path[, {audio = false^, dpiscale}])`。wrap_Graphics.lua と同じく同ファイルから audio Source を作って同期、無ければ DeltaSync。VideoStream 型は公開しない
- 未対応: Buffer / compute shader / drawInstanced / テクスチャ配列・立方体・3D / カスタム頂点形式 / captureScreenshot / Shader の `Buffer` uniform / ParticleSystem の setQuads 以外の細目（clone はあり）

## M6 で確定した実装事項

- restart: `love.event.restart([payload])` が `quit` メッセージを `"restart"` 付きで積み、`love.event.dispatch` が `"restart"` 文字列を返す → run が返し、`boot` は `LOVE_LH_RESTART`（Boot.h の負値）を返す。`love.cpp` の `do { love_lh_boot(...) } while (code == LOVE_LH_RESTART)` が再 boot。payload は `lh::variantOf` で Variant 化してプロセス側 static に置き、次の boot の `love.event.restartValue()` が `pushVariant` で戻す（table/closure は carry の複製、LOVE オブジェクトは拒否）
- `love.event.restartValue` は `love.event` に置く。`love` テーブル直下のホスト関数は `import^ love` を要し、それが `import^ love.graphics` 等のサブモジュール import と衝突する（「this table belongs to the machine」）ため
- ドロップからの再起動: `love.boot.restartInto(path)`（nogame.lua の `_noGameRestartInfo` 相当）が `quit "restart"` にゲームパスを添え、次の boot が `args.game` として使う。`filedropped` / `directorydropped` コールバックを追加
- Boot.lh の run は `dispatch` の答えが nil^ でなければ返す（数値 = 終了コード、`"restart"` = 再起動）
- nogame.lh: 埋め込み画像を持たない代わりに love.physics で組む — 静的アンカーから伸びる6連リンクの各リンクに文字（n o g a m e）、末端に浮力（`setGravityScale(-1.2)`）の風船、背景に流れる雲。クリックで風船を弾き、Esc で終了、ゲームをドロップで起動
- `testing/` の移植: `testing/lh/suite/` に upstream の TestSuite/TestMethod を L^ で置き直す。`lib/test.lh` が assertion（`isTrue` / `number` / `near` / `range` / `text` / `notNil`）を持ち、`tests/<module>.lh` が `public^let^ run` を公開、`main.lh` が全部呼んで pass なら exit 0・fail なら exit 1。Lua の `assertEquals(any, any)` は L^ に無い（`any^` は渡すか絞るかだけ）ので、比較は型ごとに分ける
- `require^` は **要求元のユニットからの相対パス**。`tests/x.lh` から `lib/test.lh` を読むには `require^ "../lib/test.lh"`（`..` は PhysFS 越しでも通る — U3 の残り）
- `testing/lh/suite` は M6 で全モジュールに広げた（451 チェック）。移植中に見つけて直したもの: `love.graphics.validateShader` が壊れたコードで投げる例外を捕まえていなかった（プロセスが fastfail）→ `(false, message)` を答える、`getScissor` が scissor を切った後も最後の矩形を答えていた → 0 を答える、`Shape.setCategory` / `setMask` に引数なしアームが無かった（LÖVE の「全部と衝突」）
- L^ の制約が 1 つ見つかった: 親モジュールと子モジュールを同じスコープに import できない（`love` と `love.graphics`）。もう 1 件は綴りの誤りだった — **ブロックは `do^{ ... }`**（02 の 8.7）。裸の `{ ... }` はテーブル literal なので文にならない
- lhat `995d0e8` で解消: 親と子の同時 import（`import^ love` と `import^ love.graphics`）、登録型どうしの重なり判定（`newThread` は `p^File;` / `p^FileData;` の 2 アームへ戻した）。`testing/lh/suite` は 451 チェック
- **ホットリロードは入れない**（`lhat_reload` を見て再検討する前に読むこと）。LÖVE 本体にもホットリロードは無く、あるのは `love.event.restart` = 全部捨てて `boot()` からやり直す形だけ。Lua 版のゲーム作者がやっていたのは `love.filesystem.load` で書き直したファイルを読み直して手で差し替える自前の仕組みで、lhat `9a5d49a` の `lhat_reload(program, path, machines, n)` を使っても届く範囲は**同じ**:
  - **状態は残らない**。入れ替わるのはユニットで、`var^` の現在値は新しい body の初期化で上書きされる。「敵の位置を保ったまま挙動だけ差し替える」は結局ゲーム側が状態を外へ逃がして書くことになる
  - **`main.lh` は差し替えても効かない**。handlers は起動時に C++ が組んで `love.boot.handlers` に park したテーブルで、ユニットを入れ替えても指す先は古い閉包のまま。効かせるには handlers を組み直す経路が要る（held とは別の話 — `lhat_reload` は旧 body を掴まれていても成功し、解放を次回まで延ばすだけ）
  - 差が出るのは「壊れたコードを読み込んでも旧世界が走り続ける」点だけで、それは `lhat_reload` の手柄というより検査器の手柄
  やるとしたら決めることは 3 つ: 引き金（`love.event.reload(path)` か、エンジンがファイル監視を持つか）、handlers の組み直し、`machines` の集め方（program を install した全 machine。love.thread のワーカー分は `ParkingLot` の machine 索引から取れる）
- **front end を落とした配布ができる**（`scripts/build.ps1 -VmOnly`）。lhat の `LHAT_WITH_FRONTEND=OFF` に乗る形で、材料はフル版が作りエンジンが読む 2 段構え:
  - **ユニット** — `lhat_unit_write_binary`。読み戻しはローダーが差し替えるだけで `require^` もパスも同じ（先頭バイトで判別、`lhat_program_is_binary_unit`）
  - **登録** — 署名はテキストなので読むのに front end が要る。フル版が `lhat_program_write_signatures` で表を書き、VM 版は `src/lh/Signatures.h` として**埋め込んだ表を登録の前に読む**。噛み合わなければ起動時に断る
  - **conf.lton** — `lhatstdlib_lton_write`。`lhatstdlib_lton_load` はどちらでも読む
  - **埋め込みユニット**も同じ扱いが要る（`BootBinary.h` / `NogameBinary.h`）。VM 版は自分の `Boot.lh` も `nogame.lh` もテキストでは読めない
  - コールバック型検査（`lhat_unit_export_conforms`）はバイナリユニットで常に false を答える（検査器の型が無い）ので、**そのユニットが binary なら検査ごと飛ばす**。確かめ直すものは無い — コンパイルしたビルドが既に検査済みで、バイナリは同じ fingerprint・同じ登録の program にしか戻らない。ローダーが読んだ先頭バイトを覚えて `Loader::isBinary(path)` が答える
  - **利得は起動であってサイズではない**。`lhat.lib` は半減するが `love.dll` は 196KB しか縮まない（表が 114KB）。`.love` は逆に増える（zip の deflate がバイナリに効かない）
  - **代償**: 実行時に構文解析器が要る口が全部落ちる — `newThread(コード文字列)`、`love.filesystem.load` / `std.load` のテキスト。ファイル版スレッドは動く（`--compile-game` がゲーム内の `.lh` を全部 check して運ぶため、実行が届かないユニットも配布物に入る）
- **love.thread を廃止し、並行処理を言語へ渡した**（[thread-design.md](thread-design.md) が比較と、lhat に頼んだ 4 点）。lhat が 4 コミットでそれを埋めた:
  - **hostdata の共有契約**（`lhat_register_hostdata_shared`）— carry が hostdata を全拒否していたのを、型がポインタの寿命（retain / let_go）を宣言すれば運ぶ形に。lhatove は `Context::objectType` の 2 オーバーロードで**全型に**立てる（`love::Object` の参照カウントは atomic）。Lua 版が任意の `love::Object` を Channel に通せた範囲に戻り、**LOVE オブジェクトを含む table** も運べるようになった
  - **`std.channel`** — carry の上の MPMC キュー。`named` はプロセス全体で共有され、どの機械からも同じ実体
  - **ワーカーの完了が push で届く** — `awaitable()`（`std.async` の待ちを立てる）・`failed()`・`lhatstdlib_thread_on_finish`（ホストのキューへ）・`lhatstdlib_thread_join_all`
  - **`lhat_program_set_lock`** — check / compile / load / install / invalidate / reload を覆うホストのロック。`lh::programMutex()` を渡し、**lhatove 側の手動ロックは全部外した**（対は入れ子にならない）
  結果、engine 側に残るのは `threaderror` イベント（`p^string^`。`p^love.thread.Thread, string^` から変わった）と、`Runtime` のデストラクタが program 破棄前に呼ぶ `join_all` / `forget_named` だけ。`LhThread` / `Channel` / `ThreadModule` / `lh_Thread.cpp` は削除。`src/modules/thread/` に残るのは Mutex / Conditional / Threadable で、audio・video が自前のワーカーに使う
  - **利得**: 本体を同じユニットに書ける（別ファイルもコード文字列も要らない）、整数が実数に落ちない、誤りが値、`await^` に載る、そして **VM のみビルドでスレッドが完全に動く**（閉包は構文解析器を要らない — `testing/lh/thread` を丸ごとコンパイルして VM 版で exit=8）
  - **代償**: `newThread(コード文字列)` は無くなった（VM のみビルドでは元から動かない）。ワーカーの失敗文が痩せた — [lhat-issues.md](lhat-issues.md) に記録
  - **ParkingLot は遅延 attach に**。std.thread は lhatove が見ていないところで機械を作るので、`ParkingLot::lotOf` が無ければその場で作る（呼ばれるのはホスト関数の中＝その機械自身のスレッド）
- **ラッパのキャッシュは lhat の弱参照表に載せた**。最初は `lh::WrapperCache` — ホスト側の
  `std::map` — で組んだが、これは不健全だった。「ホストの map は GC ルートではないから、包みは
  今までどおり回収され `dispose^` がエントリを外す」という理屈は各文が真で結論が偽。lhat の収集は
  **漸進的**（`gc.c`「Lua's incremental collector, borrowed」）で、マークとスイープの間で
  プログラムが走る — マークで白と決まった包みをスイープ前に `pushObject` が配り、L^ が生きた
  場所へ入れると、その物は判定どおり解放される。Lua の弱値表が与えているのは「根にしない」
  ではなく「**コレクタがマークの終わりにエントリを自分で消す**」ことで、ホストの map には
  知らされる口も復活させる口も無い。
  症状は `testing/lh/physics` の**約半分**（Body のメンバが self 無しで届く／instruction given the
  wrong type = 解放済みを読んだ形）。lhat の `a8d91a1` で表面化したが原因は最初からこちら側で、
  外すと 20/20 通過（有りは 13/20）。
  報告して lhat に `lhat_machine_weak_cache_get` / `_put` / `_forget`（05 の 8.12、`78b72ec`）が
  入り、`pushObject` はそれに載せ直した — コレクタがマーク終端でエントリを外し、マーク中の
  get は答えた値を灰に戻す（復活）。`dispose` でも `_forget` する（鍵が番地なので、解放された
  `love::Object` の番地を次の確保が貰いうる）。physics 20/20、churn は 120 フレームで
  collected 1,582 → 1,164 / live 8,939 → 7,643
- **既定の処理系を clang-cl にした**（`scripts/build.ps1`、`-Msvc` で cl.exe）。lhat の VM は Clang で computed goto のディスパッチになり、lhatove 上の VM 負荷はループで約 36% 減、メンバ呼び出し・table で 16〜17% 減、ホスト境界越しはほぼ同じ（PGO 無しの数字）。Ninja Multi-Config + VS 同梱の clang-cl / ninja で、MSBuild の ClangCL ツールセットは要らない。VM のみビルドの生成物は処理系に依らない（指紋に処理系が入らない）
- **係留は `L^.modules.lhatove.*`**。lhat `d7dc455` が登録済みモジュールの表を program 共有・封印にしたのに、`lhat_machine_register` は封印を素通りする。lhatove は `ParkingLot` の表を `love.registry`、Boot の値を `love.boot.*` に置いていたので、共有の黒い表の下に機械自身の表がぶら下がり、コレクタがそれを二度と見ない → 係留したコールバックが回収され physics が落ちた（clang で接触コールバック全消失、MSVC で segfault）。二分探索で `d7dc455` を特定。どの登録も作らない `lhatove` を根にして直し、`lh::park` は置き場の下位名しか受け取らない形にした（登録済みモジュールへ誤って置く道を無くす）
- **係留はホストの根**（`lhat_machine_host_root`、lhat `b10f3fe`）。`L^.modules.lhatove.*` は共有表を避けるための一時の逃げ場で、正しくはあったが、プログラムが名前で届く場所だった。ホストの根は機械が根として持ち L^ からは辿れない — Lua の registry と同じ位置。`lh::park` は 1 つのキー（`boot.handlers` 等）を取る形になった。同コミットで lhat は共有表へのホストの書き込みを断るようになり（`lhat_machine_register` は false）、`import^love` の後の `import^love.event` も通るようになった
- **添字を 0 起点へ**（lhat `5529b14 feat: indices count from 0`）。容器の仕組みは機械的に追従した — 列を組む `lhat_integer((int64_t) i + 1)` の `+ 1` を消し（System / Physics / Filesystem / Touch / Body / World / Joystick / ParticleSystem / Mesh）、列を読む `numbersOf`・頂点列・Shader の行列は 0 から読む。L^ 側は `...[0]` と nogame の `segments[i]` の走り方だけ（末尾は `segments[segments.length^ - 1]`。最初 `segments[-1]` と書いて風船が消えた — 負の序数は組込の `at` / `substr` / `slice^` / `remove^` のもので、表の添字ではただのキー `-1`）（数え上げの `for^ i from^ 1 to^ n` は添字に使っていなければ据え置き）。**LÖVE の API 上の添字も 0 起点にした** — Lua 版の wrap 層が C++ 本体の 0 起点を 1 ずらしていたのを外しただけで、残すと「頂点マップは 0 起点の列で返るのに中身の頂点番号は 1 起点」のような食い違いが言語の中に残る。変えたのは Mesh（頂点・頂点マップ・描画範囲）、SpriteBatch（id・描画範囲）、TextBatch（番号）、Quad の layer、Joystick の軸・ハット・ボタンとそのイベント、Contact の子、Shape の rayCast の子、`displayindex`。**添字でない番号は据え置き**: マウスボタン（SDL 自身が 1 から）、physics のカテゴリ 1〜16（名前）、`love.math.random(n)` の範囲 1〜n、touch / joystick の ID。`getDrawRange` の「全部描く」は `(0, 0)` のまま（範囲が立っていれば個数は 1 以上なので区別できる）。黙って壊れる種類の変更なので、suite に頂点マップの値の往復を 1 本足した（473 チェック）
