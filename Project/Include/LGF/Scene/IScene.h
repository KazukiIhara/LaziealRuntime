#pragma once

#include <memory>

namespace LGF {

	template <class State, class Data>
	class SceneManager;

	/// <summary>
	/// SceneManagerで管理するシーンの基底クラス。
	/// </summary>
	template <class State, class Data>
	class IScene {
	public:
		using StateType = State;
		using DataType = Data;
		using ManagerType = SceneManager<StateType, DataType>;

		struct InitData {
			StateType state;
			std::shared_ptr<DataType> data;
			ManagerType* manager = nullptr;
		};

		explicit IScene(const InitData& init) :
			state_(init.state),
			data_(init.data),
			manager_(init.manager) {
		}

		virtual ~IScene() = default;
		IScene(const IScene&) = delete;
		IScene& operator=(const IScene&) = delete;
		IScene(IScene&&) = delete;
		IScene& operator=(IScene&&) = delete;

		virtual void OnEnter() {}
		virtual void OnExit() {}
		virtual void Update() = 0;
		virtual void Draw() const = 0;

	protected:
		const StateType& GetState() const {
			return state_;
		}

		DataType& GetData() const {
			return *data_;
		}

		bool ChangeScene(const StateType& state);

	private:
		StateType state_;
		std::shared_ptr<DataType> data_;
		ManagerType* manager_ = nullptr;
	};

}
