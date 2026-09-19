# love.thread の設計と、std.thread / std.async / carry との比較

> **結末（2026-09-04）: love.thread は廃止した。** lhat が 4 章の 4 点を 4 コミットで埋め
> （`92f5ca3` 共有契約・`4efbd42` std.channel・`3c703a2` 完了の push・`f81d426` program の
> ロック）、lhatove は `std.thread` / `std.channel` / `std.async` を登録して `LhThread` /
> `Channel` / `ThreadModule` / `lh_Thread.cpp` を消した。以下は**その判断に至った調査**として
> 残す。7 章に、実際にやってみて分かったことを足した。

love.thread を lhat の std.thread で置き換えるとしたら何が足りないか、を決めるための調査（2026-09-03）。
基準: lhatove `7e7bf412`、lhat `a1183f6`。

## 0. 前提の訂正: love.thread は VM のみビルドで動く

「love.thread は VM のみに対応できない」は半分だけ正しい。VM のみビルド（`-VmOnly`）で動かないのは
**`newThread(コード文字列)`**（と `File` / `FileData` に入ったテキスト）だけで、これは実行時に
`lhat_program_load_text` = 構文解析器を要するから。**ファイル版 `newThread("worker.lh")` は動く** —
`--compile-game` がゲーム内の `.lh` を全部 check して運び、ワーカーは `lhat_program_check` で
バイナリユニットを読む（実測: `testing/lh/thread` 相当の最小ゲームで exit=11、[main-lh.md](main-lh.md)）。

一方 std.thread は **本体が閉包**なので、VM のみビルドでも何も欠けない。`spawn` は閉包を carry で
運ぶ（proto は共有、捕捉は写し）ので、構文解析器はどこにも要らない。

つまり置き換えの動機は「VM のみで動くかどうか」ではなく、次のどれかになる:

- スレッド本体を**同じユニットの中に閉包として書ける**こと（別ファイル・別スコープにしなくてよい）
- ゲームの L^ コードが LÖVE 以外のホストでも同じ綴りで並行処理を書けること
- エンジン側の独自機構（`LhThread` / `Variant` 経路）を減らすこと

これらは妥当な動機だが、現状の std.thread には love.thread が持つ**チャネルとホストオブジェクトの
受け渡し**が無く、そのままでは置き換えにならない（4 章）。

## 1. love.thread（lhatove の現在形）

Lua 版 LÖVE の love.thread を L^ に移したもの。設計判断は [lua-to-lhat.md](lua-to-lhat.md) の M5。

### Thread

| 項目 | 内容 |
| --- | --- |
| 実体 | `LhThread`（`Threadable`）= OS スレッド 1 本 + 専用 `LhatMachine` 1 台。`lh::Runtime::spawnMachine` が `lhat_machine_new` → `lhat_program_install` → 専用 `ParkingLot` |
| 本体 | **ユニット**（`newThread("worker.lh")`: `lhat_program_check` でプログラムのユニットになり、診断は newThread 時に panic）か **スクリプト**（コード文字列 / File / FileData: `lhat_program_load_text`、proto は Thread が所有） |
| 引数 | `start(...)` の実引数が `Variant` になってワーカーへ渡り、本体は `...` で受ける（`let^args = ...`、要素は `any^`、`fits^` で絞る） |
| 寿命 | `start` → `wait` / `isRunning` / `getError`。終わった Thread は **再 start できる**。**ハンドルを捨てても走り続ける**（OS スレッドが `Threadable` を retain。detach 的） |
| 答え | **無い**。結果はチャネル経由でしか戻らない |
| 誤り | fault / panic は `lh::describeRun` で文字列化 → `getError()`、同時に **`threaderror` コールバック**がメインスレッドのイベント列に積まれる（push 型。ワーカーが勝手に死んでもゲーム側が気づく） |
| プログラムの書込 | check / compile / load / install はすべて `lh::programMutex()` で直列化。走行中の machine は proto を読むだけ |
| デバッガ | ワーカーは DAP のスレッドとして自動的に現れる（`lhat_debug_watch_machines`） |

