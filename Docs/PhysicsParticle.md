# PhysicsParticle GPU実装

## 概要

`LGF::Physics::PhysicsWorld`のPhysicsParticleは、球形粒子の状態と物理計算をGPU上に保持する物理シミュレーションです。
煙や火花を扱うVFXの`ParticleWorld`とは用途を分け、質量、外力、粒子間衝突、Static/Kinematic Colliderとの衝突を扱います。

公開APIとハンドル管理はLaziealRuntime、DirectX 12リソースとCompute Shaderの実行はLaziealGraphicsFrameworkが担当します。

```text
PhysicsWorld
  ├─ 世代付きハンドル
  ├─ Particle Command
  └─ ColliderのGPU表現への変換
          ↓
LGF::GpuCompute
          ↓
GpuPhysicsParticleBackend
  ├─ Structured Buffer
  ├─ Spatial Hash
  ├─ Jacobi Solver
  └─ 非同期Readback
```

主な実装ファイルは次のとおりです。

- 公開API: `Project/Include/LGF/Physics/PhysicsWorld.h`
- Runtime側実装: `Project/Src/Physics/PhysicsParticle.cpp`
- GPU公開型: `LaziealGraphicsFramework/Project/Include/LGF/Graphics/GpuPhysicsParticle.h`
- GPUバックエンド: `LaziealGraphicsFramework/Project/Src/Graphics/Compute/PhysicsParticle/`
- Compute Shader: `LaziealGraphicsFramework/Project/Assets/Shaders/Compute/GpuPhysicsParticle*`

## 公開API

粒子は`PhysicsParticleDesc`から生成します。

```cpp
const auto particle = world.AddParticle({
    .position = { 0.0f, 5.0f, 0.0f },
    .velocity = {},
    .radius = 0.05f,
    .mass = 1.0f,
    .material = {
        .restitution = 0.2f,
        .friction = 0.5f,
    },
});
```

生成後はハンドルを使って個別に操作できます。

```cpp
world.SetParticlePosition(particle, position);
world.SetParticleVelocity(particle, velocity);
world.SetParticleMass(particle, mass);
world.SetParticleRadius(particle, radius);
world.SetParticleMaterial(particle, material);
world.SetParticleEnabled(particle, enabled);
world.AddParticleForce(particle, force);
world.AddParticleImpulse(particle, impulse);
```

これらの呼び出しはCPU上で軽量なコマンドを追加します。コマンドは次の`Update()`で登録順にGPUへ適用されます。
GPUリソースを作成するため、`PhysicsWorld`はGraphicsSystemの初期化後に生成してください。

### GPU直接描画

同じ見た目の粒子はRender Groupへまとめます。粒子はGPU上でGroup Indexだけを保持し、
物理計算結果のParticle BufferをMesh Shaderが直接参照するため、通常描画ではReadbackが発生しません。

```cpp
const auto riceGroup = world.CreateParticleRenderGroup({
    .visual = LGF::Physics::PhysicsParticleSphereVisual{},
    .color = { 0.95f, 0.92f, 0.78f, 1.0f },
});

const auto rice = world.AddParticle({
    .position = spawnPosition,
    .radius = 0.07f,
    .renderGroup = riceGroup,
});

// Draw内でWorldにつき1回
world.DrawParticleRenderGroups();
```

静的ModelもGroupのVisualに指定できます。Modelの表示Scaleには粒子半径が乗算されます。

```cpp
const auto modelGroup = world.CreateParticleRenderGroup({
    .visual = LGF::Physics::PhysicsParticleModelVisual{
        .model = riceModel,
        .scale = { 1.0f, 1.0f, 1.0f },
    },
});
```

`SetParticleRenderGroup()`はほかの粒子Setterと同様に、次の`Update()`で適用されるコマンドです。
GroupのVisualや色を変える場合は`SetParticleRenderGroupDesc()`を使います。
削除済みGroupを参照する粒子は描画されません。

粒子状態の同期取得APIはありません。GPUを待つ同期Readbackを避け、粒子状態をGPUに常駐させるためです。
CPU側で状態が必要な場合は明示的に非同期Readbackを要求します。

```cpp
world.RequestParticleStates();

// 数フレーム後
std::vector<LGF::Physics::PhysicsParticleState> states;
uint32_t latencyFrames{};
if (world.GetLatestParticleStates(states, latencyFrames)) {
    // statesはlatencyFrames前の完了済みスナップショット
}
```

要求しないフレームではParticle BufferのReadbackは発生しません。

## PhysicsWorldSettings

従来の物理パラメーターに加え、GPUリソース容量を生成時に指定します。

