#pragma once

#include "LGF/Collision/Shapes3D.h"
#include "LGF/Graphics/Texture.h"
#include "LGF/Graphics/GpuParticle3D.h"
#include "LGF/Math/MathTypes.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <random>
#include <vector>

namespace LGF::VFX {

	// 所有Worldと世代番号でEmitterを識別するハンドル
	struct ParticleEmitterHandle {
		static constexpr uint32_t InvalidIndex = std::numeric_limits<uint32_t>::max();

		uint32_t index = InvalidIndex;
		uint32_t generation{};
		uint64_t worldId{};

		bool IsValid() const;
		bool operator==(const ParticleEmitterHandle&) const = default;
	};

	// CPUシミュレーションの上限と範囲。時間は秒、長さはワールド単位。
	struct ParticleWorldSettings {
		Collision::AABB worldBounds{
			.min = { -100.0f, -100.0f, -100.0f },
			.max = { 100.0f, 100.0f, 100.0f },
		};
		uint32_t maximumParticleCount = 10000u;
		uint32_t maximumEmitterCount = 256u;
		float boundsPadding{};
		bool removeOutOfBounds = true;
		float maximumDeltaTime = 1.0f / 15.0f;
		uint32_t randomSeed = 1u;
	};

	// Point Emitterの生成規則。設定変更は生成済みの粒子へ影響しない。
	struct ParticleEmitterDesc {
		Vector3 position{};
		Vector3 minimumVelocity{ -1.0f, 1.0f, -1.0f };
		Vector3 maximumVelocity{ 1.0f, 3.0f, 1.0f };
		Vector3 acceleration{ 0.0f, -1.0f, 0.0f };
		float minimumLifetime = 0.5f;
		float maximumLifetime = 1.0f;
		Vector4 startColor{ 1.0f, 1.0f, 1.0f, 1.0f };
		Vector4 endColor{ 1.0f, 1.0f, 1.0f, 0.0f };
		Vector2 startSize{ 0.1f, 0.1f };
		Vector2 endSize{};
		float minimumRotation{};
		float maximumRotation{};
		float minimumAngularVelocity{};
		float maximumAngularVelocity{};
		// 0は連続生成なし。enabledは連続生成だけを制御し、Emitは常に受け付ける。
		float emissionRate = 30.0f;
		bool enabled = true;
		// 空の場合は単色の四角形。初期版のブレンドは加算に固定する。
		Texture texture{};
	};

	struct ParticleStepStatistics {
		uint32_t particleCount{};
		uint32_t emitterCount{};
		uint64_t generatedCount{};
		uint64_t rejectedCount{};
		uint32_t expiredCount{};
		uint32_t outOfBoundsCount{};
		// GPU版では完了済みFrameContextから取得した値の遅延フレーム数。
		uint32_t latencyFrames{};
		bool isAvailable = true;
	};

	// ParameterDocument形式のJSONを読む。省略項目は既定値、型の不一致は例外とする。
	ParticleWorldSettings LoadParticleWorldSettings(const std::filesystem::path& filePath);
	ParticleEmitterDesc LoadParticleEmitterDesc(const std::filesystem::path& filePath);

	// 見た目用の粒子を一元管理する。個別粒子は公開せず、生成要求を更新時に処理する。
	class ParticleWorld {
	private:
		struct EmitterSlot {
			ParticleEmitterDesc desc{};
			double emissionRemainder{};
			uint64_t pendingCount{};
			uint32_t generation = 1u;
			bool isOccupied{};
		};

		struct ParticleSlot {
			Vector3 position{};
			Vector3 velocity{};
			Vector3 acceleration{};
			Vector4 startColor{};
			Vector4 endColor{};
			Vector2 startSize{};
			Vector2 endSize{};
			float rotation{};
			float angularVelocity{};
			float age{};
			float lifetime{};
			// Texture本体を粒子ごとに複製せず、World内の共有テーブルを参照する。
			uint32_t textureIndex = ParticleEmitterHandle::InvalidIndex;
		};

	public:
		explicit ParticleWorld(const ParticleWorldSettings& settings = {});
		ParticleWorld(const ParticleWorld&) = delete;
		ParticleWorld& operator=(const ParticleWorld&) = delete;
		ParticleWorld(ParticleWorld&&) = delete;
		ParticleWorld& operator=(ParticleWorld&&) = delete;

