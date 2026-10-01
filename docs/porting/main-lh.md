# main.lh の書き方

M2 時点で動く形。実例は `testing/lh/{hello,autoquit,customrun,panic,raise,badcallback,realgame}`。
実装と食い違ったらこのファイルを直す。

## 基本形

main.lh は `module^` を宣言し、コールバックを `public^let^` メンバとして公開する。
エンジンは起動時に公開メンバを集めて handlers テーブルを作り（無いものは no-op）、
既定の `run`（埋め込み Boot.lh）がフレーム毎にそれらを呼ぶ。
Lua 版の `love.update = function(dt) ... end` 代入スタイルは存在しない
（L^ に metatable は無く、`L^` 直下のテーブルは sealed のため）。

```lhat
module^ main

import^ love.graphics
import^ love.event
import^ love.keyboard

var^ x = 0

public^let^ load = p^ {
    # one-time setup
}

public^let^ update = p^dt:number^ {
    x := x + 60 * dt
}

public^let^ draw = p^ {
    love.graphics.rectangle(love.graphics.DrawMode.fill, x, 100, 50, 50)
}

public^let^ keypressed = p^key:love.keyboard.Key, scancode:love.keyboard.Scancode, isrepeat:bool^ {
    if^ key = love.keyboard.Key.escape { love.event.quit() }
}

public^let^ quit = p^ -> bool^ {
    return^ false^   # true^ で終了を拒否
}
```

すべてのコールバックは任意。副作用を持つので `p^`（`f^` は純粋関数）。
M1 時点のコールバック: `load update draw quit keypressed keyreleased textinput mousemoved
mousepressed mousereleased wheelmoved resize focus mousefocus visible`
（一覧は `src/lh/Boot.cpp` の `callbacks[]`）。

## run のオーバーライド

`public^let^ run = p^ { … }` を公開すると既定の run の代わりに使われる。
yieldable な `p^` で、毎フレーム `yield^` する。終了は `return^ <number^>`（終了コード）。
コールバックは `love.boot.handlers()` で得る（既定 run と同じ経路）。
`love.event.dispatch(h)` がイベントを各コールバックへ配り、quit が受理されたら終了コードを返す（それ以外は `nil^`）。

```lhat
public^let^ run = p^ {
    let^ h = love.boot.handlers()
    h.load()
    love.timer.step()
    repeat^ {
        love.event.pump()
        let^ code = love.event.dispatch(h)
        if^ code isa^ number^ { return^ code }
        h.update(love.timer.step())
        love.graphics.origin()
        love.graphics.clear()
        h.draw()
        love.graphics.present()
        yield^
    }
}
```

## conf.lton

**設定はデータ。** LTON（L^ Table Object Notation）はテーブルリテラルの**中身**をそのまま書く綴りで、
`return^ {` と `}` を書かない。love.conf のスキーマを 1:1 写像。
読まれる項目: `identity` `appendidentity` `version` `console`、`window.{title width height fullscreen
fullscreentype vsync msaa stencil depth resizable minwidth minheight borderless centered displayindex
usedpiscale refreshrate x y}`、`modules.{timer event keyboard mouse image font window graphics}`（既定 `true^`）。

```lton
# conf.lton
identity = "mygame",
window = {
    title = "My Game",
    width = 1280,
    height = 720,
    resizable = true^,
},
```

綴りは L^ のもの（コメント・文字列エスケープ・数の形は同じ字句解析器が読む）。末尾の `,` は許される。

**効果のあることは書けない。** 本文は `f^` の本体として読まれ、`f^` は `f^` しか呼べない（02 の 15.1）
ので `p^` 呼び出しは誤り。加えて `love.*` も `print` も**スコープに入らない**ので、
設定ファイルからエンジンには一切触れない。算術・比較・連結・入れ子のテーブルは通る。

同じ綴りはゲームのデータファイルにも使える。

```lhat
let^ stage = try^ std.lton.load("stages/3.lton")
```

`std.lton.load(path)` / `std.lton.parse(text)` はどちらも `f^ -> t^{}|std.lton.LtonError|std.error.OutOfMemory`。
パスは `require^` と同じく PhysFS 経由なので、ディレクトリ・`.love`・fused のどれでも同じように読める。
スクリプトを読む `std.load` と違い、読んだ結果が何かを実行することはない。

## ゲームの置き場

`love path/to/dir`（`main.lh` を含むディレクトリ）、`love game.love`（zip）、`love file.lh`、
または fused 形式（下の「配布する」）。
ゲーム内の別ユニットは `require^ "lib/util.lh"`（要求側からの相対パス、リテラルのみ）。
実行時に決めるパスは `love.filesystem.load(path)`（閉包を返す）か `std.load`。