| メンバー | 意味 |
|---|---|
| `maximumParticleCount` | GPUに確保する粒子スロット数 |
| `maximumColliderCount` | 1サブステップで使用できるStatic/Kinematic Collider数 |
| `maximumParticleCommandCount` | 1フレームに転送できるParticle Command数 |
| `maximumParticleRadius` | Spatial Hashが対応する最大粒子半径 |

これらの値と`substepCount`はGPUリソース生成後に変更できません。`SetSettings()`では生成時の値が維持されます。
半径が`maximumParticleRadius`を超える粒子は追加・変更できません。

## 1フレームの処理

GPUでは次の順番で処理します。

```text
ResetCounters
ApplyCommands

for each substep:
    Integrate
    ClearSpatialHash
    BuildSpatialHash

    for each solver iteration:
        SolvePositions
        ApplyPositions

    SolveVelocities
    ApplyVelocities

Cleanup
CopyStatisticsAndRetiredParticles
```

初版ではDirect Queue上でCompute Shaderを実行します。GraphicsSystemはGPU物理粒子をRendererより先にDispatchします。

## Spatial Hash

ワールド全体を密な3D配列にすると、広いワールドと小さな粒子の組み合わせでセル数が極端に増えます。
そのため、セル座標を固定数のBucketへ割り当てるSparse Spatial Hashを使用します。

```text
BucketHeads[約 maximumParticleCount × 2]
ParticleCells[maximumParticleCount]
ParticleNext[maximumParticleCount]
```

各粒子は`InterlockedExchange`でBucketのLinked Listへ追加されます。
探索時は周囲27セルのBucketを参照し、保存した整数セル座標を比較してハッシュ衝突を除外します。

セル幅は次の固定値です。

```text
cellSize = maximumParticleRadius × 2
```

## Particle-Centric Jacobi Solver

CPU版のように1つの衝突ペアから両粒子を書き換えると、GPU上では複数スレッドが同じ粒子へ書き込んで競合します。

GPU版では各スレッドが周囲の粒子とColliderを読み、自分自身の補正量だけを`Correction Buffer`へ書き込みます。
全スレッドの計算後、別のCompute Passで補正を粒子位置へ適用します。

```text
Particle State --read--> SolvePositions --write--> Corrections
      ↑                                           |
      └---------------- ApplyPositions <----------┘
```

float atomicに依存しないため、実行順序による書き込み競合を避けられます。
CPU版の逐次ソルバーとは計算順が異なるため、数値結果の完全一致は保証しません。

## Collider

PhysicsParticleが衝突する対象は次のとおりです。

| 組み合わせ | 対応 |
|---|---:|
| PhysicsParticle × PhysicsParticle | 対応 |
| PhysicsParticle × StaticObject | 対応 |
| PhysicsParticle × KinematicObject | 対応 |
| PhysicsParticle × DynamicObject | 非対応 |

対応形状はSphere、SphericalCap、SphericalShellCap、AABB、OBBです。
Runtime側で各サブステップのKinematic形状を補間し、LGFへ依存方向を逆転させない共通GPU表現へ変換します。

## ハンドルと自動削除

CPU上には粒子の位置や速度ではなく、スロットの使用状態と世代番号だけを保持します。
`PhysicsParticleHandle::index`はGPU Particle Bufferのスロット番号と一致します。

GPUが非数または`worldBounds`外の粒子を終了させると、`index`と`generation`を終了通知バッファへ追加します。
通知はFrame ContextごとのReadback Bufferから非同期で回収されるため、`IsValid()`がfalseになるまで数フレームの遅延があります。
遅延中の古いコマンドはGPU側のAlive状態と世代番号で拒否されます。

## 統計

`GetLastParticleStepStatistics()`はGPUから非同期回収した値を返します。

| メンバー | 意味 |
|---|---|
| `particleCount` | GPU上の生存粒子数 |
| `candidatePairCount` | 位置Solverが調べた粒子候補ペア数 |
| `retiredParticleCount` | 範囲外または非数で終了した粒子数 |
| `latencyFrames` | 統計値の遅延フレーム数 |
| `isAvailable` | 最初のReadbackが完了したか |

CPU時間ではなくGPU処理になったため、以前の区間別CPUミリ秒は削除されています。
GPU時間が必要な場合はTimestamp Queryをバックエンドへ追加します。

## 制限

- DynamicObjectとの衝突と反力
- 連続衝突判定
- 粒子の回転、角速度、トルク
- Skinned Modelの粒子描画
- Collider用の広域判定
- 衝突イベント
- 同期的な粒子状態取得
- GPU間でのビット単位の決定性
- Async Compute Queue

Collider数に対する判定は現在`O(粒子数 × Collider数)`です。
Colliderが増えた場合はCollider側にもSpatial HashまたはBVHを追加する余地があります。
