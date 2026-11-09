# ParticleWorld 設計メモ

## 目的

ゲーム中の煙、火花、破片などを扱う、見た目用のパーティクルシステムを実装する。

`LGF::Physics::PhysicsWorld`が扱う`PhysicsParticle`とは用途を分ける。
`PhysicsParticle`は質量や衝突を持つ物理シミュレーション用、今回の`ParticleWorld`は大量のVFXを効率よく更新、描画するためのものとする。

最初はCPUでシミュレーションを実装し、公開APIを変えずにGPUシミュレーションへ移行できる構造を目指す。

Effekseerの組み込みは別機能として後から行う。このParticleWorldはエンジン独自の
軽量なVFXを担当し、Effekseerファイルの解釈や互換性を実装しない。

## CPU初期実装で確定した仕様

公開定義は`Project/Include/LGF/VFX/Particle/ParticleWorld.h`を正とする。
以下の仮APIは設計時の責務説明として残している。

### World設定

| 項目 | 既定値 | 意味 |
| --- | --- | --- |
| worldBounds | 各軸-100～100 | 粒子中心を判定するワールド座標のAABB |
| maximumParticleCount | 10000 | World全体の固定粒子容量 |
| maximumEmitterCount | 256 | Emitter数の上限 |
| boundsPadding | 0 | AABBの各面へ加算する余白 |
| removeOutOfBounds | true | 範囲外粒子の回収を有効にする |
| maximumDeltaTime | 1/15秒 | 1回の更新時間の上限。超過時間は捨てる |
| randomSeed | 1 | World内で使用する乱数シード |

容量不足では新しい粒子を拒否し、次フレームへ持ち越さない。
Emitterはスロット順に生成するため、上限付近での公平な割り当ては行わない。
設定はWorld生成時に確定し、実行中の容量変更は提供しない。

### Emitter設定

- `position`: Point Emitterのワールド座標。
- `minimumVelocity` / `maximumVelocity`: 各軸で独立に一様抽選する初速度。
- `acceleration`: 生成した粒子に適用する一定加速度。
- `minimumLifetime` / `maximumLifetime`: 寿命の一様抽選範囲。単位は秒、0より大きい値。
- `startColor` / `endColor`: 寿命に対して線形補間するRGBA。RGBはHDR値を許容する。
- `startSize` / `endSize`: 寿命に対して線形補間する幅と高さ。ワールド単位、非負値。
- `minimumRotation` / `maximumRotation`: 板の初期回転の抽選範囲。単位はラジアン。
- `minimumAngularVelocity` / `maximumAngularVelocity`: 角速度の抽選範囲。ラジアン/秒。
- `emissionRate`: 1秒あたりの連続生成数。小数の余りを次の更新へ持ち越す。0で連続生成なし。
- `enabled`: 連続生成の有効・無効。明示的な`Emit`には影響しない。
- `texture`: 2Dのカラーテクスチャ。空の場合は単色の板を描画する。

設定の変更は生成済み粒子に影響しない。`SetEmitterDesc`では連続生成の小数余りを
リセットする。`SetEmitterEnabled`では余りを維持する。
`Emit`は要求数をEmitterに蓄積し、次の`Update`時点の設定で生成する。
Emitter削除時は未処理の要求を破棄し、生成済み粒子は寿命まで残す。

### ハンドル・更新・描画

- ハンドルは`index`・`generation`・`worldId`を持つ。Worldのコピーとムーブは不可。
- `Clear`ですべてのハンドルを無効化し、乱数シードも初期化する。
- 負数・非有限値の更新時間は無視する。`Update(0)`は生成要求と範囲判定だけを処理できる。
- 生成後に更新するため、新しい粒子もその更新のdeltaTimeだけ進む。サブフレーム生成は将来対応。
- 初速度と一定加速度から位置を更新し、寿命切れと範囲外を同じフリーリストへ回収する。
- Rendererは4096枚単位のページを必要に応じて追加し、複数Worldの描画を蓄積する。
- 既存のbindless textureを使うため、加算描画ではテクスチャごとにバッチを分割しない。
- SceneColorへのライティング後、トーンマッピング前に描画する。深度判定あり、深度書き込みなし。
- `DrawDebug`はAABBとWorld別統計を表示する。生成・拒否・寿命切れ・範囲外数は直前の更新値。

### JSON設定

`LoadParticleWorldSettings(path)` / `LoadParticleEmitterDesc(path)`で、既存の
`ParameterDocument`形式を読み込む。パスはカレントディレクトリ基準。
省略項目は既定値、型不一致や読み込み失敗は例外。値域はWorld生成・Emitter登録時に検証する。
JSONの整数は既存エディターに合わせて非負のInt32とし、C++側ではuint32_tを使用する。
`Texture`は既存の`Texture`と同じパス解決を使用し、空文字ならテクスチャなし。
既存のParameter Editorから編集できる。自動リロードは行わない。

