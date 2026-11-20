#pragma once

#include "LGF/Scene/IScene.h"
#include "LGF/Scene/SceneTransition.h"
#include "LGF/Graphics/Graphics2D.h"
#include "LGF/System.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace LGF {

	struct EmptySceneData {};

	/// <summary>
	/// 状態をキーにシーンの生成と切り替えを管理する。
	/// </summary>
	template <class State, class Data = EmptySceneData>
	class SceneManager {
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

		template <class SceneType>
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
			if (activeTransition_) {
				return false;
			}
			if (!factories_.contains(state)) {
				return false;
			}
			pendingState_ = state;
			pendingTransition_.reset();
			return true;
		}

		bool Change(const State& state, const SceneTransition& transition) {
			if (activeTransition_ || !IsValid(transition)) {
				return false;
			}
			if (!factories_.contains(state)) {
				return false;
			}
			pendingState_ = state;
			pendingTransition_ = transition;
			return true;
		}

		bool Change(const State& state, std::string_view transitionName) {
			const auto iterator = transitions_.find(std::string(transitionName));
			return iterator != transitions_.end() && Change(state, iterator->second);
		}

		bool RegisterTransition(std::string_view name, const SceneTransition& transition) {
			if (name.empty() || !IsValid(transition)) {
				return false;
			}
			transitions_.insert_or_assign(std::string(name), transition);
			return true;
		}

		bool HasScene(const State& state) const {
			return factories_.contains(state);
		}

		bool HasTransition(std::string_view name) const {
			return transitions_.contains(std::string(name));
		}

		const std::optional<State>& GetCurrentState() const {
			return currentState_;
		}

		bool IsChangePending() const {
			return pendingState_.has_value();
		}

		bool IsTransitioning() const {
			return activeTransition_.has_value();
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
			BeginPendingChange();
			AdvanceTransition(0.0f);
			if (currentScene_) {
				currentScene_->Update();
			}
			AdvanceTransition(static_cast<float>(System::DeltaTime()));
		}

		void Draw() const {
			if (currentScene_) {
				currentScene_->Draw();
			}
			DrawTransition();
		}

	private:
		using Factory = std::function<std::unique_ptr<Scene>()>;
		enum class TransitionPhase {
			FadeOut,
			FadeIn,
		};

		struct ActiveTransition {
			SceneTransition desc;
			TransitionPhase phase = TransitionPhase::FadeOut;
			float elapsedTime = 0.0f;
		};

		static bool IsValid(const SceneTransition& transition) {
			return std::isfinite(transition.fadeOutDuration) &&
				std::isfinite(transition.fadeInDuration) &&
				transition.fadeOutDuration >= 0.0f &&
				transition.fadeInDuration >= 0.0f &&
				!transition.pipeline.empty();
		}

		void BeginPendingChange() {
			if (!pendingState_ || activeTransition_) {
				return;
			}
			if (currentScene_ && pendingTransition_) {
				activeTransition_ = ActiveTransition{
					.desc = std::move(*pendingTransition_),
				};
				pendingTransition_.reset();
				return;
			}
			ApplyPendingChange();
		}

		bool ApplyPendingChange() {
			if (!pendingState_) {
				return false;
			}

			const State nextState = *pendingState_;
			pendingState_.reset();
			pendingTransition_.reset();
			const auto iterator = factories_.find(nextState);
			if (iterator == factories_.end()) {
				return false;
			}

			std::unique_ptr<Scene> nextScene = iterator->second();
			if (!nextScene) {
				return false;
			}

			if (currentScene_) {
				currentScene_->OnExit();
			}
			currentScene_ = std::move(nextScene);
			currentState_ = nextState;
			currentScene_->OnEnter();
			return true;
		}

		void AdvanceTransition(float deltaTime) {
			if (!activeTransition_) {
				return;
			}

			const float elapsedTime = std::isfinite(deltaTime)
				? std::max(deltaTime, 0.0f)
				: 0.0f;
			activeTransition_->elapsedTime += elapsedTime;
			while (activeTransition_) {
				const float duration = activeTransition_->phase == TransitionPhase::FadeOut
					? activeTransition_->desc.fadeOutDuration
					: activeTransition_->desc.fadeInDuration;
				if (activeTransition_->elapsedTime < duration) {
					return;
				}

				activeTransition_->elapsedTime -= duration;
				if (activeTransition_->phase == TransitionPhase::FadeOut) {
					ApplyPendingChange();
					activeTransition_->phase = TransitionPhase::FadeIn;
				} else {
					activeTransition_.reset();
				}
			}
		}

		void DrawTransition() const {
			if (!activeTransition_) {
				return;
			}

			const ActiveTransition& active = *activeTransition_;
			const bool isFadeOut = active.phase == TransitionPhase::FadeOut;
			const float phaseDuration = isFadeOut
				? active.desc.fadeOutDuration
				: active.desc.fadeInDuration;
			const float phaseProgress = phaseDuration > 0.0f
				? std::clamp(active.elapsedTime / phaseDuration, 0.0f, 1.0f)
				: 1.0f;
			const float totalDuration = active.desc.fadeOutDuration + active.desc.fadeInDuration;
			const float transitionTime = isFadeOut
				? active.elapsedTime
				: active.desc.fadeOutDuration + active.elapsedTime;
			const float progress = totalDuration > 0.0f
				? std::clamp(transitionTime / totalDuration, 0.0f, 1.0f)
				: 1.0f;
			const float easedProgress = Easing::Apply(
				isFadeOut ? active.desc.fadeOutEasing : active.desc.fadeInEasing,
				phaseProgress);
			const float opacity = isFadeOut ? easedProgress : 1.0f - easedProgress;

			SceneTransitionDrawDesc drawDesc{
				.pipeline = active.desc.pipeline,
				.color = active.desc.color,
				.parameters = active.desc.parameters,
				.progress = progress,
				.phaseProgress = easedProgress,
				.opacity = opacity,
				.phase = isFadeOut ? 0.0f : 1.0f,
			};
			if (!Graphics2D::DrawSceneTransition(drawDesc) &&
				drawDesc.pipeline != "SceneTransitionFade") {
				drawDesc.pipeline = "SceneTransitionFade";
				Graphics2D::DrawSceneTransition(drawDesc);
			}
		}

		std::unordered_map<State, Factory> factories_;
		std::unordered_map<std::string, SceneTransition> transitions_;
		std::shared_ptr<Data> data_;
		std::unique_ptr<Scene> currentScene_;
		std::optional<State> firstState_;
		std::optional<State> currentState_;
		std::optional<State> pendingState_;
		std::optional<SceneTransition> pendingTransition_;
		std::optional<ActiveTransition> activeTransition_;
	};

	template <class State, class Data>
	bool IScene<State, Data>::ChangeScene(const StateType& state) {
		return manager_ && manager_->Change(state);
	}

	template <class State, class Data>
	bool IScene<State, Data>::ChangeScene(const StateType& state, const SceneTransition& transition) {
		return manager_ && manager_->Change(state, transition);
	}

	template <class State, class Data>
	bool IScene<State, Data>::ChangeScene(const StateType& state, std::string_view transitionName) {
		return manager_ && manager_->Change(state, transitionName);
	}

}
