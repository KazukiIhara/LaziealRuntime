#include "LGF/VFX/EffectWorld.h"

#include <atomic>
#include <stdexcept>
#include <utility>

#include "LGF/Graphics/Effect3D.h"
#include "LGF/Math/Functions/MatrixMath.h"
#include "LGF/Math/Functions/TransformMath.h"

namespace {
	std::atomic_uint64_t g_nextEffectWorldId{ 1u };
	const std::filesystem::path g_emptyFilePath;
}

class LGF::VFX::EffectData {
public:
	EffectData(
		EffectResourceHandle3D handle,
		std::filesystem::path filePath,
		uint64_t worldId) :
		handle(handle),
		filePath(std::move(filePath)),
		worldId(worldId) {
	}

	~EffectData() {
		Effect3D::Remove(handle);
	}

	EffectResourceHandle3D handle{};
	std::filesystem::path filePath;
	uint64_t worldId{};
};

class LGF::VFX::EffectWorldData {
public:
	EffectWorldHandle3D handle{};
	uint64_t worldId{};
};

bool LGF::VFX::EffectHandle::IsValid() const {
	return index != InvalidIndex && generation != 0u && worldId != 0u;
}

LGF::VFX::Effect::Effect(std::shared_ptr<EffectData> data) :
	data_(std::move(data)) {
}

LGF::VFX::Effect::~Effect() = default;

bool LGF::VFX::Effect::IsValid() const {
	return data_ && Effect3D::IsValid(data_->handle);
}

const std::filesystem::path& LGF::VFX::Effect::GetFilePath() const {
	return data_ ? data_->filePath : g_emptyFilePath;
}

LGF::VFX::EffectWorld::EffectWorld(const EffectWorldSettings& settings) :
	data_(std::make_unique<EffectWorldData>()) {
	data_->handle = Effect3D::CreateWorld(settings.maximumInstanceCount);
	if (!data_->handle.IsValid()) {
		throw std::runtime_error("Failed to create Effekseer EffectWorld.");
	}
	data_->worldId = g_nextEffectWorldId.fetch_add(1u, std::memory_order_relaxed);
	if (data_->worldId == 0u) {
		data_->worldId = g_nextEffectWorldId.fetch_add(1u, std::memory_order_relaxed);
	}
}

LGF::VFX::EffectWorld::~EffectWorld() {
	if (data_) {
		Effect3D::RemoveWorld(data_->handle);
	}
}

LGF::VFX::Effect LGF::VFX::EffectWorld::Load(
	const std::filesystem::path& filePath) {
	const EffectResourceHandle3D handle = Effect3D::Load(data_->handle, filePath);
	return handle.IsValid()
		? Effect(std::make_shared<EffectData>(handle, filePath, data_->worldId))
		: Effect{};
}

LGF::VFX::EffectHandle LGF::VFX::EffectWorld::Play(const Effect& effect) {
	return Play(effect, Math::Identity4x4());
}

LGF::VFX::EffectHandle LGF::VFX::EffectWorld::Play(
	const Effect& effect,
	const Transform3D& transform) {
	return Play(
		effect,
		Math::MakeAffineMatrix(
			transform.scale,
			transform.rotation,
			transform.translation));
}

LGF::VFX::EffectHandle LGF::VFX::EffectWorld::Play(
	const Effect& effect,
	const Matrix4x4& transform) {
	if (!effect.data_ || effect.data_->worldId != data_->worldId) {
		return {};
	}
	const EffectInstanceHandle3D handle = Effect3D::Play(
		effect.data_->handle,
		transform);
	return handle.IsValid()
		? EffectHandle{ handle.index, handle.generation, data_->worldId }
		: EffectHandle{};
}

bool LGF::VFX::EffectWorld::Stop(EffectHandle handle) {
	if (!handle.IsValid() || handle.worldId != data_->worldId) {
		return false;
	}
	return Effect3D::Stop({
		handle.index,
		handle.generation,
		data_->handle,
	});
}

bool LGF::VFX::EffectWorld::IsPlaying(EffectHandle handle) const {
	if (!handle.IsValid() || handle.worldId != data_->worldId) {
		return false;
	}
	return Effect3D::IsPlaying({
		handle.index,
		handle.generation,
		data_->handle,
	});
}

bool LGF::VFX::EffectWorld::SetTransform(
	EffectHandle handle,
	const Transform3D& transform) {
	return SetTransform(
		handle,
		Math::MakeAffineMatrix(
			transform.scale,
			transform.rotation,
			transform.translation));
}

bool LGF::VFX::EffectWorld::SetTransform(
	EffectHandle handle,
	const Matrix4x4& transform) {
	if (!handle.IsValid() || handle.worldId != data_->worldId) {
		return false;
	}
	return Effect3D::SetTransform({
		handle.index,
		handle.generation,
		data_->handle,
	}, transform);
}

bool LGF::VFX::EffectWorld::Update(float deltaTime) {
	return Effect3D::Update(data_->handle, deltaTime);
}

void LGF::VFX::EffectWorld::Clear() {
	Effect3D::Clear(data_->handle);
}
