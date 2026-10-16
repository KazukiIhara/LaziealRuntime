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

同じソリューションには、JSONパラメータを編集する独立アプリ
`LaziealParameterEditor`も生成されます。エディタのソースと専用設定は
`Tools/ParameterEditor`にあり、ビルド後の実行一式は
`generated/outputs/<構成>/x64/ParameterEditor`へ出力されます。
初回起動時にパラメータディレクトリを選択し、以降は前回選択した場所を使用します。
ディレクトリはツールバーの`Change...`から変更できます。

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
- 型付きJSONパラメータの読み込みと保存
- LaziealGraphicsFrameworkの初期化と終了処理

## JSONパラメータ

`LGF::Parameter`は、`Assets/Parameters`内のJSONを初回アクセス時に読み込み、
キャッシュした値を静的APIから返します。

```cpp
const LGF::Vector3 position =
    LGF::Parameter::Get<LGF::Vector3>("Sphere.json", "Pos");
const float speed =
    LGF::Parameter::GetOr<float>("Player.json", "Speed", 1.0f);
```

`Get`はファイル、パラメータ、型のいずれかが不正な場合に例外を送出します。
失敗を許容する場合は`TryGet`、既定値を使用する場合は`GetOr`を使用します。
`Reload`で個別のファイルを再読み込みでき、`ClearCache`で全キャッシュを破棄できます。

`LGF::ParameterDocument`は、`/`区切りのパスで階層パラメータを扱います。
JSONの解析に失敗した場合は`ParameterIoResult`に理由が格納され、読み込み前の
ドキュメントは変更されません。

```cpp
LGF::ParameterDocument parameters;
const LGF::ParameterIoResult result =
    parameters.Load("Assets/Parameters/Game.json");
if (result) {
    const float speed = parameters.GetOr<float>("Player/Speed", 1.0f);
}
```

## 物理粒子

`LGF::Physics::ParticleWorld`は、CPU上で球形粒子の落下、粒子間衝突、
静的コライダーとの衝突、摩擦、反発を計算する簡易物理シミュレーターです。
静的コライダーには`Collision::Sphere`と`Collision::AABB`を使用できます。

```cpp
LGF::Physics::ParticleWorld particles({
    .worldBounds = {
        .min = { -20.0f, -10.0f, -20.0f },
        .max = { 20.0f, 20.0f, 20.0f },
    },
});

particles.AddCollider(LGF::Collision::AABB{
    .min = { -5.0f, -0.5f, -5.0f },
    .max = { 5.0f, 0.0f, 5.0f },
});

const LGF::Physics::ParticleHandle particle = particles.Add({
    .position = { 0.0f, 2.0f, 0.0f },
    .radius = 0.05f,
    .mass = 1.0f,
    .collisionParameter = {
        .restitution = 0.2f,
        .friction = 0.5f,
    },
});

particles.Step(static_cast<float>(LGF::System::DeltaTime()));
```

粒子の中心が`ParticleWorldSettings::worldBounds`を越えると、
その粒子はワールドから自動的に削除されます。
