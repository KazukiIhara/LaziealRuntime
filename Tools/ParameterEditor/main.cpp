#include <Windows.h>
#include <ShlObj.h>
#include <shobjidl.h>

#include <LGF/LGF.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <imgui.h>
#include <nlohmann/json.hpp>

using namespace LGF;

namespace {
	using Json = nlohmann::json;

	constexpr std::string_view kParameterDirectoryKey = "parameterDirectory";

	std::filesystem::path GetPreferencesFilePath() {
		PWSTR localAppDataPath = nullptr;
		if (FAILED(SHGetKnownFolderPath(
			FOLDERID_LocalAppData,
			KF_FLAG_CREATE,
			nullptr,
			&localAppDataPath))) {
			return {};
		}

		const std::filesystem::path preferencesFile =
			std::filesystem::path(localAppDataPath) /
			"Lazieal" /
			"ParameterEditor" /
			"settings.json";
		CoTaskMemFree(localAppDataPath);
		return preferencesFile;
	}

	std::optional<std::filesystem::path> LoadParameterDirectory() {
		const std::filesystem::path preferencesFile = GetPreferencesFilePath();
		if (preferencesFile.empty()) {
			return std::nullopt;
		}

		try {
			std::ifstream file(preferencesFile);
			if (!file) {
				return std::nullopt;
			}

			Json preferences;
			file >> preferences;
			if (!preferences.contains(kParameterDirectoryKey)) {
				return std::nullopt;
			}

			const std::filesystem::path directory =
				preferences.at(kParameterDirectoryKey).get<std::string>();
			std::error_code error;
			if (!std::filesystem::is_directory(directory, error)) {
				return std::nullopt;
			}
			return directory;
		} catch (...) {
			return std::nullopt;
		}
	}

	bool SaveParameterDirectory(const std::filesystem::path& directory) {
		const std::filesystem::path preferencesFile = GetPreferencesFilePath();
		if (preferencesFile.empty()) {
			return false;
		}

		std::error_code error;
		std::filesystem::create_directories(preferencesFile.parent_path(), error);
		if (error) {
			return false;
		}

		std::ofstream file(preferencesFile);
		if (!file) {
			return false;
		}

		const Json preferences{
			{ kParameterDirectoryKey, directory.string() }
		};
		file << preferences.dump(4) << '\n';
		return file.good();
	}

	std::optional<std::filesystem::path> SelectParameterDirectory(
		HWND owner,
		const std::filesystem::path& initialDirectory = {}) {
		IFileDialog* dialog = nullptr;
		if (FAILED(CoCreateInstance(
			CLSID_FileOpenDialog,
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&dialog)))) {
			return std::nullopt;
		}

		DWORD options = 0;
		HRESULT result = dialog->GetOptions(&options);
		if (SUCCEEDED(result)) {
			result = dialog->SetOptions(
				options |
				FOS_PICKFOLDERS |
				FOS_FORCEFILESYSTEM |
				FOS_PATHMUSTEXIST);
		}
		if (SUCCEEDED(result)) {
			result = dialog->SetTitle(L"Select parameter directory");
		}

		IShellItem* initialFolder = nullptr;
		if (SUCCEEDED(result) && !initialDirectory.empty()) {
			if (SUCCEEDED(SHCreateItemFromParsingName(
				initialDirectory.c_str(),
				nullptr,
				IID_PPV_ARGS(&initialFolder)))) {
				dialog->SetFolder(initialFolder);
				initialFolder->Release();
			}
		}

		if (SUCCEEDED(result)) {
			result = dialog->Show(owner);
		}

		IShellItem* selectedItem = nullptr;
		if (SUCCEEDED(result)) {
			result = dialog->GetResult(&selectedItem);
		}

		PWSTR selectedPath = nullptr;
		if (SUCCEEDED(result)) {
			result = selectedItem->GetDisplayName(SIGDN_FILESYSPATH, &selectedPath);
		}

		std::optional<std::filesystem::path> selectedDirectory;
		if (SUCCEEDED(result)) {
			selectedDirectory = std::filesystem::path(selectedPath);
		}

