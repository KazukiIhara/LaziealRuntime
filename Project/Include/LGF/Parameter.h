#pragma once

#include "LGF/LGFMath.h"

#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
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

	/// <summary>
	/// Assets/Parameters内のパラメータを簡単に取得するための静的API
	/// </summary>
	class Parameter {
	public:
		template <class T>
		static T Get(
			const std::filesystem::path& fileName,
			std::string_view parameterPath) {
			ValidateType<T>();
			std::scoped_lock lock(GetCacheMutex());

			std::string loadError;
			const ParameterDocument* document = GetOrLoadDocument(fileName, loadError);
			if (!document) {
				throw std::runtime_error(loadError);
			}

			const ParameterData* data = document->Find(parameterPath);
			if (!data) {
				throw std::runtime_error(
					"Parameter not found. File: " + fileName.string() +
					", Path: " + std::string(parameterPath));
			}

			const T* value = std::get_if<T>(&data->value);
			if (!value) {
				throw std::runtime_error(
					"Parameter type mismatch. File: " + fileName.string() +
					", Path: " + std::string(parameterPath) +
					", Expected: " + std::string(GetTypeName<T>()) +
					", Actual: " + std::string(ToString(GetParameterType(data->value))));
			}
			return *value;
		}

		template <class T>
		static T GetOr(
			const std::filesystem::path& fileName,
			std::string_view parameterPath,
			const T& defaultValue) {
			return TryGet<T>(fileName, parameterPath).value_or(defaultValue);
		}

		template <class T>
		static std::optional<T> TryGet(
			const std::filesystem::path& fileName,
			std::string_view parameterPath) {
			ValidateType<T>();
			std::scoped_lock lock(GetCacheMutex());

			std::string loadError;
			const ParameterDocument* document = GetOrLoadDocument(fileName, loadError);
			return document ? document->Get<T>(parameterPath) : std::nullopt;
		}

		static ParameterIoResult Reload(const std::filesystem::path& fileName);
		static void ClearCache();

	private:
		template <class T>
		static constexpr void ValidateType() {
			static_assert(
				std::is_same_v<T, bool> ||
				std::is_same_v<T, int32_t> ||
				std::is_same_v<T, float> ||
				std::is_same_v<T, Vector2> ||
				std::is_same_v<T, Vector3> ||
				std::is_same_v<T, Vector4> ||
				std::is_same_v<T, std::string>,
				"T is not a supported parameter type");
		}

		template <class T>
		static constexpr std::string_view GetTypeName() {
			if constexpr (std::is_same_v<T, bool>) {
				return "Bool";
			} else if constexpr (std::is_same_v<T, int32_t>) {
				return "Int32";
			} else if constexpr (std::is_same_v<T, float>) {
				return "Float";
			} else if constexpr (std::is_same_v<T, Vector2>) {
				return "Vector2";
			} else if constexpr (std::is_same_v<T, Vector3>) {
				return "Vector3";
			} else if constexpr (std::is_same_v<T, Vector4>) {
				return "Vector4";
			} else {
				return "String";
			}
		}

		static std::mutex& GetCacheMutex();
		static const ParameterDocument* GetOrLoadDocument(
			const std::filesystem::path& fileName,
			std::string& errorMessage);
	};

}
