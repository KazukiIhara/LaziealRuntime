#pragma once

#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>

#include "LGF/Graphics/Transform3D.h"
#include "LGF/Math/MathTypes.h"

namespace LGF::VFX {

	class EffectData;
	class EffectWorldData;

	struct EffectHandle {
		static constexpr uint32_t InvalidIndex = std::numeric_limits<uint32_t>::max();

		uint32_t index = InvalidIndex;
		uint32_t generation{};
		uint64_t worldId{};

		bool IsValid() const;
		bool operator==(const EffectHandle&) const = default;
	};

	struct EffectWorldSettings {
		uint32_t maximumInstanceCount = 8000u;
	};

	/// <summary>EffectWorldへ同期読み込みされたEffekseerエフェクト</summary>
	class Effect {
	public:
		Effect() = default;
		~Effect();

		bool IsValid() const;
		const std::filesystem::path& GetFilePath() const;

	private:
		friend class EffectWorld;
		explicit Effect(std::shared_ptr<EffectData> data);

		std::shared_ptr<EffectData> data_;
	};

	/// <summary>
	/// Effekseerエフェクトの同期読み込み、再生個体、更新を一元管理する。
	/// 描画はメイン3Dカメラを使用し、トーンマッピング前の透明パスで自動実行される。
	/// </summary>
	class EffectWorld {
	public:
		explicit EffectWorld(const EffectWorldSettings& settings = {});
		~EffectWorld();
		EffectWorld(const EffectWorld&) = delete;
		EffectWorld& operator=(const EffectWorld&) = delete;
		EffectWorld(EffectWorld&&) = delete;
		EffectWorld& operator=(EffectWorld&&) = delete;

		/// <summary>相対パスはRuntimeSetting.iniのassetRootを基準に同期読み込みする。</summary>
		Effect Load(const std::filesystem::path& filePath);
		EffectHandle Play(const Effect& effect);
		EffectHandle Play(const Effect& effect, const Transform3D& transform);
		/// <summary>アフィン変換行列を指定して再生する。</summary>
		EffectHandle Play(const Effect& effect, const Matrix4x4& transform);
		bool Stop(EffectHandle handle);
		bool IsPlaying(EffectHandle handle) const;
		/// <summary>再生中エフェクトのアフィン変換を置き換える。</summary>
		bool SetTransform(EffectHandle handle, const Transform3D& transform);
		bool SetTransform(EffectHandle handle, const Matrix4x4& transform);

		/// <summary>秒単位の経過時間で、所属する全エフェクトを同期更新する。</summary>
		bool Update(float deltaTime);
		void Clear();

	private:
		std::unique_ptr<EffectWorldData> data_;
	};

}