### Channel

Lua 版と同じ MPMC の FIFO。`Variant` のキューを mutex + condition variable で守る。

| メンバ | 意味 |
| --- | --- |
| `push(v) -> id` | 積んで即返る。id は通し番号 |
| `supply(v[, timeout]) -> bool` | 積み、**誰かが取り出すまで待つ**（ハンドシェイク）。timeout 付きは待ち切れたか |
| `pop() -> any^` | 空なら `nil^` |
| `demand([timeout]) -> any^` | **来るまで待つ**。timeout 付きは切れたら `nil^` |
| `peek()` / `getCount()` / `clear()` | 覗く / 個数 / 空にする（supply の待ちも解く） |
| `hasRead(id)` | その push が取り出されたか |
| `performAtomic(fn, ...)` | チャネルをロックしたまま `fn(channel, ...)` を呼ぶ（複合操作の原子化） |
| `love.thread.getChannel(name)` | **名前付きチャネル**。プロセス全体の表で、どの machine からも同じ実体 |

Channel は `love::Object` なので **hostdata として別 machine へ渡せる**（`Variant::LOVEOBJECT`）。
無名チャネルを `start` の引数やチャネルの中身として送れる。

### 何が渡るか（`lh::variantOf` / `lh::pushVariant`）

| 値 | 経路 | 備考 |
| --- | --- | --- |
| `nil^` `bool^` `string^` | そのまま | 文字列は複製 |
| `number^` | `Variant::NUMBER` = double | **整数が実数になる**（`push(42)` → `pop()` は `42.0`）。int64 の精度も 2^53 で落ちる |
| LÖVE オブジェクト（hostdata） | **ポインタ共有**（retain） | Channel / ImageData / SoundData / ByteData / FileData 等を**そのまま渡せる**。ワーカーで画像を読んでメインに ImageData を返す、という LÖVE の定石はこれで成り立つ |
| table / 閉包 | `lhat_carry` の写し（`lh::Carried` = `love::Object` に包む） | 循環・共有部分表は保たれる。**LÖVE オブジェクトを含む table は運べない**（carry が hostdata を拒む）。Lua 版は `Variant::TABLE` の中にオブジェクトを入れられたので、ここは Lua 版より狭い |
| コルーチン / def^ / 誤りの値 | 拒否 → panic | carry と同じ |

### 実測

- ファイル版スレッドの `newThread` + `start` + `wait` を直列に 50 回: **1.55 ms / 本**（machine の生成と install が主）
- table の `push` + `pop`（carry + uncarry）1 万回: 28 ms = **2.8 µs / 往復**
- ワーカーが `demand()` でブロックしたまま本体が `quit` → プロセスは exit=12 で普通に終わる（OS が畳む）。
  ただし **restart は未検証**: program を作り直す間にワーカーが走っていれば proto の use-after-free になり得る。
  Lua 版は lua_State がスレッドごとに独立していたので無かった問題

## 2. std.thread / std.async / carry（lhat の現在形）

### std.thread（`stdlib/thread.c`）

```lhat
let^h = std.thread.spawn(p^ ... { ... return^ 42 }, "arg1", 2)
if^h fits^std.thread.ThreadHandle {
    let^r = h.join()          # number^|bool^|string^|nil^ | ThreadError | OutOfMemory
}
```