		CoTaskMemFree(selectedPath);
		if (selectedItem) {
			selectedItem->Release();
		}
		dialog->Release();
		return selectedDirectory;
	}

	class ParameterEditorApp {
	public:
		explicit ParameterEditorApp(std::filesystem::path parameterDirectory)
			: parameterDirectory_(std::move(parameterDirectory)) {
			RefreshFiles();
			if (!files_.empty()) {
				Open(files_.front());
			}
		}

		void Draw() {
			const ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->WorkPos);
			ImGui::SetNextWindowSize(viewport->WorkSize);

			constexpr ImGuiWindowFlags windowFlags =
				ImGuiWindowFlags_NoDecoration |
				ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoSavedSettings;
			if (ImGui::Begin("Lazieal Parameter Editor", nullptr, windowFlags)) {
				DrawToolbar();
				ImGui::Separator();
				DrawFileList();
				ImGui::SameLine();
				DrawDocument();
			}
			ImGui::End();
		}

	private:
		static ParameterValue MakeDefaultValue(ParameterType type) {
			switch (type) {
			case ParameterType::Bool: return false;
			case ParameterType::Int32: return int32_t{ 0 };
			case ParameterType::Float: return 0.0f;
			case ParameterType::Vector2: return Vector2{};
			case ParameterType::Vector3: return Vector3{};
			case ParameterType::Vector4: return Vector4{};
			case ParameterType::String: return std::string{};
			default: return false;
			}
		}

		void SetStatus(std::string message, bool isError = false) {
			status_ = std::move(message);
			isStatusError_ = isError;
		}

		void RefreshFiles() {
			files_.clear();
			std::error_code error;
			if (!std::filesystem::exists(parameterDirectory_, error)) {
				SetStatus("The parameter directory does not exist yet.");
				return;
			}

			for (const std::filesystem::directory_entry& entry :
				std::filesystem::directory_iterator(parameterDirectory_, error)) {
				if (error) {
					SetStatus("Failed to enumerate the parameter directory.", true);
					return;
				}
				if (entry.is_regular_file() && entry.path().extension() == ".json") {
					files_.push_back(entry.path());
				}
			}
			std::ranges::sort(files_);
			SetStatus("Parameter file list refreshed.");
		}

		void Open(const std::filesystem::path& filePath) {
			if (isDirty_) {
				SetStatus("Save or reload the current file before opening another file.", true);
				return;
			}

			const ParameterIoResult result = document_.Load(filePath);
			if (!result) {
				SetStatus(result.message, true);
				return;
			}
			currentFile_ = filePath;
			selectedParameter_.clear();
			SetStatus("Loaded " + filePath.filename().string() + '.');
		}

		void Save() {
			if (currentFile_.empty()) {
				SetStatus("Create or open a parameter file first.", true);
				return;
			}

			const ParameterIoResult result = document_.Save(currentFile_);
			if (!result) {
				SetStatus(result.message, true);
				return;
			}
			isDirty_ = false;
			SetStatus("Saved " + currentFile_.filename().string() + '.');
			RefreshFiles();
		}

		void Reload() {
			if (currentFile_.empty()) {
				return;
			}
			isDirty_ = false;
			Open(currentFile_);
		}

		void CreateFile() {
			if (isDirty_) {
				SetStatus("Save or reload the current file before creating another file.", true);
				return;
			}

			std::filesystem::path name(newFileName_.data());
			if (name.empty() || name.filename() != name || name.has_parent_path()) {
				SetStatus("Enter a file name without a directory.", true);
				return;
			}
			if (name.extension().empty()) {
				name += ".json";
			}
			if (name.extension() != ".json") {
				SetStatus("The parameter file extension must be .json.", true);
				return;
			}

			document_.Clear();
			currentFile_ = parameterDirectory_ / name;
			selectedParameter_.clear();
			isDirty_ = true;
			SetStatus("New parameter file created. Add a parameter and save it.");
		}

		void DrawToolbar() {
			constexpr float buttonWidth = 80.0f;
			constexpr float changeButtonWidth = 100.0f;
			constexpr ImGuiTableFlags tableFlags =
				ImGuiTableFlags_SizingStretchProp |
				ImGuiTableFlags_NoPadOuterX;
			if (ImGui::BeginTable("Toolbar", 5, tableFlags)) {
				ImGui::TableSetupColumn("Directory", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Change", ImGuiTableColumnFlags_WidthFixed, changeButtonWidth);
				ImGui::TableSetupColumn("Refresh", ImGuiTableColumnFlags_WidthFixed, buttonWidth);
				ImGui::TableSetupColumn("Save", ImGuiTableColumnFlags_WidthFixed, buttonWidth);
				ImGui::TableSetupColumn("Reload", ImGuiTableColumnFlags_WidthFixed, buttonWidth);
				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted(parameterDirectory_.string().c_str());

				ImGui::TableSetColumnIndex(1);
				if (ImGui::Button("Change...", ImVec2(-1.0f, 0.0f))) {
					ChangeParameterDirectory();
				}

				ImGui::TableSetColumnIndex(2);
				if (ImGui::Button("Refresh", ImVec2(-1.0f, 0.0f))) {
					RefreshFiles();
				}

				ImGui::TableSetColumnIndex(3);
				if (ImGui::Button("Save", ImVec2(-1.0f, 0.0f))) {
					Save();
				}

				ImGui::TableSetColumnIndex(4);
				if (ImGui::Button("Reload", ImVec2(-1.0f, 0.0f))) {
					Reload();
				}
				ImGui::EndTable();
			}

			const ImVec4 statusColor = isStatusError_
				? ImVec4(1.0f, 0.35f, 0.30f, 1.0f)
				: ImVec4(0.50f, 0.85f, 0.55f, 1.0f);
			ImGui::TextColored(statusColor, "%s", status_.c_str());
		}

		void ChangeParameterDirectory() {
			if (isDirty_) {
				SetStatus(
					"Save or reload the current file before changing the parameter directory.",
					true);
				return;
			}

			const std::optional<std::filesystem::path> selectedDirectory =
				SelectParameterDirectory(GetActiveWindow(), parameterDirectory_);
			if (!selectedDirectory) {
				return;
			}

			parameterDirectory_ = *selectedDirectory;
			document_.Clear();
			currentFile_.clear();
			selectedParameter_.clear();
			RefreshFiles();

			if (!SaveParameterDirectory(parameterDirectory_)) {
				SetStatus(
					"The directory was changed, but the preference could not be saved.",
					true);
			}
		}

		void DrawFileList() {
			ImGui::BeginChild("Files", ImVec2(230.0f, 0.0f), ImGuiChildFlags_Borders);
			ImGui::SeparatorText("Parameter files");
			for (const std::filesystem::path& file : files_) {
				const bool selected = file == currentFile_;
				if (ImGui::Selectable(file.filename().string().c_str(), selected)) {
					Open(file);
				}
			}

			ImGui::SeparatorText("New file");
			ImGui::InputText("##NewFile", newFileName_.data(), newFileName_.size());
			if (ImGui::Button("Create", ImVec2(-1.0f, 0.0f))) {
				CreateFile();
			}
			ImGui::EndChild();
		}

		void DrawDocument() {
			ImGui::BeginChild("Document", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
			if (currentFile_.empty()) {
				ImGui::TextDisabled("Open or create a parameter file.");
				ImGui::EndChild();
				return;
			}

			ImGui::Text("%s%s", currentFile_.string().c_str(), isDirty_ ? " *" : "");
			DrawAddParameter();
			ImGui::Separator();

			const float treeWidth = std::max(240.0f, ImGui::GetContentRegionAvail().x * 0.38f);
			ImGui::BeginChild("ParameterTree", ImVec2(treeWidth, 0.0f), ImGuiChildFlags_Borders);
			ImGui::SeparatorText("Parameters");
			DrawNode(document_.GetRoot(), "");
			ImGui::EndChild();
			ImGui::SameLine();
			ImGui::BeginChild("ParameterValue", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
			DrawSelectedParameter();
			ImGui::EndChild();
			ImGui::EndChild();
		}

		void DrawAddParameter() {
			ImGui::SetNextItemWidth(-300.0f);
			ImGui::InputTextWithHint(
				"##ParameterPath",
				"Group/ParameterName",
				newParameterPath_.data(),
				newParameterPath_.size());
			ImGui::SameLine();

			constexpr std::array<const char*, 7> typeNames{
				"Bool", "Int32", "Float", "Vec2", "Vec3", "Vec4", "String"
			};
			ImGui::SetNextItemWidth(130.0f);
			ImGui::Combo(
				"##ParameterType",
				&newParameterType_,
				typeNames.data(),
				static_cast<int>(typeNames.size()));
			ImGui::SameLine();
			if (ImGui::Button("Add parameter", ImVec2(150.0f, 0.0f))) {
				const ParameterType type = static_cast<ParameterType>(newParameterType_);
				if (document_.Set(newParameterPath_.data(), MakeDefaultValue(type))) {
					selectedParameter_ = newParameterPath_.data();
					isDirty_ = true;
					SetStatus("Parameter added.");
				} else {
					SetStatus("The parameter path is empty or conflicts with an existing group.", true);
				}
			}
		}

		void DrawNode(const ParameterNode& node, const std::string& parentPath) {
			for (const auto& [name, child] : node.children) {
				const std::string path = parentPath.empty() ? name : parentPath + '/' + name;
				if (child.data) {
					const bool selected = path == selectedParameter_;
					if (ImGui::Selectable(name.c_str(), selected)) {
						selectedParameter_ = path;
					}
				} else if (ImGui::TreeNode(name.c_str())) {
					DrawNode(child, path);
					ImGui::TreePop();
				}
			}
		}

		bool DrawValue(ParameterValue& value, const ParameterEditorSettings& editor) {
			const bool hasRange =
				editor.minimum && editor.maximum && *editor.minimum <= *editor.maximum;
			const float minimum = hasRange ? *editor.minimum : 0.0f;
			const float maximum = hasRange ? *editor.maximum : 0.0f;
			return std::visit([&](auto& current) {
				using T = std::decay_t<decltype(current)>;
				if constexpr (std::is_same_v<T, bool>) {
					return ImGui::Checkbox("Value", &current);
				} else if constexpr (std::is_same_v<T, int32_t>) {
					int valueAsInt = current;
					const bool changed = ImGui::DragInt(
						"Value",
						&valueAsInt,
						editor.step.value_or(1.0f),
						static_cast<int>(minimum),
						static_cast<int>(maximum));
					current = static_cast<int32_t>(valueAsInt);
					return changed;
				} else if constexpr (std::is_same_v<T, float>) {
					return ImGui::DragFloat(
						"Value", &current, editor.step.value_or(0.01f), minimum, maximum);
				} else if constexpr (std::is_same_v<T, Vector2>) {
					return ImGui::DragFloat2(
						"Value", &current.x, editor.step.value_or(0.01f), minimum, maximum);
				} else if constexpr (std::is_same_v<T, Vector3>) {
					return ImGui::DragFloat3(
						"Value", &current.x, editor.step.value_or(0.01f), minimum, maximum);
				} else if constexpr (std::is_same_v<T, Vector4>) {
					return ImGui::DragFloat4(
						"Value", &current.x, editor.step.value_or(0.01f), minimum, maximum);
				} else {
					std::array<char, 1024> buffer{};
					std::snprintf(buffer.data(), buffer.size(), "%s", current.c_str());
					if (ImGui::InputText("Value", buffer.data(), buffer.size())) {
						current = buffer.data();
						return true;
					}
					return false;
				}
			}, value);
		}

		bool DrawOptionalFloat(const char* label, std::optional<float>& value) {
			bool enabled = value.has_value();
			bool changed = ImGui::Checkbox(label, &enabled);
			ImGui::SameLine(140.0f);
			ImGui::BeginDisabled(!enabled);
			float current = value.value_or(0.0f);
			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::DragFloat((std::string("##") + label).c_str(), &current, 0.01f)) {
				changed = true;
			}
			ImGui::EndDisabled();
			if (enabled) {
				value = current;
			} else {
				value.reset();
			}
			return changed;
		}

		void DrawSelectedParameter() {
			ParameterData* data = document_.Find(selectedParameter_);
			if (!data) {
				ImGui::TextDisabled("Select a parameter.");
				return;
			}

			ImGui::SeparatorText(selectedParameter_.c_str());
			ImGui::Text("Type: %s", ToString(GetParameterType(data->value)).data());
			bool changed = DrawValue(data->value, data->editor);

			ImGui::SeparatorText("Editor settings");
			changed = DrawOptionalFloat("Minimum", data->editor.minimum) || changed;
			changed = DrawOptionalFloat("Maximum", data->editor.maximum) || changed;
			changed = DrawOptionalFloat("Step", data->editor.step) || changed;
			isDirty_ = isDirty_ || changed;

			ImGui::Spacing();
			if (ImGui::Button("Delete parameter")) {
				ImGui::OpenPopup("Delete parameter?");
			}
			if (ImGui::BeginPopupModal(
				"Delete parameter?",
				nullptr,
				ImGuiWindowFlags_AlwaysAutoResize)) {
				ImGui::Text("Delete %s?", selectedParameter_.c_str());
				if (ImGui::Button("Delete", ImVec2(120.0f, 0.0f))) {
					document_.Remove(selectedParameter_);
					selectedParameter_.clear();
					isDirty_ = true;
					ImGui::CloseCurrentPopup();
				}
				ImGui::SameLine();
				if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f))) {
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}
		}

		ParameterDocument document_;
		std::filesystem::path parameterDirectory_;
		std::filesystem::path currentFile_;
		std::vector<std::filesystem::path> files_;
		std::string selectedParameter_;
		std::string status_ = "Ready.";
		bool isStatusError_ = false;
		bool isDirty_ = false;

		std::array<char, 128> newFileName_{};
		std::array<char, 512> newParameterPath_{};
		int newParameterType_ = 2;
	};

	std::filesystem::path g_parameterDirectory;

}

