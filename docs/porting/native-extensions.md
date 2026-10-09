# ネイティブ拡張

デスクトップ版は、ゲームの `extensions.txt` に列挙された共有ライブラリから、型検査前に L^ のホストバインドを登録する。EOS などのサービス固有の実装・SDK は別リポジトリで開発できる。共通の C ABI・登録処理・ライブラリ寿命管理は lhat 本体が提供し、LÔVE はリストの読み取り・パスの制約・SDL によるロードを担当する。

## ゲーム側の指定

ゲームのルートに UTF-8 の `extensions.txt` を置く。1 行に 1 ライブラリ。空行と、前後の空白を取り除いた後で `#` から始まる行は無視する。BOM・LF・CRLF を受け付ける。

```text
# Platform suffixes may be omitted.
native/eos
```

拡張子を省略すると Windows は `.dll`、Linux は `.so`、macOS は `.dylib` を補う。`lib` プレフィックスは補わない。明示した `.dll` / `.so` / `.dylib` はそのまま使う。OS・CPU に合うライブラリを配布する。

| ゲーム形式 | リストの場所 | ライブラリの相対パスの基準 |
| --- | --- | --- |
| ディレクトリ | ゲームルートの `extensions.txt` | ゲームディレクトリ |
| `.love` | アーカイブ内の `extensions.txt` | `.love` を置いた実ディレクトリ |
| fused 実行ファイル | 埋め込まれたアーカイブ内の `extensions.txt` | 実行ファイルを置いた実ディレクトリ |

共有ライブラリとその依存ライブラリは実ファイルとして配置する。アーカイブ内からの DLL 展開は行わない。例えば fused 配布は次の形になる。

```text
MyGame.exe               # contains extensions.txt: native/eos
native/eos.dll
native/EOSSDK-....dll
```

依存ライブラリの解決には OS の規則が適用される。Unix では必要に応じて `$ORIGIN` / `@loader_path` 等を拡張側のビルドで設定する。

リストがなければ拡張なし。セーブディレクトリや、アーカイブ・実行ファイルの隣のリストへのフォールバックはしない。現在は外部 MOD リストを追加する設定も設けていない。DLL パスは `/` 区切りの相対パスで、絶対パス・`.`・`..`・基準ディレクトリ外への解決・重複指定は拒否する。OS の検索パスで拡張本体を探さず、解決済みの絶対パスでロードする。

指定されたライブラリの欠落・入口の欠落・ABI 不一致・登録失敗は起動エラーとなる。`--no-error-screen` を付けると stderr と終了コード 1 で受け取れる。この仕組みは外置きのリスト追加を防ぐためのもので、配布済み DLL の差し替えを検出するものではない。

## 拡張側の ABI

公開ヘッダは lhat の `include/lhat/extension.h`（`#include <lhat/extension.h>`）。対応する L^ の公開ヘッダと、エンジンをビルドした際の生成ヘッダ `lhat/version.h` を include パスに加える。**lhat の静的ライブラリや liblove を拡張へリンクしない**。非 inline の L^ API は、ホストから渡された `LhatExtensionAPI` の関数ポインタで呼ぶ。これによりレジストリとランタイムを本体と共有する。

`lhat_extension_v2` を C リンケージで export し、静的寿命の `LhatExtension` を返す。C++ でも公開ヘッダの宣言が C リンケージを指定する。入口では記述子を返すだけにして、L^ API の呼び出しやネットワーク接続は行わない。ローダーは ABI 番号・構造体サイズ・L^ の版数・値サイズを確認してから `register_bindings` を呼ぶ。独自の公開ヘッダ改変や ABI の異なるビルド間での互換性は保証しない。

起動順は次のとおり。

1. C++ のファイルシステムを初期化し、ゲームソースをマウントする。
2. セーブ領域をマウントする前に、そのソースの `extensions.txt` を読む。
3. L^ Program を作り、`love.*` と標準ライブラリを登録する。
4. 指定された DLL をロードして記述子を検証する。
5. 全拡張の `LHAT_EXTENSION_TYPES` をリスト順に実行する。
6. 全拡張の `LHAT_EXTENSION_MEMBERS` をリスト順に実行する。
7. ゲームを型検査・コンパイル・実行する。`conf.lton` の処理順は変更しない。

型・enum・エラー宣言は TYPES、関数・メンバは MEMBERS で登録する。別の拡張が宣言する型も MEMBERS の時点では使える。基底型など TYPES 同士の依存がある場合は、依存先を先に列挙する。失敗時は説明文字列、成功時は NULL を返す。C ABI を越えて例外を送出してはならない。

`void **state` は Program ごとに NULL で始まり、その Program の 2 回の呼び出しで共有する。寿命のある状態には `host->lhat_program_on_dispose` で解放処理を登録する。登録途中の失敗時にも解放できるよう、確保直後に登録する。

