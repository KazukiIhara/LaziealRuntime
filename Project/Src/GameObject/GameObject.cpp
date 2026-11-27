#include "LGF/GameObject.h"
#include "LGF/Graphics/Sprite.h"

#include <atomic>

using namespace LGF;

namespace {

	// レベルの入れ替えや別のWorldでも、古いハンドルが新しい個体を指さないようにする。
	std::atomic_uint64_t g_nextGeneration{ 1u };

}

GameObject::GameObject(std::string id, std::string name, bool enabled) :
	id_(std::move(id)),
	name_(std::move(name)),
	isEnabled_(enabled) {
}

const std::string& GameObject::GetId() const {
	return id_;
}

const std::string& GameObject::GetName() const {
	return name_;
}

void GameObject::SetName(std::string name) {
	name_ = std::move(name);
}

bool GameObject::IsEnabled() const {
	return isEnabled_;
}

void GameObject::SetEnabled(bool enabled) {
	isEnabled_ = enabled;
}

ObjectHandle GameObjectWorld::Add(std::string id, std::string name, bool enabled) {
	const bool append = freeSlots_.empty();
	if (id.empty() || handles_.contains(id) || (append && slots_.size() >= ObjectHandle::InvalidIndex)) {
		return {};
	}

	const uint64_t generation = g_nextGeneration.fetch_add(1u, std::memory_order_relaxed);
	if (generation > std::numeric_limits<uint32_t>::max()) {
		return {};
	}
	auto object = std::unique_ptr<GameObject>(new GameObject(std::move(id), std::move(name), enabled));
	const ObjectHandle handle{
		.index = append ? static_cast<uint32_t>(slots_.size()) : freeSlots_.back(),
		.generation = static_cast<uint32_t>(generation),
	};
	const auto [entry, inserted] = handles_.emplace(object->GetId(), handle);
	if (!inserted) {
		return {};
	}

	Slot slot{ std::move(object), handle.generation };
	if (append) {
		try {
			slots_.push_back(std::move(slot));
		} catch (...) {
			handles_.erase(entry);
			throw;
		}
	} else {
		slots_[handle.index] = std::move(slot);
		freeSlots_.pop_back();
	}
	return handle;
}

bool GameObjectWorld::Remove(ObjectHandle handle) {
	const GameObject* object = Get(handle);
	if (!object) {
		return false;
	}
	freeSlots_.push_back(handle.index);
	handles_.erase(object->GetId());
	slots_[handle.index].object.reset();
	return true;
}

ObjectHandle GameObjectWorld::Find(std::string_view id) const {
	const auto found = handles_.find(id);
	return found != handles_.end() ? found->second : ObjectHandle{};
}

GameObject* GameObjectWorld::Get(ObjectHandle handle) {
	return const_cast<GameObject*>(std::as_const(*this).Get(handle));
}

const GameObject* GameObjectWorld::Get(ObjectHandle handle) const {
	if (!handle.IsValid() || handle.index >= slots_.size()) {
		return nullptr;
	}
	const Slot& slot = slots_[handle.index];
	return slot.generation == handle.generation ? slot.object.get() : nullptr;
}

std::size_t GameObjectWorld::GetObjectCount() const {
	return handles_.size();
}

void GameObjectWorld::Draw() const {
	Draw3D();
	Draw2D();
}

void GameObjectWorld::Draw3D() const {
	for (const Slot& slot : slots_) {
		if (!slot.object || !slot.object->IsEnabled()) {
			continue;
		}
		const GameObject& object = *slot.object;
		const Transform3D* transform = object.GetComponent<Transform3D>();
		const ModelRenderer* renderer = object.GetComponent<ModelRenderer>();
		if (transform && renderer && renderer->model.IsValid()) {
			const Matrix4x4 matrix = Math::MakeAffineMatrix(
				transform->scale,
				transform->rotation,
				transform->translation);
			renderer->model.Draw(matrix);
		}
	}
}

void GameObjectWorld::Draw2D() const {
	for (const Slot& slot : slots_) {
		if (!slot.object || !slot.object->IsEnabled()) {
			continue;
		}
		const GameObject& object = *slot.object;
		const Transform2D* transform2D = object.GetComponent<Transform2D>();
		const SpriteRenderer* spriteRenderer = object.GetComponent<SpriteRenderer>();
		if (transform2D && spriteRenderer && spriteRenderer->texture.IsValid()) {
			const Vector2 textureSize{
				static_cast<float>(spriteRenderer->texture.GetWidth()),
				static_cast<float>(spriteRenderer->texture.GetHeight()),
			};
			const Vector2 size = textureSize * transform2D->scale;
			Sprite sprite(
				spriteRenderer->texture,
				transform2D->translation - size * spriteRenderer->pivot,
				size);
			sprite.rotation = transform2D->rotation;
			sprite.pivot = spriteRenderer->pivot;
			sprite.Draw();
		}
	}
}
