#pragma once

#include "LGF/LGFMath.h"

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace LGF {

	/// <summary>
	/// パラメータの型
	/// </summary>
	enum class ParameterType : uint8_t {
		Bool,
		Int32,
		Float,
		Vector2,
		Vector3,
		Vector4,
		String,
	};

	/// <summary>
	/// パラメータの値
	/// </summary>
	using ParameterValue = std::variant<
		bool,
		int32_t,
		float,
		Vector2,
		Vector3,
		Vector4,
		std::string>;

	/// <summary>
	/// パラメータ編集用の補助情報
	/// </summary>
	struct ParameterEditorSettings {
		std::optional<float> minimum;
		std::optional<float> maximum;
		std::optional<float> step;
		std::string label;
		std::string tooltip;
	};

	/// <summary>
	/// パラメータデータ
	/// </summary>
	struct ParameterData {
		ParameterValue value{};
		ParameterEditorSettings editor{};
	};

	/// <summary>
	/// 階層化されたパラメータノード
	/// </summary>
	struct ParameterNode {
		std::map<std::string, ParameterNode> children;
		std::optional<ParameterData> data;
	};

	/// <summary>
	/// パラメータファイルの入出力結果
	/// </summary>
	struct ParameterIoResult {
		bool succeeded = false;
		std::string message;

		explicit operator bool() const {
			return succeeded;
		}
	};

	/// <summary>
	/// 型付きの階層パラメータを管理するドキュメント
	/// </summary>
	class ParameterDocument {
	public:
		ParameterIoResult Load(const std::filesystem::path& filePath);
		ParameterIoResult Save(const std::filesystem::path& filePath) const;

		void Clear();
		bool IsEmpty() const;

		bool Contains(std::string_view path) const;
		bool Set(
			std::string_view path,
			const ParameterValue& value,
			const ParameterEditorSettings& editor = {});
		bool Remove(std::string_view path);

		ParameterData* Find(std::string_view path);
		const ParameterData* Find(std::string_view path) const;

		ParameterNode& GetRoot();
		const ParameterNode& GetRoot() const;

		template <class T>
		std::optional<T> Get(std::string_view path) const {
			const ParameterData* data = Find(path);
			if (!data) {
				return std::nullopt;
			}
			const T* value = std::get_if<T>(&data->value);
			return value ? std::optional<T>{ *value } : std::nullopt;
		}

		template <class T>
		T GetOr(std::string_view path, const T& defaultValue) const {
			const std::optional<T> value = Get<T>(path);
			return value.value_or(defaultValue);
		}

	private:
		ParameterNode root_;
	};

	ParameterType GetParameterType(const ParameterValue& value);
	std::string_view ToString(ParameterType type);

}