## 配布する

fused にする。exe の末尾に `.love`（zip）を連結した 1 つのファイルで、Windows は先頭の
PE ヘッダを読み、PhysFS は末尾の central directory を読むので、同じファイルが実行形式と
アーカイブの両方として妥当になる（自己展開書庫と同じ原理）。LÖVE は起動時に自分自身を
zip としてマウントしてみて、開けたら fused と判定する。

```powershell
.\scripts\build.ps1 -Shipping                          # デバッガの入らないビルド
copy /b build-shipping\love\Release\love.exe+game.love mygame.exe
```

土台は `love.exe`（窓版）。`lovec.exe` はコンソールを開くので開発用。

`.love` は `main.lh` を**アーカイブの根**に置いた zip。PowerShell の `Compress-Archive` は
`.zip` 以外の拡張子を拒むので、`.zip` で作ってから改名する。

```powershell
cd path\to\game                                        # main.lh のあるディレクトリ
Compress-Archive -Path * -DestinationPath ..\game.zip -Force
Rename-Item ..\game.zip game.love
```

**同梱するもの**（`build-shipping\love\Release\` から）:

- `mygame.exe` — 上で作ったもの
- `love.dll` — エンジン本体。`love.exe` 自体は 48KB の殻でしかない
- `SDL3.dll`・`OpenAL32.dll`

静的リンクにして 1 ファイルにはできない。OpenAL が LGPL で、差し替え可能性を残すために
動的リンクが要る。DLL 同梱は今日のデスクトップアプリでは普通なので、ここは諦めてよい。

**fused にする理由は「1 ファイルになるから」ではない**（ならない）。実質的な差は 3 つ:

- **セーブ先が独立する。** 通常は `%APPDATA%\LOVE\<identity>\` で、LÖVE で作られた
  全ゲームが同じ棚に並ぶ。fused なら `%APPDATA%\<identity>\`。`identity` を設定し忘れた
  ゲーム同士がセーブを踏み合う事故が消える
- **自分の名前で走る。** 遊ぶ側が起動するのは `love.exe` ではなく `mygame.exe`
- **中身が一体になる。** `.love` が隣にあると、zip なので開けるし差し替えられる

`love.filesystem.isFused()` で自分がどちらかを訊ける。

### ソースを配らない（VM のみビルド）

`-VmOnly` で建てたエンジンは front end を持たない。読めるのはコンパイル済みユニットだけで、
テキストの `.lh` は `this build has no front end; only a binary unit runs` と断られる。
配布物にゲームの綴りが入らない、という副産物がある。

```powershell
.\scripts\build.ps1 -VmOnly -Shipping                              # 1 度だけ
.\build\love\Release\lovec.exe --compile-game out\mygame game\     # フル版で materials を作る
copy /b build-vmonly-shipping\love\Release\love.exe+mygame.love mygame.exe
```

`--compile-game` はゲーム内の `.lh` を**全部** check して書き出す（実行が届かない
スレッドユニットも入る）。`conf.lton` を含む全 `.lton` も、サブディレクトリまで
`std.lton` の writer を通してバイナリ化する。VM 版でも `std.lton.load` で読める。
画像・フォント・音はそのまま複製されるので、出力ディレクトリがそのまま `.love` になる。

- **利得は起動**。登録署名 800 本の解析（4.2ms）が表引き（0.0ms）になり、ゲームの
  check / compile が消える。`lhat.lib` は半分以下だが `love.dll` は 196KB しか縮まない
  （署名表を埋め込むため）
- **`.love` は縮まない**。zip が deflate をかけるので、既に密なバイナリユニットは
  テキストより圧縮が効かない（realgame 実測 4,496 → 5,862 バイト）
- **動かないもの**: `love.thread.newThread(コード文字列)`、`love.filesystem.load` /
  `std.load` のテキスト。実行時に構文解析器が要る。**ファイル版スレッドは動く**
- **デバッガとは併用できる**（`-VmOnly` 単体）。行番号はコンパイル済みでも残るので
  ブレークポイントもスタックも効く。ローカル名が要るなら `--compile-game ... --debug-names`

## エラー表示

型検査エラーは起動前に診断として（lovec はコンソール、love.exe はメッセージボックス）。
実行時の `panic^`（ホストが検出したプログラマエラーを含む）は traceback 付きの青画面。Escape で閉じる。

## Lua 版との違い（ゲーム作者向け）

- **添字は 0 から**（L^ 本体 `5529b14`）。`{ a, b, c }` のキーは 0, 1, 2、`...` の先頭は `...[0]`、頭から走るのは `for^ i from^ 0 to^ t.length^ - 1`、末尾は `t[t.length^ - 1]`。範囲は両端を含むまま。**負の序数（`-1` が最後）は `at` / `substr` / `slice^` / `remove^` / `insert^` などの組込だけ**（`insert^(-1, x)` は末尾に付く = `push^` と同じ。lhat `2e8061e`） — 表の `t[-1]` はキー `-1` を引くので `nil^` になり、静的には `T|nil^` で通ってしまう（nogame の風船がこれで消えた）。**LÖVE の API の添字も 0 から**にした — Lua 版は C++ 本体の 0 起点を Lua 向けに 1 ずらしていたので、そのずらしを外しただけ: Mesh の頂点番号（`getVertex` / `setVertex` / `setVertices` の開始 / `setVertexMap` / `getVertexMap` / 描画範囲の開始）、SpriteBatch の `add` が答える id と `set` / 描画範囲、TextBatch の `add` / `addf` が答える番号と `getWidth(i)` など（省略で全体）、Quad の layer、Joystick の `getAxis` / `getHat` / `isDown` と `joystickaxis` / `joystickhat` / `joystickpressed` / `joystickreleased` イベントの番号、Contact の `getChildren`、Shape の `rayCast` の child、ウィンドウ設定の `displayindex`。**変えていないのは添字ではない番号**: マウスボタン（1 主 / 2 副 / 3 中、SDL 自身の番号）、physics のカテゴリ 1〜16、`love.math.random(n)` の 1〜n（範囲であって添字ではない — 列から選ぶなら `t[love.math.random(0, t.length^ - 1)]`）、touch と joystick の ID
- 型チェックが実行前に走る。typo・引数違いは起動時に診断として報告される
- 文は `;` で区切らない（改行で終わる）。コメントは `#`
- 真偽値は `true^` / `false^`。真偽値以外を条件に使えない（`0` や `""` は false ではない）
- 関数の末尾の式は戻り値にならない。`return^` を書く
- 多値戻りは固定幅タプル。`let^ w, h = love.graphics.getDimensions()` のように分解束縛
- エラーは値。`pcall` は無く `try^` / `catch^` / `isa^` を使う。love.* が返しうるエラーを未処理のまま放置すると静的エラー
- 実行時の `panic^` は traceback 付きでゲームを止める（Lua の error 相当）
- LÖVE オブジェクトの `=` は同一実体なら真（別ラッパでも可）。`is^` はラッパ同一性
- `string.format` は無い。`$"x = {x}"` の補間を使う
- Lua パターン（`string.match` 等）は無い。`s.find/replace/split` の組込か `std.regex`（実正規表現のサブセット。backref/lookaround 無し）
- `table.sort` 等は組込メンバ `t.sort^()`（ハット必須）。比較関数は 3値（負/0/正）
- `math.*` 相当は `std.math`（`import^ std.math` → `std.math.sin/cos/sqrt/atan2/lerp/min/max/...`）。**角度は弧度法**で、`love.graphics.rotate` など LÖVE の API と同じなのでそのまま渡せる（lhat `abe8c9b` まで度数法だった）。度で書きたい時だけ `std.math.rad(deg)` / `std.math.deg(rad)` で変換。`abs/sign/clamp/floor/ceil/round` は `number^` のメンバ（`x.clamp(lo, hi)`）、定数は `std.math.pi` / `tau` / `e`（lhat `743cf41` まで `number^.pi` だった）、表現についての `number^.inf` / `nan`
- 公開したコールバックの型が違うと（例 `update = p^dt:string^`）起動前に診断で止まる
- 実行時のコード読み込みは `love.filesystem.load(path)`（ゲームのファイルシステム経由）か `std.load.file/text`
- `love.physics`: 型は `World` / `Body` / `Shape` / `Joint` / `Contact` の 5 つ。`CircleShape` や `RevoluteJoint` は無く、種別は `shape.getType()` / `joint.getType()` で見分け、種別のメンバ（`setRadius`、`setMotorSpeed`…）は合う種別にだけ呼べる（違う種別へ呼ぶと panic）。`world.setCallbacks(begin, end, presolve, postsolve)` は省いた分がクリアされる。`postsolve` は `(a, b, contact, n1, t1, n2, t2)` の 7 引数固定。`queryShapesInArea` / `rayCast` / `setContactFilter` のコールバックは `p^`（`f^` は外の変数へ代入できない）。座標の列は `getPoints()` → `t^{number^[]}` で受け、`body.getWorldPoints(shape.getPoints()...)...` のように展開して渡す
- タプルを返す呼び出しは `$"..."` の補間スロットへ直接書けない（静的エラー）。`let^ x, y = body.getPosition()` で受けてから補間する
- **スレッド**（`import^ std.thread` / `import^ std.channel`。love.thread は無い）: ワーカーの本体は**同じユニットに書く閉包** — `std.thread.spawn(p^ ... { ... }, 引数...)`。捕捉した外の名前は**写し**で渡る（向こうで書いても戻らない）。答えは `h.join()`、待たずに訊くなら `h.done()` / `h.failed()`、`std.async` に載せるなら `h.awaitable()`。チャネルは `std.channel.new()` / `.named(名前)` で、`push/supply/pop/demand/peek/count/hasRead/clear/atomic`。number（整数は整数のまま）・string・table・閉包・**LOVE オブジェクト**が渡る。ワーカーが落ちたら `threaderror`（`p^string^`）が来る。これらの入口はメモリ不足を含む合併を返すので、`let^ c = try^ std.channel.named("jobs")` と書き、ブロックの末尾に `catch^:` の腕を置く（04 の 4.5。腕はブロック一般の節で、`do^{ … catch^: … }` でも、`p^` / `f^` の本体に直接でもよい）— `try^` は式の位置だけなので捨てる値も `let^_^= try^ c.push(x)`、腕を持つブロックの直下で値としての `catch^` を書くなら括弧が要る
- `love.graphics`: Canvas は `newCanvas(w, h)` が返す Texture（`isCanvas()`）。`setCanvas(c)` / `setCanvas()`。ピクセルを読むには `love.graphics.readbackTexture(c)` → ImageData。`draw(texture, quad, x, y, ...)` は第 2 引数に Quad。`Shader.send(name, ...)` は数値列・`{...}` 表・Texture・Transform を受ける。Mesh の頂点は `{x, y, u, v, r, g, b, a}`（後ろ 6 つ省略可）
- ブロックは `do^{ ... }`。裸の `{ ... }` はテーブル literal なので文にならない。同じ名前を二度作りたい時（`known` を 2 回など）は `do^` で囲んでスコープを分ける