| 項目 | 内容 |
| --- | --- |
| 本体 | **閉包** `p^...`（yieldable 不可）。`lhat_carry` で運ぶ: proto 共有 + 捕捉した場所の**スナップショット**。同じユニットの中に書け、外の名前を捕捉できる（向こうで書いても戻らない） |
| machine | spawn ごとに `lhat_machine_new` + `lhat_program_install`。**登録の `context` は機械間で共有**（ホストが守る） |
| 引数 / 答え | 引数は carry の写し、`join()` が答えを carry で戻す |
| 寿命 | `join()` は 1 回（2 回目は `AlreadyJoined`）。`done()` が非ブロックの問い合わせ。**`dispose()` は未 join ならブロックして join する**（未 join のスレッドがハンドルより長生きすると program 解放後に proto を読む UAF になるため。実クラッシュで確認済みと thread.c にある） |
| 誤り | **値**。`ThreadError.{NotSpawnable, BadArgument, SpawnFailed, BadResult, AlreadyJoined}`。ワーカーの fault は `BadResult` のメッセージに traceback を連結して join 時に戻る。**push 型の通知は無い** |
| `sleep(秒)` | 呼んだスレッドを止める |
| 登録先 | CLI と tests のみ。godot ポートは登録していない |

答えの型に 1 点ずれがある: `thread_main` は `ran.value` を何でも `lhat_carry` するので table も戻るが、
`join` の署名は `number^|bool^|string^|nil^`。table を `return^` した閉包の join は静的には scalar、
実際は table になる。

### std.async（`stdlib/async.c`）

**1 machine のスケジューラ部品**であって、スレッドではない。待ちの表と、それに対する 2 つの問い。

| 関数 | 意味 |
| --- | --- |
| `timer(秒) -> id` | 期限を積む |
| `external() -> id` | 誰かが `lhatstdlib_async_complete(waits, id)` を呼ぶまで済まない待ち。**どのスレッドから呼んでもよい** |
| `wait(秒) -> id\|nil^` | 済んだものを 1 つ取る。`wait(0)` は眠らない（フレーム駆動のホスト用）。正の秒は最長 20 ms の仮眠を繰り返す（push を condition variable で起こさない） |
| `next()` / `pending()` / `drop(id)` | 最短の期限 / 残り数 / 取り下げ |

スケジューラ本体は L^（`sample/async.lh`）: `delay(秒)` と `joined(h)`（`done()` を訊いて `delay(0.002)`）が
コルーチンで、`Scheduler.add(task())` → `poll()` を毎フレーム。LÖVE の `update` にそのまま乗る形
（`sample/asyncpump.lh` がその形を L^ 自身で刻んで見せている）。

Memo.md の std.task 議論の結論も同じ線: 同一 machine ではバックグラウンドで L^ は進まない、
CPU バウンドは std.thread（写しで渡す）の仕事。

### carry（`stdlib/carry.c`）

どの machine にも属さない木（`LhatCarried`）にほどいて向こうで組み直す。

- 運ぶ: `nil^` / `bool^` / `number^`（**int は int のまま**）/ `string^`（複製）/ table（深い写し、循環・共有保持）/
  閉包（proto 共有 + 捕捉セルの写し、共有セルは共有のまま）
- 拒む: コルーチン、def^ とインスタンス、**hostdata / host value / ホストの手続き**、`std.load` の閉包、誤りの値、単位
- hostdata を拒む理由は 05 の 8.8改に明記: 「ポインタの再鋳造は所有（dispose が両機械で走る）の規則を決めないと
  安全に言えず、それはホストの契約の側。`LhatHostDataTag` に『共有可』の印を足す拡張はこの境界の外に置いてある」
- テストは `test_thread.c` が spawn 経由で兼ねる（carry 単体のテストは無い）

## 3. 比較

