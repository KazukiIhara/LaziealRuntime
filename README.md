# LaziealRuntime

LaziealGraphicsFrameworkをゲームアプリケーションから扱いやすくするための
アプリケーション層です。ウィンドウ、入力、フレームループ、時間管理、
エントリーポイントを担当し、描画機能はGit submoduleとして参照する
LaziealGraphicsFrameworkから提供されます。

## クローン

依存リポジトリを含めて取得します。

```powershell
git clone --recursive <repository-url>
```

再帰オプションを付けずにクローンした場合は、次のコマンドを実行してください。

```powershell
git submodule update --init --recursive
```

## プロジェクト生成とビルド

`Project/Premake.bat`を実行した後、生成された
`Project/LaziealRuntime.slnx`をVisual Studioで開き、x64構成をビルドします。
通常の開発には`Develop`構成を使用します。

## 公開API

アプリケーション側では`Project/Include`にある`LGF/LGF.h`をインクルードします。
Runtimeが`WinMain`を所有し、アプリケーションが定義した`ConfigureRuntime()`と
`Main()`を呼び出します。

```cpp
#include <LGF/LGF.h>

void Main() {
    while (LGF::System::Update()) {
        // 更新処理と描画処理
    }
}
```

## 主な役割

- Win32ウィンドウの生成と管理
- メインループとフレームライフサイクル
- DeltaTimeなどの時間管理
- キーボード、マウス、ゲームパッド入力
- SDL3を利用したJoy-Conとモーションセンサー入力
- INIファイルからのランタイム設定読み込み
- LaziealGraphicsFrameworkの初期化と終了処理