## 配置と依存関係

- 公開API: `Project/Include/LGF/VFX/Particle/`
- CPUシミュレーション: `Project/Src/VFX/Particle/`
- 描画とGPUリソース: `LaziealGraphicsFramework`

`Collision::AABB`はLaziealRuntimeにあるため、`ParticleWorld`、Emitter、CPUシミュレーションはLaziealRuntimeへ置く。
LaziealGraphicsFramework側はRuntimeへ依存させず、パーティクル描画に必要なデータだけを受け取る。

```text
ゲーム
  -> LaziealRuntime
       ParticleWorld / Emitter / Collision::AABB
  -> LaziealGraphicsFramework
       ParticleRenderer / GPU Buffer / Shader
```

## 基本構成

### ParticleWorld

1つのWorldが、Emitterとそこから生成されたすべてのパーティクルを一元管理する。

主な責務は次のとおり。

- Emitterの作成、削除、設定変更
- Emitterからの生成要求の受付
- パーティクルの生成、更新、削除
- World全体の最大パーティクル数の管理
- 描画用データの構築と描画要求
- 統計情報とデバッグ表示の提供

シーン内に複数の`ParticleWorld`を作成できる設計にする。
環境エフェクト、戦闘エフェクト、UIなど、寿命や一括停止の単位が異なる場合にWorldを分ける。
エフェクトの種類ごとに細かくWorldを増やす使い方は想定しない。

### Emitter

Emitterはパーティクルそのものを所有しない。
生成位置、生成形状、方向、速度、寿命、色などの生成規則だけを持ち、生成要求を`ParticleWorld`へ送る。

連続生成と、爆発などの一括生成の両方を扱えるようにする。

### Particle

個別パーティクルは`ParticleWorld`内部だけで保持する。
公開APIからパーティクルの参照、ポインター、`span`を取得できるようにはしない。

これにより、CPU上の配列をGPUバッファへ移した後も、ゲーム側のAPIを維持できる。

## 公開APIの方針

既存のWorld系クラスに合わせ、世代番号付きハンドルを使用する。

```cpp
namespace LGF::VFX {

	struct ParticleEmitterHandle {
		static constexpr uint32_t InvalidIndex = std::numeric_limits<uint32_t>::max();

		uint32_t index = InvalidIndex;
		uint32_t generation{};

		bool IsValid() const;
		bool operator==(const ParticleEmitterHandle&) const = default;
	};

	struct ParticleWorldSettings {
		Collision::AABB worldBounds{
			.min = { -100.0f, -100.0f, -100.0f },
			.max = { 100.0f, 100.0f, 100.0f },
		};
		uint32_t maximumParticleCount = 10000;
		float boundsPadding{};
		bool removeOutOfBounds = true;
	};

	class ParticleWorld {
	public:
		explicit ParticleWorld(const ParticleWorldSettings& settings = {});

		ParticleEmitterHandle CreateEmitter(const ParticleEmitterDesc& desc);
		void RemoveEmitter(ParticleEmitterHandle handle);
		void SetEmitterPosition(ParticleEmitterHandle handle, const Vector3& position);
		void SetEmitterEnabled(ParticleEmitterHandle handle, bool enabled);
		void Emit(ParticleEmitterHandle handle, uint32_t count);

		void Update(float deltaTime);
		void Draw() const;
		void Clear();
	};

}
```

これは責務を示すための仮APIであり、実装開始時に既存の命名、引数形式、コメント形式へ合わせて確定する。

ハンドルが別のWorldへ誤って渡された場合を検出できるよう、所有Worldの識別方法も用意する。
World識別子をハンドルへ含めるか、各World側で検証できる情報を持たせる。

## 更新処理

CPU版は次の順番で更新する。

1. Emitterへの変更命令を反映する。
2. 連続生成と明示的な`Emit()`を生成コマンドへまとめる。
3. 空きスロットからパーティクルを生成する。
4. 速度、加速度、色、サイズ、回転、寿命を更新する。
5. 寿命切れと範囲外のパーティクルを削除する。
6. 描画単位ごとにデータをまとめる。
7. 統計情報を保存する。

パーティクル配列はスロット、フリーリスト、有効リストで管理する。
削除理由に関係なく同じ削除処理を通し、空きスロットを再利用する。

## World範囲

`PhysicsWorld`と同じように、`ParticleWorldSettings`へ`Collision::AABB worldBounds`を持たせる。