| 観点 | love.thread（lhatove） | std.thread + async + carry |
| --- | --- | --- |
| 本体の置き場 | 別ユニット or コード文字列。本体は `...` だけを受け、外のスコープは見えない | **同じユニットの閉包**。捕捉はスナップショット |
| VM のみビルド | ファイル版は動く / コード文字列は不可 | **全部動く** |
| 引数の型 | `...`（`any^`、実行時に絞る） | 同じ（`p^...`、13.7 の理由で `any^`） |
| 答え | 無し（チャネルで戻す） | `join()` で 1 値。署名は scalar、実体は carry が運ぶもの |
| 誤りの届き方 | **push**: `threaderror` コールバック + `getError()` | **pull**: `join()` の `BadResult`。join しない限り気づかない |
| 通信 | **Channel**（FIFO、ブロック / 非ブロック / timeout、supply–hasRead のハンドシェイク、performAtomic、名前付き） | **無し**。引数と答えだけ。継続的な通信は `std.async.external` + ホスト側の completion で組む前提 |
| ホストオブジェクト | **ポインタ共有で渡る**（Channel / ImageData / SoundData …） | **拒否**。ワーカーは ImageData を返せない |
| table の中のオブジェクト | 拒否（Lua 版は可） | 拒否 |
| 整数 | 実数に落ちる（Variant） | **保つ** |
| 待ち | `demand` / `supply` が condition variable で眠る | `done()` を訊いて仮眠（`async.wait` は最長 20 ms）。**ブロックする受信が無い** |
| ハンドルを捨てる | 走り続ける（detach 的）。program 解放との競合は未解決（restart） | `dispose()` が join する（安全側） |
| 再利用 | 終わった Thread を再 `start` できる | 1 回きり（spawn し直す） |
| スケジューラとの噛み合い | 無し（`update` で `pop` / `isRunning` を回す） | `await^ async.joined(h)` で書ける。`poll()` をフレームに乗せる形が既にある |
| プログラム書込の直列化 | `lh::programMutex()` が全部覆う | **無い**。`thread_main` の `lhat_program_install` は program を読むだけ、と仮定。実行時の `lhat_program_check` / `load_text`（`love.filesystem.load`、`std.load`）と競合しうる |
| 名前付きの共有点 | `getChannel(name)`（プロセス全体） | 無し |
| DAP | ワーカーがスレッドとして見える | 同じ（machine の watcher は共通） |
| 登録実績 | lhatove | CLI・tests のみ |

**std.thread の側が優れている点**: 本体の置き場（閉包・捕捉）、VM のみビルド、整数の保持、
誤りが値であること、`dispose` の安全性、スケジューラ（`await^`）との自然な接続。

**love.thread の側にしか無いもの**: Channel、ホストオブジェクトの受け渡し、push 型の誤り通知、
ブロックする受信、名前付きの共有点。ゲームでスレッドを使う場面（バックグラウンドの読込・生成、
ジョブキュー）はすべてこちらの機能に依存する。

## 4. std.thread に取り込むべきもの（優先順）

### 4.1 ホストデータを carry で運べるようにする（決定的）

これが無いとワーカーは ImageData も SoundData も Channel も受け取れず・返せず、LÖVE の
スレッド用途が成り立たない。05 の 8.8改が「境界の外」に置いた拡張をそのまま入れる:

- `LhatHostDataTag` に **共有の契約を宣言する口**を足す。ホストが「この型のポインタは machine を跨いで
  よい」と言い、carry がその型の hostdata を **`NODE_HOSTDATA(tag, pointer)`** として運ぶ。
  `lhat_uncarry` は向こうの machine で `lhat_machine_make_hostdata(tag, pointer)` を作る
- 所有の規則は **retain / release** で言う。carry 時に retain、各 machine の `dispose` で release。
  `love::Object` の参照カウントは atomic なので lhatove ではそのまま `Variant::LOVEOBJECT` と同じ形になる
  （`LhatHostDataTag.release` は既にある。足すのは **retain** と「共有可」の印）
- 登録は `lhat_register_hostdata_type` の亜種か、タグに後から立てる関数。lhatove は全 hostdata 型に
  立てることになる（Lua 版が全 `love::Object` を Variant に載せていたのと同じ範囲）
- スレッド安全でない型（graphics の Texture 等）を渡してしまう問題は Lua 版と同じくホストの責任のまま
  でよい。契約を型ごとに立てられるので、lhatove が graphics 型には立てない、という選択もできる

