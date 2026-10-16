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

`LGF::Physics::ParticleWorld`は、CPU上でPBDによる球形粒子の移動、粒子間衝突、
摩擦、反発を計算します。`Collision::Sphere`または`Collision::AABB`を境界として
登録すると、粒子をプリミティブの内側へ留められます。

```cpp
LGF::Physics::ParticleWorld particles;
particles.AddBoundary(LGF::Collision::AABB{
    .min = { -2.0f, 0.0f, -2.0f },
    .max = { 2.0f, 4.0f, 2.0f },
});

const LGF::Physics::ParticleHandle particle = particles.Add({
    .position = { 0.0f, 2.0f, 0.0f },
    .radius = 0.05f,
    .mass = 1.0f,
});

particles.Step(static_cast<float>(LGF::System::DeltaTime()));
```

境界には`Collision::AABB`、`Collision::Sphere`、`Collision::Cylinder`を
使用できます。`ParticleBoundaryOptions::cuttingPlane`を指定すると、平面の法線が
向いている側の表面が削除されます。切断面自体は衝突面にならないため、
次の例では上側が開いた半球になります。

```cpp
LGF::Physics::ParticleBoundaryOptions bowlOptions{
    .cuttingPlane = LGF::Collision::Plane{
        // 上向きの法線を指定し、球の上側を削除する。
        .normal = { 0.0f, 1.0f, 0.0f },
        .distance = 0.0f,
    },
};

particles.AddBoundary(
    LGF::Collision::Sphere{ .center = {}, .radius = 2.0f },
    bowlOptions);
```
