#pragma once

#include "LGF/Scene/IScene.h"

#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace LGF {

	struct EmptySceneData final {};

	template<class State, class Data = EmptySceneData>
	class SceneManager final {
	public:
		using Scene = IScene<State, Data>;
		using InitData = typename Scene::InitData;

		SceneManager() :
			data_(std::make_shared<Data>()) {
		}

		explicit SceneManager(std::shared_ptr<Data> data) :
			data_(std::move(data)) {
		}

		SceneManager(const SceneManager&) = delete;
		SceneManager& operator=(const SceneManager&) = delete;
		SceneManager(SceneManager&&) = delete;
		SceneManager& operator=(SceneManager&&) = delete;

		template<class SceneType>
		SceneManager& Register(const State& state) {
			static_assert(
				std::is_base_of_v<Scene, SceneType>,
				"SceneType must derive from SceneManager::Scene");
			static_assert(
				std::is_constructible_v<SceneType, const InitData&>,
				"SceneType must be constructible from const InitData&");

			const InitData initData{
				.state = state,
				.data = data_,
				.manager = this,
			};
			factories_.insert_or_assign(state, [initData] {
				return std::make_unique<SceneType>(initData);
			});
			if (!firstState_) {
				firstState_ = state;
			}
			return *this;
		}

		bool Change(const State& state) {
			if (!factories_.contains(state)) {
				return false;
			}
			pendingState_ = state;
			return true;
		}

		bool HasScene(const State& state) const {
			return factories_.contains(state);
		}

		const std::optional<State>& GetCurrentState() const {
			return currentState_;
		}

		bool IsChangePending() const {
			return pendingState_.has_value();
		}

		Data& GetData() {
			return *data_;
		}

		const Data& GetData() const {
			return *data_;
		}

		void Update() {
			if (!currentScene_ && !pendingState_ && firstState_) {
				pendingState_ = firstState_;
			}
			ApplyPendingChange();
			if (currentScene_) {
				currentScene_->Update();
			}
		}

		void Draw() const {
			if (currentScene_) {
				currentScene_->Draw();
			}
		}

	private:
		using Factory = std::function<std::unique_ptr<Scene>()>;

		void ApplyPendingChange() {
			if (!pendingState_) {
				return;
			}

			const State nextState = *pendingState_;
			pendingState_.reset();
			const auto iterator = factories_.find(nextState);
			if (iterator == factories_.end()) {
				return;
			}

			std::unique_ptr<Scene> nextScene = iterator->second();
			if (!nextScene) {
				return;
			}

			if (currentScene_) {
				currentScene_->OnExit();
			}
			currentScene_ = std::move(nextScene);
			currentState_ = nextState;
			currentScene_->OnEnter();
		}

		std::unordered_map<State, Factory> factories_;
		std::shared_ptr<Data> data_;
		std::unique_ptr<Scene> currentScene_;
		std::optional<State> firstState_;
		std::optional<State> currentState_;
		std::optional<State> pendingState_;
	};

	template<class State, class Data>
	bool IScene<State, Data>::ChangeScene(const StateType& state) {
		return manager_ && manager_->Change(state);
	}

}