		ParticleEmitterHandle CreateEmitter(const ParticleEmitterDesc& desc = {});
		// 生成済みの粒子は残し、未処理の生成要求は破棄する。
		bool RemoveEmitter(ParticleEmitterHandle handle);
		bool IsValid(ParticleEmitterHandle handle) const;
		bool SetEmitterDesc(ParticleEmitterHandle handle, const ParticleEmitterDesc& desc);
		bool SetEmitterPosition(ParticleEmitterHandle handle, const Vector3& position);
		bool SetEmitterEnabled(ParticleEmitterHandle handle, bool enabled);
		std::optional<ParticleEmitterDesc> GetEmitterDesc(ParticleEmitterHandle handle) const;
		bool Emit(ParticleEmitterHandle handle, uint32_t count);

		void Update(float deltaTime);
		void Draw() const;
		void DrawDebug() const;
		// 全ハンドルを無効化し、乱数を初期シードに戻す。
		void Clear();
		std::size_t GetParticleCount() const;
		std::size_t GetEmitterCount() const;
		const ParticleWorldSettings& GetSettings() const;
		const ParticleStepStatistics& GetLastStepStatistics() const;

	private:
		EmitterSlot* FindEmitter(ParticleEmitterHandle handle);
		const EmitterSlot* FindEmitter(ParticleEmitterHandle handle) const;
		void GenerateParticles(const ParticleEmitterDesc& desc, uint64_t count);
		void RemoveParticleAt(std::size_t activeIndex);
		uint32_t FindOrCreateTextureIndex(const Texture& texture);
		float Random(float minimum, float maximum);

	private:
		ParticleWorldSettings settings_;
		uint64_t worldId_{};
		std::mt19937 random_;
		std::vector<EmitterSlot> emitters_;
		std::vector<uint32_t> freeEmitterIndices_;
		std::vector<ParticleSlot> particles_;
		std::vector<uint32_t> freeParticleIndices_;
		std::vector<uint32_t> activeParticleIndices_;
		std::vector<Texture> particleTextures_;
		uint32_t emitterCount_{};
		ParticleStepStatistics lastStepStatistics_{};
	};

	// Compute Shaderで生成・更新・破棄を行うGPU Particleの試作World。
	// Emitter操作はCPU版と揃え、統計だけはGPUを待たずに遅延スナップショットを返す。
	class GpuParticleWorld {
	private:
		struct EmitterSlot {
			ParticleEmitterDesc desc{};
			double emissionRemainder{};
			uint64_t pendingCount{};
			uint32_t generation = 1u;
			bool isOccupied{};
		};

	public:
		explicit GpuParticleWorld(const ParticleWorldSettings& settings = {});
		~GpuParticleWorld();
		GpuParticleWorld(const GpuParticleWorld&) = delete;
		GpuParticleWorld& operator=(const GpuParticleWorld&) = delete;
		GpuParticleWorld(GpuParticleWorld&&) = delete;
		GpuParticleWorld& operator=(GpuParticleWorld&&) = delete;

		ParticleEmitterHandle CreateEmitter(const ParticleEmitterDesc& desc = {});
		bool RemoveEmitter(ParticleEmitterHandle handle);
		bool IsValid(ParticleEmitterHandle handle) const;
		bool SetEmitterDesc(ParticleEmitterHandle handle, const ParticleEmitterDesc& desc);
		bool SetEmitterPosition(ParticleEmitterHandle handle, const Vector3& position);
		bool SetEmitterEnabled(ParticleEmitterHandle handle, bool enabled);
		std::optional<ParticleEmitterDesc> GetEmitterDesc(ParticleEmitterHandle handle) const;
		bool Emit(ParticleEmitterHandle handle, uint32_t count);

		void Update(float deltaTime);
		void Draw() const;
		void DrawDebug() const;
		void Clear();
		std::size_t GetParticleCount() const;
		std::size_t GetEmitterCount() const;
		const ParticleWorldSettings& GetSettings() const;
		const ParticleStepStatistics& GetLatestStatistics() const;

	private:
		EmitterSlot* FindEmitter(ParticleEmitterHandle handle);
		const EmitterSlot* FindEmitter(ParticleEmitterHandle handle) const;

	private:
		ParticleWorldSettings settings_;
		uint64_t worldId_{};
		GpuParticleWorldHandle graphicsHandle_{};
		std::mt19937 random_;
		std::vector<EmitterSlot> emitters_;
		std::vector<uint32_t> freeEmitterIndices_;
		uint32_t emitterCount_{};
		ParticleStepStatistics latestStatistics_{ .isAvailable = false };
	};

}