## enum で指定する定数

候補が決まったモードや種別は、文字列ではなくモジュールが公開する enum を使う。
メンバの綴りは従来の文字列と同じ。`DrawMode.fill` と `MeshDrawMode.fan` は別の型で、取り違えは型検査で拒否される。
戻り値にも同じ enum が使われるため、そのまま setter に渡したり、`when^` で網羅的に分岐したりできる。

```lhat
import^love.graphics
import^love.joystick

love.graphics.rectangle(love.graphics.DrawMode.fill, 10, 20, 100, 50)
love.graphics.setBlendMode(love.graphics.BlendMode.alpha)
let^mode, alpha = love.graphics.getBlendMode()
love.graphics.setBlendMode(mode, alpha)

public^let^gamepadpressed = p^stick:love.joystick.Joystick, button:love.joystick.Button {
    if^button = love.joystick.Button.a { print("A pressed") }
}
```

| モジュール | enum |
| --- | --- |
| `love.graphics` | `DrawMode`, `ArcMode`, `AlignMode`, `BlendMode`, `BlendAlphaMode`, `StencilMode`, `LineStyle`, `LineJoin`, `FilterMode`, `WrapMode`, `PixelFormat`, `MipmapMode`, `MeshDrawMode`, `Usage`, `ParticleInsertMode`, `AreaSpreadDistribution` |
| `love.joystick` | `Button`, `Axis`, `Hat` |
| `love.audio` | `SourceType`, `TimeUnit` |
| `love.data` | `EncodeFormat`, `HashFunction`, `CompressedDataFormat` |
| `love.keyboard` | `Key`, `Scancode` |
| `love.mouse` | `Button` |
| `love.physics` | `BodyType`, `ShapeType`, `JointType` |
| `love.filesystem` | `FileMode`, `FileType` |
| `love.sensor` | `SensorType` |
| `love.system` | `PowerState` |

