#pragma once

#include "LGF/Graphics/Model.h"
#include "LGF/Graphics/Texture.h"
#include "LGF/Graphics/Transform2D.h"
#include "LGF/Graphics/Transform3D.h"
#include "LGF/ObjectHandle.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace LGF {

	struct ModelRenderer {
		Model model;
	};

	struct SpriteRenderer {
		Texture texture;
		/// <summary>Transform2Dの位置に合わせる画像内の基準点。拡縮と回転の中心になる。</summary>
		Vector2 pivot{ 0.5f, 0.5f };
	};

	/// <summary>1つのレベルに属するオブジェクト。種類ごとにコンポーネントを1つ持てる。</summary>
	class GameObject {
	public:
		GameObject(const GameObject&) = delete;
		GameObject& operator=(const GameObject&) = delete;

		const std::string& GetId() const;
		const std::string& GetName() const;
		void SetName(std::string name);

		bool IsEnabled() const;
		void SetEnabled(bool enabled);

		template <class T>
		T* GetComponent() {
			auto& component = std::get<std::optional<T>>(components_);
			return component ? &*component : nullptr;
		}

		template <class T>
		const T* GetComponent() const {
			const auto& component = std::get<std::optional<T>>(components_);
			return component ? &*component : nullptr;
		}

		template <class T>
		T& SetComponent(T component) {
			return std::get<std::optional<T>>(components_).emplace(std::move(component));
		}

		template <class T>
		void RemoveComponent() {
			std::get<std::optional<T>>(components_).reset();
		}

	private:
		friend class GameObjectWorld;

		GameObject(std::string id, std::string name, bool enabled);

		std::string id_;
		std::string name_;
		bool isEnabled_ = true;
		std::tuple<
			std::optional<Transform3D>,
			std::optional<ModelRenderer>,
			std::optional<Transform2D>,
			std::optional<SpriteRenderer>> components_;
	};

	/// <summary>個体を所有し、ハンドルの解決と描画を行う。</summary>
	class GameObjectWorld {
	public:
		GameObjectWorld() = default;
		GameObjectWorld(GameObjectWorld&&) noexcept = default;
		GameObjectWorld& operator=(GameObjectWorld&&) noexcept = default;

		ObjectHandle Add(std::string id, std::string name = {}, bool enabled = true);
		bool Remove(ObjectHandle handle);
		ObjectHandle Find(std::string_view id) const;
		GameObject* Get(ObjectHandle handle);
		const GameObject* Get(ObjectHandle handle) const;
		std::size_t GetObjectCount() const;
		void Draw() const;
		void Draw2D() const;
		void Draw3D() const;

	private:
		struct Slot {
			std::unique_ptr<GameObject> object;
			uint32_t generation{};
		};

		std::vector<Slot> slots_;
		std::vector<uint32_t> freeSlots_;
		std::map<std::string, ObjectHandle, std::less<>> handles_;
	};

}
