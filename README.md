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
アプリケーションのソリューションから起動するときは、同じプロジェクト定義を
`Tools/ParameterEditor/premake5.lua`から取り込み、
`--parameter-directory "<フォルダのパス>"`で編集先を指定できます。
起動引数で指定したフォルダは前回の選択より優先され、起動時にユーザー設定へ保存されません。

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

## レベルデータ

`LevelData` のJSONは、`Type: "Level"`、`Version`、2つのパス `UI`、`World` を持ちます。
相対パスはこのJSONがあるフォルダを基準とし、使わない参照は空文字列にできます。
同じUIやワールドの配置ファイルを複数のレベルで参照できます。

```cpp
const auto result = LGF::SetLevelData("Level1.json");
if (!result) {
    // result.message に読み込み失敗の理由が入ります。
}
```

`SetLevelData` の相対パスは `Assets/LevelDatas` が基準です。
読み込んだ画面UIはRuntimeが更新・描画し、アンカーの基準には現在のウィンドウサイズを使います。
`GetLevelUIWorld()` と `FindLevelUIElement("PlayButton")` でボタンなどを取得できます。
レベルを切り替えたり `ClearLevelData()` を呼んだりしたあとは、ポインタとハンドルを取得し直してください。
ワールド配置の参照先にある `Objects` から `GameObjectWorld` を生成し、3Dモデルと2DスプライトをRuntimeが描画します。
`GetLevelGameObjectWorld()` で現在のWorldを取得でき、`Find("Box01")` で配置IDからハンドルを引けます。
個体は `Get(handle)` で取得し、`GetComponent<Transform3D>()` などでコンポーネントを参照できます。
ワールド配置の参照パスは `GetLevelData()` で取得できます。

ワールド配置JSONは `Type: "WorldLayout"`、`Version`、`Objects` を持ちます。
`LoadWorldLayout` と `SaveWorldLayout` はRuntimeと編集ツールで共有できます。
各オブジェクトには `Id` と
`Components` を指定でき、`Name` と `Enabled` は省略できます。
`Transform3D` の `Rotation` は `[x, y, z, w]` のクォータニオンです。
`ModelRenderer` を付ける場合は `Transform3D` も指定します。
`ModelRenderer` の `Model` にある相対パスは、ワールド配置JSONのフォルダを基準に解決します。

2Dオブジェクトには `Transform2D` と `SpriteRenderer` を組み合わせます。
`SpriteRenderer` の `Texture` はワールド配置JSONを基準にした相対パスで指定できます。
`Pivot` は画像内の基準点で、左上は `[0, 0]`、中央は `[0.5, 0.5]`、右下は `[1, 1]` です。
省略時は中央になります。`Transform2D` の `Position` にピボットを置き、
`Scale` はテクスチャ本来のサイズに対する倍率、`Rotation` は時計回りのラジアンです。

## 2Dレベルデータ

`LGF::LevelData2D` はレベル名と2Dオブジェクトの一覧を持ちます。
1レベルにつき1つのJSONファイルを使い、ゲームと編集ツールで同じ入出力APIを共有できます。

```cpp
LGF::LevelData2D level;
const auto loaded = LGF::LoadLevel2D("Assets/LevelDatas/World/Example2D.json", level);
if (loaded) {
    const auto saved = LGF::SaveLevel2D("Assets/LevelDatas/World/Example2DCopy.json", level);
    // 保存に失敗した場合は saved.message で理由を取得できます。
}
```

JSONの必須項目は `Version`（現在は `1`）と `Objects` 配列です。
各オブジェクトの `Id` はレベル内で一意にし、`Type` に用途を示す名前を指定します。
`Transform` の `Translation` と `Scale` は2要素の配列で、`Rotation` はラジアンです。
省略した座標と回転角は0、拡大率は `[1, 1]` になります。
`Name` と `AssetPath` は省略できます。オブジェクトの生成、種別の解釈、
アセットパスの解決は、読み込んだアプリケーションが行います。

## 画面UIレイアウト

`LaziealLevelEditor` は `UIWorld` の画面用レイアウトを編集します。
画面UIのJSONには `Type: "UILayout2D"` を保存します。
Canvas・Panel・Label・Image・Button を階層上で配置し、位置はImGuizmoでも変更できます。
画像の相対パスはレイアウトJSONのディレクトリを基準に解決されます。
プレビューでは `UIWorld` がパネル、ボタン、テキスト、画像のスプライトを描画します。

実行ファイルは `generated/outputs/<構成>/x64/LevelEditor` に出力されます。
`--level-data-directory "<フォルダのパス>"` で、ファイル選択の初期フォルダを指定できます。
エディタのトップ画面で `UI Edit`、`World Edit`、`Level Preview` を選びます。
UI配置は `LevelDatas/UI`、2D・3Dのワールド配置は `LevelDatas/World` に置けます。
ゲーム側では `LoadUILayout2D` でJSONを読み、フォントと画像の読み込み処理を
`CreateUIWorld2D` に渡すと同じUIWorldを作成できます。

## 物理粒子

`LGF::Physics::PhysicsWorld`は、GPU上で球形粒子の落下、粒子間衝突、
静的オブジェクトとの衝突、摩擦、反発を計算する簡易物理シミュレーターです。
静的オブジェクトには`Collision::Sphere`、`Collision::SphericalCap`、
`Collision::SphericalShellCap`、`Collision::AABB`、`Collision::OBB`を使用できます。

```cpp
LGF::Physics::PhysicsWorld particles({
    .worldBounds = {
        .min = { -20.0f, -10.0f, -20.0f },
        .max = { 20.0f, 20.0f, 20.0f },
    },
});

particles.AddStaticObject(LGF::Collision::AABB{
    .min = { -5.0f, -0.5f, -5.0f },
    .max = { 5.0f, 0.0f, 5.0f },
});

const LGF::Physics::PhysicsParticleHandle particle = particles.AddParticle({
    .position = { 0.0f, 2.0f, 0.0f },
    .radius = 0.05f,
    .mass = 1.0f,
    .material = {
        .restitution = 0.2f,
        .friction = 0.5f,
    },
});

particles.Update(static_cast<float>(LGF::System::DeltaTime()));
```

粒子の中心が`PhysicsWorldSettings::worldBounds`を越えると、
その粒子はワールドから自動的に削除されます。

アルゴリズムと内部実装については、[PhysicsParticle実装解説](Docs/PhysicsParticle.md)を参照してください。

## Effekseerエフェクト

`LGF::VFX::EffectWorld`はEffekseerのエフェクトを同期読み込みし、再生個体を
世代付きハンドルで管理します。相対パスはアプリケーションの作業ディレクトリを
基準に解決され、描画はメイン3Dカメラを使用してHDR透明パスで自動実行されます。

```cpp
LGF::VFX::EffectWorld effectWorld;
LGF::VFX::Effect explosion = effectWorld.Load("Assets/Effects/Explosion.efkefc");

LGF::Transform3D transform{};
transform.translation = { 0.0f, 1.0f, 0.0f };
LGF::VFX::EffectHandle explosionHandle = effectWorld.Play(explosion, transform);

while (LGF::System::Update()) {
    effectWorld.Update(static_cast<float>(LGF::System::DeltaTime()));
}
```

`Stop()`、`IsPlaying()`、`SetTransform()`で再生個体を操作できます。現在は
メインパス、同期読み込みに限定し、歪み、深度テクスチャ連携、
カスタムRenderPass、非同期読み込みには対応していません。