登録は実行時だけでなく、`--dump-host-api`、`--dump-signatures`、コンパイル時にも行われる。並列 LTON コンパイラは複数の Program を作り、スレッド開始前にそれぞれの登録を完了する。したがって、登録中に EOS の起動・ログインなどを実行せず、ゲームが呼ぶ関数として公開する。型の identity はプロセス共有なので、解放・共有契約など identity に属するコールバックにはプロセス寿命の context を使う。

ABI 2 の記述子の任意の `shutdown` は、全 Program とレジストリを破棄した後、DLL を解放する直前に呼ばれる。SDK 全体の終了処理はここへ置く。

DLL は restart を越えて保持し、すべての Program・machine・プロセス共有レジストリを破棄した後に、ロードの逆順で解放する。拡張独自のワーカーや非同期処理は Program の解放までに終了させる。ゲーム実行途中に新しい拡張を追加する API は提供しない。

## VM 専用版の署名表

**署名表は各 DLL に埋め込む。** フル版での登録には不要なので、最初は `signatures = NULL`、`signatures_size = 0` でビルドできる。

1. 開発用ゲームの `extensions.txt` に拡張とその依存拡張を列挙する。
2. フル版で、登録済み署名表を出力する。ゲーム本体は実行されない。
3. バイト列を C 配列として埋め込み、記述子の `signatures` と `signatures_size` から参照する。
4. DLL を再ビルドする。同じ DLL がフル版と VM 専用版の双方で使える。

```powershell
.\build\love\Release\lovec.exe --dump-signatures eos-signatures.bin path\to\devgame
.\scripts\bin2header.ps1 -In eos-signatures.bin -Out eos-signatures.h -Name eos_signatures
```

出力表には本体と標準ライブラリの署名も含まれるが、そのまま埋め込める。各拡張は独立した表を持てる。VM 専用版はその拡張の登録を呼ぶ直前に表を切り替える。登録済みの型や関数は保持される。表の欠落・不正な形式・異なる L^ バイナリ形式は拒否する。登録変更や L^ 更新時は表を再生成する。

`--compile-game` は拡張を登録した状態でゲームをコンパイルし、`extensions.txt` も通常のアセットとしてコピーする。ディレクトリ内の DLL もコピー対象だが、入力がアーカイブで外置き DLL を使っている場合、その DLL と依存ライブラリは配布先へ別途配置する。`--compile -o` は従来どおりユニットのみを書き出すので、配布用には `--compile-game` を使う。

LSP 用の型情報も、拡張を列挙したゲームを指定して生成する。

```powershell
.\build\love\Release\lovec.exe --dump-host-api path\to\game\lhat-host.json path\to\game
```

ゲームを指定せず、DLL を直接指定することもできる。`--extension` は複数回指定でき、通常版・VM 専用版の双方で使える。

```powershell
.\build\love\Release\lovec.exe --dump-host-api lhat-host.json --extension path\to\eos_lhat.dll
```

パスは実行時のカレントディレクトリ基準、または絶対パスで指定し、`.dll` / `.so` / `.dylib` などの拡張子も含める。ゲームも指定した場合は、その `extensions.txt` の後に追加する。同じライブラリの重複指定はエラーになる。`--extension` は `--dump-host-api` 専用で、通常起動や fused 実行ファイルでは使えない。

## ホスト側の共通ヘルパー

lhat の `lhat_extensions_*` API が、記述子の検証、TYPES → MEMBERS の登録、VM 署名表の切り替え、読み込み済みライブラリの保持と解放を担当する。ホストは `LhatExtensionLoader` に open / symbol / close / error を渡す。LÔVE は `src/lh/Extensions.cpp` で SDL の操作を渡し、ゲームソースまたは明示的な `--extension` 指定から決めた絶対パスをロードする。

ほかのホストは同じ公開ヘッダとヘルパーを利用し、リスト形式やロード許可の規則を独自に決められる。静的リンクした拡張は `lhat_extensions_add` で同じ登録処理を使える。詳細は lhat の `DesignDocuments/05-modules.md` §8.13。

## テスト

[`testing/native-extension`](../../testing/native-extension) は、エンジンにリンクせずに作る C の実例と失敗ケース。[`testing/test_extensions.py`](../../testing/test_extensions.py) は CMake でそれらをビルドし、通常版・VM 版、別々の埋め込み署名表、複数拡張の型参照、並列コンパイル、restart、解放、リストの読み取り元、エラー診断を検証する。Windows では実行ファイルへアーカイブを連結して fused も検証する。

```powershell
python testing/test_extensions.py
```

他のビルド配置では `--lovec`、`--vm`、`--lhat`、`--lhat-generated` で指定する。後者は `lhat/version.h` の親の親に当たる include ディレクトリ。