これが入れば **table の中の LÖVE オブジェクト**も自然に運べるようになり、Lua 版の表現力に戻る。

### 4.2 Channel

carry を土台に、`port/thread.h` の mutex + condition で書ける（love の `Channel.cpp` は 180 行）。

- `std.channel.new() -> Channel`、`std.channel.named(name) -> Channel`（プロセス全体の表）
- `push(v) -> number^`、`supply(v[, timeout]) -> bool^`、`pop() -> any^`、`demand([timeout]) -> any^`、
  `peek()`、`count`、`clear()`、`hasRead(id)`
- 中身は **`LhatCarried` のまま保持**し、取り出す machine で uncarry する（lhatove の `lh::Carried` がやっている形）。
  4.1 が入れば hostdata もこの中を通る
- Channel 自身が hostdata なので、**4.1 の共有可の印が立った型**にする。それで spawn の引数として渡せる
- `performAtomic` は「ロックを持ったまま L^ の閉包を呼ぶ」— `lhat_machine_call` の入れ子で書ける。
  ただし閉包の中で同じ Channel を `demand` すると自己デッドロックするのは Lua 版と同じ

`async.wait` が push を condition variable で起こさない設計（「a condition variable here would tie every host
to waking it」）は、Channel には当てはまらない — Channel は自分の condition を持ってよい。

### 4.3 誤りとハンドルの完了を push で届ける

`join()` を呼ばないと fault に気づけないのはゲームでは困る（ワーカーが死んでも `update` は回り続ける）。

- **`spawn` の完了を `std.async.external` の id に結びつける**: `h.awaitable() -> number^`（または spawn の
  時点で id を配る）。`thread_main` の末尾で `lhatstdlib_async_complete` を呼ぶ。`async.joined` の
  「2 ms ごとに `done()` を訊く」ポーリングが消え、`await^ async.joined(h)` が即座に起きる
- **ホスト側の完了フック**: `lhatstdlib_thread_on_finish(program, fn, ctx)`。lhatove はここで
  `threaderror` をイベント列に積む（今 `LhThread::onError` がやっていること）。`context` 共有の規則どおり
  どのスレッドから呼ばれるかはホストが承知する
- 非ブロックで誤りを読む口: `h.failed() -> string^|nil^`（join せずに）。`done()` の隣

### 4.4 プログラム書込のロック

std.thread の `thread_main` は `lhat_program_install` が「読むだけ」で他と競合しないと仮定している。
lhatove は `lhat_program_check` / `load_text` を実行時にも呼ぶ（`love.filesystem.load`、`std.load`、
ファイル版 `newThread`）ので、**program の書込と install の直列化は lhat の側に要る**。案:

- `lhat_program_set_lock(program, lock_fn, unlock_fn, ctx)` を足し、program.c の書込経路と
  `lhat_program_install` がそれを取る。ホストが渡さなければ従来どおり
- または `lhatthread` をリンクするビルドでは program が自前の `LhatMutex` を持つ

VM のみビルドでも `lhat_program_load_text` がバイナリを受けるなら実行時書込は残るので、この問題は
VM のみでは消えない。

### 4.5 program 破棄前に全スレッドを join する口

std.thread は「未 join の dispose がブロックする」ことで UAF を防いでいるが、**ハンドルを捨てた
ワーカーが走ったまま program が破棄される**経路はホスト任せ（restart がまさにそれ）。
`lhatstdlib_thread_join_all(program)` か、`lhat_program_on_dispose` の順序に乗せて std.thread 自身が
残りを join する。lhatove は restart の直前にこれを呼ぶ。

### 4.6 小さいもの

- `join` の答えの型を carry が運ぶものに合わせる（`any^`、または hostdata も含む合併）。
  今は table が scalar として届く
- 整数の保持は std の側が正しい。love.thread を残すなら `Variant::NUMBER` の代わりに `lh::Carried` を
  数にも使う（`variantOf` の 1 行）