void Main() {
	Window::SetTitle(L"Lazieal Parameter Editor");
	ParameterEditorApp editor(g_parameterDirectory);
	while (System::Update()) {
		editor.Draw();
	}
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	const HRESULT comInitialization = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	if (FAILED(comInitialization)) {
		return EXIT_FAILURE;
	}

	// このアプリ自身の絶対パスを取得する。
	// 例: C:\Tools\ParameterEditor\LaziealParameterEditor.exe
	std::array<wchar_t, 32768> executablePathBuffer{};
	const DWORD pathLength = GetModuleFileNameW(
		nullptr,
		executablePathBuffer.data(),
		static_cast<DWORD>(executablePathBuffer.size()));

	// パスを取得できなかった場合や、用意したバッファに収まらなかった場合は
	// この後の設定ファイルやアセットを正しく探せないため、起動を中止する。
	if (
		pathLength == 0u ||
		pathLength >= static_cast<DWORD>(executablePathBuffer.size())) {
		CoUninitialize();
		return EXIT_FAILURE;
	}

	// 実行ファイル名を取り除き、ParameterEditor.exeが置かれているフォルダを取得する。
	const std::filesystem::path executableDirectory =
		std::filesystem::path(executablePathBuffer.data()).parent_path();

	// RuntimeSetting.iniやAssets/Frameworkを、アプリを起動した場所ではなく
	// ParameterEditor.exeが置かれているフォルダから探せるようにする。
	if (!SetCurrentDirectoryW(executableDirectory.c_str())) {
		CoUninitialize();
		return EXIT_FAILURE;
	}

	std::optional<std::filesystem::path> parameterDirectory = LoadParameterDirectory();
	if (!parameterDirectory) {
		parameterDirectory = SelectParameterDirectory(nullptr);
		if (!parameterDirectory) {
			CoUninitialize();
			return EXIT_SUCCESS;
		}
		if (!SaveParameterDirectory(*parameterDirectory)) {
			MessageBoxW(
				nullptr,
				L"Failed to save the selected parameter directory.",
				L"Lazieal Parameter Editor",
				MB_OK | MB_ICONERROR);
			CoUninitialize();
			return EXIT_FAILURE;
		}
	}
	g_parameterDirectory = *parameterDirectory;

	// ParameterEditor専用の設定ファイルを指定してLaziealを起動する。
	const int exitCode = LGF::Run(Main, executableDirectory / "RuntimeSetting.ini");
	CoUninitialize();
	return exitCode;
}