- パーティクル中心が範囲外へ出たら削除する。
- 寿命切れと範囲外削除は同じ回収処理を使用する。
- 大きなパーティクルが境界で急に消える場合は`boundsPadding`で判定範囲を広げる。
- 最初はワールド座標のAABBだけを扱う。
- ローカル座標シミュレーションは必要になってから追加する。

Emitter自体が範囲外にある場合も生成は禁止しない。
生成されたパーティクルを通常どおり範囲判定し、範囲外なら同じフレームで回収する。
Emitter形状とAABBの交差を個別実装せず、処理を単純に保つ。

Worldごとに異なる`worldBounds`を指定できるようにする。
`DrawDebug()`ではAABBと、現在数、生成数、寿命切れ数、範囲外削除数を確認できるようにする。

## 描画

初期実装ではビルボードと加算合成を使用する。テクスチャはbindlessで選択し、
バッファページ単位でまとめて描画する。ブレンドモードや描画方式を増やす際にバッチを分ける。

3Dパーティクルはライティング後、シーンカラーをSwapChainへ合成する前に描画する。
加算合成を最初に対応し、必要に応じて通常のアルファ合成へ広げる。

複数Worldを描画する場合に順序を制御できるよう、ゲーム側で`Draw()`を呼ぶ順序を使用するか、描画レイヤーを設定できる形にする。

## GPU移行を妨げないためのルール

- 個別パーティクルを公開しない。
- ゲーム側からの操作はWorld、Emitter、生成コマンド単位にする。
- 描画データのCPU読み戻しを前提にしない。
- CPU版でも生成要求をコマンドとしてまとめる。
- `maximumParticleCount`を最初から固定上限として扱う。
- 乱数の生成方法をWorld内部へ隠す。

GPU版では、概ね次のリソースを使用する。

- `ParticleState`のPing-Pongバッファ
- 生存インデックスのPing-Pongバッファ
- 空きインデックスバッファ
- Emitter設定バッファ
- 生成コマンドバッファ
- Indirect Draw用引数バッファ

Compute Shaderで更新と生成を行い、生存粒子を次フレーム用バッファへ追加する。
寿命切れ、または`worldBounds`外の粒子は空きインデックスへ戻す。
`worldBounds`の最小、最大座標は定数バッファへ渡し、GPU内で判定を完結させる。

## 最初の実装範囲

まずCPU版の3Dパーティクルを完成させる。

- `ParticleWorldSettings`
- `ParticleEmitterHandle`
- Point Emitter
- 明示的な一括生成
- 一定間隔の連続生成
- 位置、速度、加速度
- 寿命
- 開始色と終了色
- 開始サイズと終了サイズ
- `worldBounds`による削除
- ビルボード描画
- 加算合成
- 統計情報
- AABBのデバッグ描画

初期実装には次の機能を含めない。

- パーティクル同士の衝突
- 物理コライダーとの衝突
- GPUシミュレーション
- ソートが必要な高品質アルファ合成
- ローカル座標シミュレーション
- 複雑なEmitter形状
- エディター

## 実装順

1. 現在の`PhysicsWorld`、`ColliderWorld`、描画クラスを再確認し、命名とファイル構成を合わせる。
2. 公開データ、ハンドル、`ParticleWorld`の最小APIを追加する。
3. Emitterなしで、直接生成した1個のパーティクルをCPU更新する。
4. 寿命切れと`worldBounds`外の回収、フリーリスト再利用を実装する。
5. Point Emitterと生成コマンドを実装する。
6. 最小のビルボード描画を追加する。
7. バッチ、最大数、統計、デバッグ表示を追加する。
8. 複数Worldと無効ハンドルの動作を確認する。
9. CPU版のAPIが固まってからGPUバックエンドへ移行する。

## 確認項目

- 寿命切れでパーティクル数が減る。
- AABBの各面を越えたパーティクルが削除される。
- `boundsPadding`によって境界付近の急な消失を緩和できる。
- 削除済みEmitterの古いハンドルを操作できない。
- 異なるWorldのハンドルを誤使用できない。
- 最大数を超える生成要求でも配列外アクセスしない。
- Worldを`Clear()`した後、すべての古いハンドルが無効になる。
- 1シーンに複数Worldがあっても独立して更新、停止、破棄できる。
- Worldごとに異なるAABBを使用できる。
- Emitterを範囲外へ置いても処理が破綻しない。

## 今後の拡張

- 高品質な通常アルファ合成とソート。
- 連続生成タイミングのサブフレーム精度。
- Sphere / Box / Cone Emitterやカーブによる色・サイズ変化。
- GPUバックエンド。公開APIから個別粒子を参照させない方針を維持する。
- Effekseerは独立したEffectWorld等で対応し、この生成規則に変換しない。