- 再 `start` は要らない（spawn し直せばよい）。detach は要らない（4.5 で十分）

## 5. 移行の道筋（lhatove 側）

4.1 と 4.2 が lhat に入るまでの間、**混成は今すぐ動く**:

- `std.thread` / `std.async` を登録する（`registerStdlib` に 2 行）。登録はプロセス寿命の識別なので
  restart にも耐える
- std.thread のワーカーから `love.thread.getChannel("name")` は**そのまま使える** — 登録は各 machine に
  install されるので、名前付きチャネルはプロセス全体の表を指す。渡せないのはチャネルの**ハンドル**だけ
- ただし std.thread のワーカーが受ける `context` 共有の規則は lhatove の `binding` static に当てはまる
  （登録後は読むだけ）。`ParkingLot::lotOf` は mutex 付き
- 4.4 が無い間、ワーカーの `lhat_program_install` と本体の `programMutex()` は噛み合わない。
  `love.filesystem.load` と spawn を同時に呼ばないという運用でしのぐか、`lh::Runtime::spawnMachine` を
  std.thread に使わせる口（= 4.4 のホスト側）を先に作る

4.1 〜 4.3 が入った時点で love.thread の Thread は std.thread に、Channel は std.channel に置き換えられ、
lhatove に残るのは `threaderror` を積む完了フックの 10 行だけになる。それまでは love.thread を残す
（ファイル版は VM のみビルドでも動くので、急ぐ理由は無い）。

## 6. スケジューラとプールは無い（両方とも）

love.thread にも std.thread にも、タスクスケジューラもワーカープールも無い。**1 タスク = 1 OS スレッド = 1 machine**。

- love.thread: `Thread.start()` ごとに `SDL_CreateThread`。終了後に再 `start` しても新しい OS スレッドと
  新しい machine（`LhThread::threadFunction` が毎回 `spawnMachine`）。Lua 版も同じ
- std.thread: `spawn` ごとに OS スレッドと machine

違いは**プールを利用者が組めるか**。love.thread では N 本の Thread + `jobs` / `results` チャネルで組める
（`testing/lh/thread/worker.lh` の `repeat^{ let^job = jobs.demand() ... }` がその形）。成立条件は
Channel の `demand` = ブロックする受信。std.thread にはそれが無いので、ワーカーがジョブを待つ手段が
`done()` ポーリングか `sleep` しか無く、プールを組めない — 4.2 が要る理由の言い換え。

**goroutine 型（M:N、コルーチンが OS スレッドを渡り歩く）は lhat の設計上どちらにも作れない。**
コルーチンは machine のフレームそのもの、machine は 1 スレッド 1 台、carry はコルーチンを拒む
（Memo.md の std.task の結論と同じ）。作れるのは 2 種:

1. **同一 machine の協調スケジューラ** — std.async + `sample/async.lh` の `Scheduler`。並列性は無く、
   `await^` の見た目と「フレームを跨ぐタスク」が得られる
2. **machine プール** — N 台の machine / スレッドを常駐させ、ジョブ = carry した閉包（proto 共有・
   捕捉の写し）をキューで配り、答えを carry で戻す。ジョブは run-to-completion で、途中で別スレッドへ
   移れない。Elixir の「軽量プロセスが数本のスケジューラを渡り歩く」形にはならない

プールの利得は起動費: machine 生成 + install + OS スレッド起動で **1.55 ms / 本**（1 章の実測）。
毎フレーム spawn する用途はプールが要る。

この観点でも高機能なのは love.thread だが、理由はスケジューラではなく **Channel** — プールを利用者が
書ける最低限の部品を持つこと。std に Channel（4.2）が入れば同等になり、さらに「machine プール +
閉包ジョブ」を stdlib に置けば love.thread を超える（love は本体がユニットかコード文字列なので、
閉包をジョブとして配れない）。

## 7. やってみて分かったこと（2026-09-04）

移してから見えた、4 章に書けていなかったこと。