`gamepadpressed` / `gamepadreleased` の第2引数は `Button`、`gamepadaxis` の第2引数は `Axis`、`joystickhat` の第3引数は `Hat`、`sensorupdated` の第1引数は `SensorType`。
`keypressed` / `keyreleased` の第1引数は `Key`、第2引数は `Scancode`。`mousepressed` / `mousereleased` の第3引数は `love.mouse.Button`。

`love.mouse.Button` のメンバは `none` `left` `right` `middle` `forward` `back` `extra1` `extra2` `extra3`。ほかの enum と違い、`.value` が LÖVE のボタン番号そのもの（`left` = 1、`right` = 2、`middle` = 3、`back` = 4、`forward` = 5、`extra1`〜`extra3` = 6〜8、`none` = 0）。`back` / `forward` は SDL の X1 / X2（ブラウザの戻る・進むと同じ）。9 番以降のボタンは `none` で届き、`isDown(none)` は常に偽。

キーとスキャンコードは、LÖVE の定数名が識別子にならないものだけ **SDL の名前を小文字にした綴り**を使う（`"1"` → `digit1`、`"-"` → `minus`、`"kp+"` → `kpplus`、`"nonus#"` → `nonushash`。数字には `digit` を付ける）。識別子になる名前（`a`、`escape`、`kp1`、`lshift` など）は LÖVE と同じ。SDL 自身の命名に従うので、同じ記号でもキーとスキャンコードで綴りが違うものがある（`'` はキーが `quote`、スキャンコードが `apostrophe`）。

LÖVE ではキー配列が出す文字ならどれでもキーになりうる（ドイツ語配列の `ä` など）。宣言に無いそうした文字は `Key.unknown` で届く。配列に依らない入力は `Scancode`（物理位置、閉じた集合）で、打たれた文字は `textinput` で受け取る。
`newCanvas` の設定内の `format` / `mipmaps` も `PixelFormat` / `MipmapMode` を渡す。
省略した引数の既定値は従来どおり。たとえば `setFilter(FilterMode.nearest)` は min / mag 両方を nearest にする。

文字列が必要なパス・テキスト・シェーダーの uniform 名などは文字列のまま。
`conf.lton` は `love.*` を参照できないため、fullscreen などの設定値も文字列を使う。
enum の整数値は C++ の列挙値を公開する契約ではない。メンバ名と型を使うこと。

検証: `python testing/test_enums.py --lovec build/love/Release/lovec.exe`。
通常の `testing/lh/suite` は enum の往復・型の区別・網羅分岐も確認する。

## ソースのコンパイルと自動テスト

```powershell
# LÖVE API を登録した状態でソースをコンパイルする。実行はしない。
.\build\love\Release\lovec.exe --compile -o out path\to\fighter.lh

# LTON データ単体を出力する（out/settings.lton）。
.\build\love\Release\lovec.exe --compile -o out path\to\settings.lton

# ゲーム全体（未参照の .lh、全 .lton、素材も含む）を出力する。
.\build\love\Release\lovec.exe --compile-game out\game path\to\game

# panic でエラー画面を待たず、診断を stderr に出して終了する。
.\build\love\Release\lovec.exe --no-error-screen path\to\game
```

`--compile` の入力が `.lh` の場合、指定したソースと `require^` で到達するユニットだけを、入力ファイルのあるディレクトリからの相対配置で出力する。たとえば `fighter.lh` と `lib/ai.lh` は `out/fighter.lh` と `out/lib/ai.lh` になる。拡張子は `.lh` のまま、中身がバイナリになる。通常のライブラリも対象なので、公開された `draw` や `update` にゲーム用コールバックの型は要求しない。無関係なソース、`conf.lton`、素材は出力しない。

入力が `.lton` の場合、そのデータだけをコンパイルし、同じファイル名で出力ディレクトリに書き出す。拡張子は `.lton` のままで、コンパイル時には実行しない。`--debug-names` も利用できる。不正な LTON はファイル名付きの診断と終了コード 1 で拒否する。

`-o DIR`（または `--output DIR`）は必須。`--debug-names` を添えるとローカル名・捕捉名を残す。バイナリは同じ L^ バージョン・ホスト API を持つエンジンで読む。ソースとバイナリを同じプログラム内で混在させず、実行用エントリから依存グラフ全体をコンパイルする。

型検査だけでなく、コンパイラ段階の失敗も、処理系が持つファイル名・行・列・ソース抜粋・理由を stderr に表示し、終了コード 1 を返す。`--compile-game` が追加で検査する未参照ユニットの失敗にも診断を表示する。コンパイル用オプションはエラーダイアログを開かない。

`--no-error-screen` は実行時のエラー画面とエラーダイアログを抑止する。通常のゲーム画面や描画は無効化しない。panic の本文と位置・トレースを stderr に出し、終了コード 1 で終了する。省略時は従来どおりエラー画面で操作を待つ。`lovec.exe` と `love.exe` の両方で使える。自動テストではこのオプションに加えて、無限ループ対策のプロセスタイムアウトも設定するとよい。

回帰テスト: `python testing/test_cli_compile.py --lovec build/love/Release/lovec.exe`。