- **合併を返す入口が増えるが、読むのは `try^` の仕事。** `std.channel.new()` も `named()` も
  `spawn()` も `push()` も `|std.error.OutOfMemory` を含む合併で、合併にメンバは無い
  （`typeof^(c).signature` が `std.channel.Channel` と答えるのは実行時の型なので紛らわしい）。
  ただしこれは `fits^` で包む話ではない — 04 の 4.5 の `catch^` 節（腕）がまさにこのために
  在り、`try^ e` が値を取り出して拒否だけを腕へ送るので、**本体は失敗しない綴りのまま**書ける。
  腕はブロック一般の節なので `do^{ … catch^: … }` と囲んでも、`p^` / `f^` の本体に直接書いてもよい
  （これを書いた時点では専用の `try^{ }` だった。lhat `db2a7b9` で廃止され、腕が節になった）。
  `fits^` は「その誤りが来ること自体が主張」の場所にだけ残す（`AlreadyJoined` / `BadResult`）。
  love.thread が `p^ -> love.thread.Channel;` と言い切っていた分の差は、呼び出しごとの `try^`
  1 語と、ブロックの末尾の腕 1 つ。テストは両方ともこの形

  2 つ覚えることがある: **`try^` は式の位置のみ**（文頭に置くと命令モードの呼び出しに読まれるので、
  捨てる値も `let^_^= try^ c.push(x)` と束縛する）と、**腕を持つブロックの直下の `catch^` は腕を開く語**
  なので値としての `catch^` は括弧が要る（`let^ v = (f() catch^ nil^)`）。
  「呼び出し文の直後に `try^{` を置けない」はここで見つけたパーザのバグで、`f43f8b1` で直った
  （その構文自体が今は無い）
- **`import^` はワーカーの中に書かない。** 本体は**このユニットの閉包**なので、ユニットが
  import したものをそのまま名前で引ける（向こうの `L^.modules` から解決される）。
  閉包の中に `import^` を書くのは構文として通らない
- **`ParkingLot` は遅延 attach が要る。** std.thread は lhatove の見ていないところで機械を作る
  （`lhat_machine_new` + `lhat_program_install` だけ）。`lotOf` が nullptr を返すと
  `lh::Parked` が黙って死んだコールバックになるので、無ければその場で作る形にした。
  呼ばれるのはホスト関数の中＝その機械自身のスレッドなので、表を作るのは安全
- **`lhat_program_set_lock` は入れ子にならない。** ホストが手で取っていたロックは**全部外す**
  （でないと自分でデッドロックする）。lhatove では `Runtime::check` / `compile` /
  `spawnMachine` / `love.filesystem.load` の 4 箇所。危険な再入は 1 経路だけ残る —
  **ホストの loader はロック保持中に呼ばれる**（`lhat_program_check` の内側）ので、
  loader から program を触ってはいけない。再帰ロックにすべきではない: 対を渡すのはホストなので
  `std::recursive_mutex` を渡すのは今日でもできるが、再入した先が見るのは**書き換え途中の
  unit 表**で、デッドロック（うるさい）が破壊（静か）に変わるだけ
- **ワーカーの失敗文が痩せていた** — `lh::describeRun` は panic の値と行番号を綴るのに、
  std.thread の `failure_text` は状態名 + traceback だけで、最上位 panic は深さ 1 で
  traceback も無いので `panic^` の 1 語になっていた。報告して直った（`threaderror` は今
  `panic^: "boom from a thread" (line 75)` と言う）
- **VM のみビルドでスレッドが完全に動く。** 本体が閉包だから構文解析器が要らない。
  `testing/lh/thread` は 1 ユニットにコンパイルされ、VM 版で exit=8
- **DAP は何も変わらない。** `lhat_debug_watch_machines` が `lhat_machine_new` を拾うので、
  spawn した閉包の中のブレークポイントがそのまま効く（threadId 2 で停止を確認）
