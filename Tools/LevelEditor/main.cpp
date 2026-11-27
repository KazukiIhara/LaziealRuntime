#include <Windows.h>
#include <ShlObj.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include <LGF/Graphics/Graphics2D.h>
#include <LGF/Level.h>
#include <LGF/Runtime.h>
#include <LGF/System.h>
#include <LGF/UI/UILayout2D.h>

#include <imgui.h>
#include <ImGuizmo.h>
#include <nlohmann/json.hpp>

#include "EditorWidgets.h"
#include "LevelPreview.h"
#include "WorldEditor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <exception>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <stdexcept>
#include <system_error>
#include <utility>

using namespace LGF;
using namespace LevelEditorWidgets;

namespace {

	struct LaunchOptions {
		std::filesystem::path dataDirectory;
		std::string message;
	};

	LaunchOptions g_launchOptions;
	constexpr const char* kElementTypes[]{ "Canvas", "Panel", "Label", "Image", "Button" };
	enum class DocumentKind { Home, UILayout, Level, World };

	LaunchOptions ParseLaunchOptions(int argumentCount, wchar_t* const* arguments) {
		if (!arguments) {
			return { .message = "Could not read the startup arguments. Use Open... to select a UI layout." };
		}
		if (argumentCount <= 1) {
			return {};
		}
		const std::wstring_view option(arguments[1]);
		if (option != L"--level-data-directory") {
			return { .message = "Unknown startup option: \"" + GetPathText(arguments[1]) +
				"\". Expected --level-data-directory \"<folder>\". You can still use Open... to select a UI layout." };
		}
		if (argumentCount < 3 || arguments[2][0] == L'\0') {
			return { .message = "--level-data-directory requires a folder. Use Open... to select a UI layout." };
		}
		if (argumentCount > 3) {
			return { .message = "--level-data-directory expects one folder. Enclose paths with spaces in quotes." };
		}

		std::error_code error;
		auto directory = std::filesystem::absolute(arguments[2], error).lexically_normal();
		if (error || !std::filesystem::is_directory(directory, error)) {
			return { .message = "Level data directory is unavailable: \"" + GetPathText(arguments[2]) +
				"\". Use Open... to select a UI layout." };
		}
		if (directory != directory.root_path() && !directory.has_filename()) {
			directory = directory.parent_path();
		}
		return { .dataDirectory = std::move(directory) };
	}

	std::filesystem::path PathFromText(std::string_view text) {
		return std::filesystem::path(std::u8string(text.begin(), text.end()));
	}

	bool CanHaveChildren(UIElementType type) {
		return type == UIElementType::Canvas || type == UIElementType::Panel;
	}

	nlohmann::json ReadDocumentJson(const std::filesystem::path& path) {
		std::ifstream file(path, std::ios::binary);
		if (!file) {
			throw std::runtime_error("Failed to open the JSON file.");
		}
		auto root = nlohmann::json::parse(file);
		if (!root.is_object()) {
			throw std::runtime_error("Expected a JSON object.");
		}
		return root;
	}

	DocumentKind GetDocumentKind(const nlohmann::json& root) {
		if (root.contains("Type")) {
			const std::string type = root.at("Type").get<std::string>();
			if (type == "Level") {
				return DocumentKind::Level;
			}
			if (type == "UILayout2D") {
				return DocumentKind::UILayout;
			}
			if (type == "WorldLayout") {
				return DocumentKind::World;
			}
			throw std::runtime_error("Unknown document Type: " + type);
		}
		// Typeの追加前に作られた配置も開けるようにする。
		if (root.contains("Elements")) {
			return DocumentKind::UILayout;
		}
		if (root.contains("UI") || root.contains("World")) {
			return DocumentKind::Level;
		}
		if (root.contains("Objects")) {
			return DocumentKind::World;
		}
		throw std::runtime_error("Expected a level, UI layout or world layout.");
	}

	class LevelEditorApp {
	public:
		explicit LevelEditorApp(const LaunchOptions& launchOptions) :
			dataDirectory_(launchOptions.dataDirectory) {

			LoadFont();
			if (!launchOptions.message.empty()) {
				status_ = launchOptions.message;
			}
		}

		void Draw() {
			ImGuizmo::BeginFrame();
			const ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->WorkPos);
			ImGui::SetNextWindowSize(viewport->WorkSize);
			constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground;
			if (ImGui::Begin("Lazieal Level Editor", nullptr, flags)) {
				if (document_ != DocumentKind::Home) {
					DrawToolbar();
				}
				if (document_ == DocumentKind::Home) {
					DrawHome();
				} else if (document_ == DocumentKind::Level) {
					DrawLevel();
				} else if (document_ == DocumentKind::World) {
					isDirty_ |= worldEditor_.Draw(filePath_.parent_path(), [this](bool model) -> std::optional<std::filesystem::path> {
						const auto path = PickFile(false, model ? FileKind::Model : FileKind::Texture);
						return path ? std::optional{ RebaseReference(*path, filePath_.parent_path()) } : std::nullopt;
					});
				} else if (ImGui::BeginTable("UI layout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
					ImGui::TableSetupColumn("Hierarchy", ImGuiTableColumnFlags_WidthFixed, 305.0f);
					ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthStretch);
					ImGui::TableNextColumn();
					if (ImGui::BeginChild("Hierarchy")) {
						DrawInspector();
					}
					ImGui::EndChild();
					ImGui::TableNextColumn();
					constexpr ImGuiWindowFlags previewFlags = ImGuiWindowFlags_NoBackground |
						ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
					if (ImGui::BeginChild("Preview", {}, ImGuiChildFlags_None, previewFlags)) {
						DrawPreview();
					}
					ImGui::EndChild();
					ImGui::EndTable();
				}
			}
			ImGui::End();
		}

	private:
		enum class DocumentAction { Home, NewUI, NewLevel, NewWorld, Open };
		enum class FileKind { Document, Model, Texture };

		void MarkChanged() {
			isDirty_ = true;
			rebuildPreview_ = true;
		}

		void NewLayout() {
			document_ = DocumentKind::UILayout;
			layout_ = {};
			layout_.elements.push_back({ .id = "Canvas" });
			filePath_.clear();
			selectedIndex_ = 0u;
			isDirty_ = false;
			rebuildPreview_ = true;
			status_.clear();
		}

		void NewLevel() {
			document_ = DocumentKind::Level;
			level_ = {};
			filePath_.clear();
			isDirty_ = false;
			rebuildLevelPreview_ = true;
			status_.clear();
		}

		void NewWorld() {
			document_ = DocumentKind::World;
			worldEditor_.Open({});
			filePath_.clear();
			isDirty_ = false;
			status_.clear();
		}

		std::optional<std::filesystem::path> PickFile(bool save, FileKind kind = FileKind::Document, std::optional<DocumentKind> expected = {}) {
			using Microsoft::WRL::ComPtr;

			const auto dialogError = [this](HRESULT result) {
				if (result != HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
					status_ = "File dialog error: " + std::to_string(result);
				}
				return std::nullopt;
			};

			ComPtr<IFileDialog> dialog;
			HRESULT result = CoCreateInstance(
				save ? CLSID_FileSaveDialog : CLSID_FileOpenDialog,
				nullptr,
				CLSCTX_INPROC_SERVER,
				IID_PPV_ARGS(dialog.GetAddressOf()));
			if (FAILED(result)) {
				return dialogError(result);
			}

			DWORD options = 0u;
			result = dialog->GetOptions(&options);
			if (SUCCEEDED(result)) {
				result = dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR | FOS_PATHMUSTEXIST |
					(save ? FOS_OVERWRITEPROMPT : FOS_FILEMUSTEXIST));
			}
			const wchar_t* label = L"Level data";
			const wchar_t* extensions = L"*.json";
			if (kind == FileKind::Model) {
				label = L"3D models";
				extensions = L"*.glb;*.gltf;*.obj;*.fbx";
			} else if (kind == FileKind::Texture) {
				label = L"Textures";
				extensions = L"*.png;*.bmp;*.jpg;*.dds";
			}
			const COMDLG_FILTERSPEC filters[]{
				{ label, extensions },
				{ L"All files", L"*.*" },
			};
			if (SUCCEEDED(result)) {
				result = dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), filters);
			}
			if (SUCCEEDED(result) && kind == FileKind::Document) {
				result = dialog->SetDefaultExtension(L"json");
			}
			if (SUCCEEDED(result) && save && filePath_.empty()) {
				const wchar_t* name = document_ == DocumentKind::Level ? L"Untitled.level.json" :
					(document_ == DocumentKind::World ? L"Untitled.world.json" : L"Untitled.ui.json");
				result = dialog->SetFileName(name);
			}
			if (FAILED(result)) {
				return dialogError(result);
			}

			auto initialDirectory = dataDirectory_.empty() ? filePath_.parent_path() : dataDirectory_;
			if (!dataDirectory_.empty() && kind == FileKind::Document && (expected || (save && filePath_.empty()))) {
				const DocumentKind mode = expected.value_or(document_);
				if (mode == DocumentKind::UILayout || mode == DocumentKind::World) {
					const auto folder = dataDirectory_ / (mode == DocumentKind::UILayout ? "UI" : "World");
					std::error_code error;
					if (std::filesystem::is_directory(folder, error)) {
						initialDirectory = folder;
					}
				}
			}
			if (!initialDirectory.empty()) {
				ComPtr<IShellItem> folder;
				result = SHCreateItemFromParsingName(initialDirectory.c_str(), nullptr, IID_PPV_ARGS(folder.GetAddressOf()));
				if (SUCCEEDED(result)) {
					// ダイアログの履歴より、起動時に指定されたフォルダを優先する。
					result = dialog->SetFolder(folder.Get());
				}
				if (FAILED(result)) {
					status_ = "Could not open the initial folder: " + GetPathText(initialDirectory);
				}
			}

			result = dialog->Show(GetActiveWindow());
			if (FAILED(result)) {
				return dialogError(result);
			}
			ComPtr<IShellItem> item;
			result = dialog->GetResult(item.GetAddressOf());
			if (FAILED(result)) {
				return dialogError(result);
			}
			PWSTR selectedPath = nullptr;
			result = item->GetDisplayName(SIGDN_FILESYSPATH, &selectedPath);
			const std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> path(selectedPath, CoTaskMemFree);
			if (FAILED(result)) {
				return dialogError(result);
			}
			return std::filesystem::path(path.get());
		}

		void Open(const std::filesystem::path& path, std::optional<DocumentKind> expected = {}) {
			try {
				const DocumentKind kind = GetDocumentKind(ReadDocumentJson(path));
				if (expected && *expected != kind) {
					throw std::runtime_error("Choose a JSON file for the selected mode.");
				}
				if (kind == DocumentKind::UILayout) {
					layout_ = LoadUILayout2D(path);
					selectedIndex_.reset();
					rebuildPreview_ = true;
				} else if (kind == DocumentKind::Level) {
					const auto result = LoadLevelData(path, level_);
					if (!result) {
						status_ = result.message;
						return;
					}
					rebuildLevelPreview_ = true;
				} else {
					worldEditor_.Open(LoadWorldLayout(path));
				}
				document_ = kind;
				filePath_ = path;
				isDirty_ = false;
				status_.clear();
			} catch (const std::exception& error) {
				status_ = GetPathText(path) + ": " + error.what();
			}
		}

		void Save(const std::filesystem::path& path) {
			try {
				if (document_ == DocumentKind::Level) {
					const LevelData savedLevel{
						.uiLayoutPath = RebaseReference(level_.uiLayoutPath, path.parent_path()),
						.worldLayoutPath = RebaseReference(level_.worldLayoutPath, path.parent_path()),
					};
					const auto result = SaveLevelData(path, savedLevel);
					if (!result) {
						status_ = result.message;
						return;
					}
					level_ = savedLevel;
					rebuildLevelPreview_ = true;
				} else if (document_ == DocumentKind::UILayout) {
					UILayout2D savedLayout = layout_;
					if (filePath_.parent_path() != path.parent_path()) {
						for (auto& element : savedLayout.elements) {
							if (!element.texturePath.empty()) {
								element.texturePath = GetPathText(RebaseReference(PathFromText(element.texturePath), path.parent_path()));
							}
						}
					}
					SaveUILayout2D(path, savedLayout);
					layout_ = std::move(savedLayout);
					rebuildPreview_ = true;
				} else {
					WorldLayout savedLayout = worldEditor_.GetLayout();
					if (filePath_.parent_path() != path.parent_path()) {
						for (auto& object : savedLayout.objects) {
							for (auto& component : object.components) {
								if (auto* model = std::get_if<ModelRendererDesc>(&component)) {
									model->modelPath = RebaseReference(model->modelPath, path.parent_path());
								} else if (auto* sprite = std::get_if<SpriteRendererDesc>(&component)) {
									sprite->texturePath = RebaseReference(sprite->texturePath, path.parent_path());
								}
							}
						}
					}
					SaveWorldLayout(path, savedLayout);
					worldEditor_.GetLayout() = std::move(savedLayout);
				}
				filePath_ = path;
				isDirty_ = false;
				status_ = "Saved.";
			} catch (const std::exception& error) {
				status_ = error.what();
			}
		}

		std::filesystem::path ResolveReference(const std::filesystem::path& reference) const {
			return std::filesystem::absolute(filePath_.parent_path() / reference).lexically_normal();
		}

		std::filesystem::path RebaseReference(const std::filesystem::path& reference, const std::filesystem::path& directory) const {
			if (reference.empty()) {
				return {};
			}
			const auto source = ResolveReference(reference);
			const auto relative = source.lexically_relative(directory);
			return relative.empty() ? source : relative;
		}

		std::optional<std::filesystem::path> DrawLevelReference(const char* label, std::filesystem::path& reference, bool ui) {
			ImGui::PushID(label);
			const std::string displayedPath = reference.empty() ? "(none)" : GetPathText(reference);
			ImGui::TextWrapped("%s: %s", label, displayedPath.c_str());
			if (ImGui::Button("Choose...")) {
				if (const auto path = PickFile(false, FileKind::Document, ui ? DocumentKind::UILayout : DocumentKind::World)) {
					try {
						if (ui) {
							LoadUILayout2D(*path);
						} else {
							LoadWorldLayout(*path);
						}
						reference = RebaseReference(*path, filePath_.parent_path());
						isDirty_ = true;
						rebuildLevelPreview_ = true;
						status_.clear();
					} catch (const std::exception& error) {
						status_ = GetPathText(*path) + ": " + error.what();
					}
				}
			}
			ImGui::SameLine();
			ImGui::BeginDisabled(reference.empty());
			const bool openReference = ImGui::Button(ui ? "Open in UI Edit" : "Open in World Edit");
			ImGui::SameLine();
			if (ImGui::Button("Clear")) {
				reference.clear();
				isDirty_ = true;
				rebuildLevelPreview_ = true;
			}
			ImGui::EndDisabled();
			ImGui::PopID();
			return openReference ? std::optional{ ResolveReference(reference) } : std::nullopt;
		}

		void DrawLevel() {
			std::optional<std::filesystem::path> openReference;
			if (ImGui::BeginTable("Level preview", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
				ImGui::TableSetupColumn("References", ImGuiTableColumnFlags_WidthFixed, 305.0f);
				ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableNextColumn();
				if (ImGui::BeginChild("References")) {
					ImGui::SeparatorText("Level references");
					openReference = DrawLevelReference("UI layout", level_.uiLayoutPath, true);
					if (const auto worldReference = DrawLevelReference("World layout", level_.worldLayoutPath, false)) {
						openReference = worldReference;
					}
					ImGui::Spacing();
					if (ImGui::Button("Reload preview")) {
						rebuildLevelPreview_ = true;
					}
				}
				ImGui::EndChild();
				ImGui::TableNextColumn();
				constexpr ImGuiWindowFlags previewFlags = ImGuiWindowFlags_NoBackground |
					ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
				if (ImGui::BeginChild("Combined preview", {}, ImGuiChildFlags_None, previewFlags)) {
					if (rebuildLevelPreview_) {
						levelPreview_.Load(
							{
								.uiLayoutPath = level_.uiLayoutPath.empty() ? std::filesystem::path{} : ResolveReference(level_.uiLayoutPath),
								.worldLayoutPath = level_.worldLayoutPath.empty() ? std::filesystem::path{} : ResolveReference(level_.worldLayoutPath),
							},
							font_);
						rebuildLevelPreview_ = false;
					}
					levelPreview_.Draw();
				}
				ImGui::EndChild();
				ImGui::EndTable();
			}
			if (openReference) {
				RequestDocumentAction(DocumentAction::Open, *openReference);
			}
		}

		void DrawHomeSection(const char* title, const char* description, DocumentKind kind, DocumentAction create) {
			ImGui::TableNextColumn();
			ImGui::PushID(title);
			ImGui::SeparatorText(title);
			ImGui::TextWrapped("%s", description);
			ImGui::Spacing();
			if (ImGui::Button("Open...")) {
				if (const auto path = PickFile(false, FileKind::Document, kind)) {
					Open(*path, kind);
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("New")) {
				PerformDocumentAction(create, {});
			}
			ImGui::PopID();
		}

		void DrawHome() {
			if (ImGui::BeginTable("Editor modes", 3, ImGuiTableFlags_SizingStretchSame)) {
				DrawHomeSection("UI Edit", "Create or edit the screen UI layout.", DocumentKind::UILayout, DocumentAction::NewUI);
				DrawHomeSection("World Edit", "Place 2D sprites and 3D models.", DocumentKind::World, DocumentAction::NewWorld);
				DrawHomeSection("Level Preview", "Preview the world with its screen UI and edit level references.", DocumentKind::Level, DocumentAction::NewLevel);
				ImGui::EndTable();
			}
			if (!status_.empty()) {
				ImGui::Spacing();
				ImGui::TextWrapped("%s", status_.c_str());
			}
		}

		void LoadFont() {
			std::array<wchar_t, MAX_PATH> windowsDirectory{};
			const UINT length = GetWindowsDirectoryW(windowsDirectory.data(), static_cast<UINT>(windowsDirectory.size()));
			const auto projectFont = dataDirectory_.parent_path() / "Fonts/NotoSansJP/NotoSansJP-wght.ttf";
			if (length == 0u || length >= windowsDirectory.size()) {
				return;
			}
			const auto systemFont = std::filesystem::path(windowsDirectory.data()) / "Fonts/segoeui.ttf";
			try {
				const bool hasProjectFont = !dataDirectory_.empty() && std::filesystem::is_regular_file(projectFont);
				font_ = Font2D(hasProjectFont ? projectFont : systemFont, 22u);
			} catch (const std::exception& error) {
				if (status_.empty()) {
					status_ = error.what();
				}
			}
		}

		std::filesystem::path ResolveTexturePath(std::string_view reference) const {
			return ResolveReference(PathFromText(reference));
		}

		void PerformDocumentAction(DocumentAction action, const std::filesystem::path& reference) {
			if (action == DocumentAction::Home) {
				document_ = DocumentKind::Home;
				filePath_.clear();
				isDirty_ = false;
				status_.clear();
			} else if (action == DocumentAction::NewUI) {
				NewLayout();
			} else if (action == DocumentAction::NewLevel) {
				NewLevel();
			} else if (action == DocumentAction::NewWorld) {
				NewWorld();
			} else if (!reference.empty()) {
				Open(reference);
			} else if (const auto path = PickFile(false)) {
				Open(*path);
			}
		}

		void RequestDocumentAction(DocumentAction action, const std::filesystem::path& reference = {}) {
			if (isDirty_) {
				pendingAction_ = action;
				pendingReference_ = reference;
				ImGui::OpenPopup("Unsaved changes");
			} else {
				PerformDocumentAction(action, reference);
			}
		}

		void DrawToolbar() {
			std::optional<DocumentAction> requestedNew;
			if (ImGui::Button("Home")) {
				RequestDocumentAction(DocumentAction::Home);
			}
			if (document_ == DocumentKind::Home) {
				return;
			}
			ImGui::SameLine();
			if (ImGui::Button("New...")) {
				ImGui::OpenPopup("New document");
			}
			if (ImGui::BeginPopup("New document")) {
				if (ImGui::MenuItem("Level")) {
					requestedNew = DocumentAction::NewLevel;
				}
				if (ImGui::MenuItem("UI layout")) {
					requestedNew = DocumentAction::NewUI;
				}
				if (ImGui::MenuItem("World layout")) {
					requestedNew = DocumentAction::NewWorld;
				}
				ImGui::EndPopup();
			}
			if (requestedNew) {
				RequestDocumentAction(*requestedNew);
			}
			ImGui::SameLine();
			if (ImGui::Button("Open...")) {
				RequestDocumentAction(DocumentAction::Open);
			}
			ImGui::SameLine();
			if (ImGui::Button("Save")) {
				if (!filePath_.empty()) {
					Save(filePath_);
				} else if (const auto path = PickFile(true)) {
					Save(*path);
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Save as...")) {
				if (const auto path = PickFile(true)) {
					Save(*path);
				}
			}
			ImGui::SameLine();
			const char* untitled = document_ == DocumentKind::Level ? "Untitled level" :
				(document_ == DocumentKind::World ? "Untitled world layout" : "Untitled UI layout");
			const std::string title = filePath_.empty() ? untitled : GetPathText(filePath_);
			ImGui::Text("%s%s", title.c_str(), isDirty_ ? " *" : "");
			if (!status_.empty()) {
				ImGui::TextWrapped("%s", status_.c_str());
			}
			if (ImGui::BeginPopupModal("Unsaved changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
				ImGui::TextUnformatted("Discard unsaved changes?");
				if (ImGui::Button("Discard")) {
					ImGui::CloseCurrentPopup();
					PerformDocumentAction(pendingAction_, pendingReference_);
				}
				ImGui::SameLine();
				if (ImGui::Button("Cancel")) {
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}
		}

		const UILayoutElement2D* FindElement(std::string_view id) const {
			const auto iterator = std::ranges::find(layout_.elements, id, &UILayoutElement2D::id);
			return iterator == layout_.elements.end() ? nullptr : &*iterator;
		}

		bool HasAncestor(const UILayoutElement2D& element, std::string_view id) const {
			for (auto* parent = FindElement(element.parentId); parent; parent = FindElement(parent->parentId)) {
				if (parent->id == id) {
					return true;
				}
			}
			return false;
		}

		void AddElement(UIElementType type) {
			const std::string prefix = kElementTypes[static_cast<std::size_t>(type)];
			UILayoutElement2D element{ .id = prefix, .type = type };
			for (std::size_t suffix = 2u; FindElement(element.id); ++suffix) {
				element.id = prefix + std::to_string(suffix);
			}
			if (type != UIElementType::Canvas) {
				const auto firstCanvas = std::ranges::find(layout_.elements, UIElementType::Canvas, &UILayoutElement2D::type);
				if (firstCanvas == layout_.elements.end()) {
					AddElement(UIElementType::Canvas);
				}
				const auto& selected = layout_.elements[selectedIndex_.value_or(0u)];
				element.parentId = CanHaveChildren(selected.type) ? selected.id : selected.parentId;
				element.transform.position = { 16.0f, 16.0f };
				element.transform.size = { 144.0f, 52.0f };
				if (type == UIElementType::Panel) {
					element.transform.size = { 280.0f, 170.0f };
					element.color = UIPanelDesc{}.color;
				} else if (type == UIElementType::Button) {
					element.text = "Button";
					element.color = UIButtonStyle{}.normalColor;
				} else if (type == UIElementType::Label) {
					element.text = "Label";
				}
			}
			layout_.elements.push_back(std::move(element));
			selectedIndex_ = layout_.elements.size() - 1u;
			MarkChanged();
		}

		void DrawHierarchy(std::string_view parentId) {
			for (std::size_t index = 0u; index < layout_.elements.size(); ++index) {
				const auto& element = layout_.elements[index];
				if (element.parentId != parentId) {
					continue;
				}
				const bool hasChildren = std::ranges::find(layout_.elements, element.id, &UILayoutElement2D::parentId) != layout_.elements.end();
				ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
				flags |= hasChildren ? 0 : ImGuiTreeNodeFlags_Leaf;
				flags |= selectedIndex_ == index ? ImGuiTreeNodeFlags_Selected : 0;
				const bool open = ImGui::TreeNodeEx(&element, flags, "%s [%s]", element.id.c_str(), kElementTypes[static_cast<std::size_t>(element.type)]);
				if (ImGui::IsItemClicked()) {
					selectedIndex_ = index;
				}
				if (open) {
					if (hasChildren) {
						DrawHierarchy(element.id);
					}
					ImGui::TreePop();
				}
			}
		}

		void DrawInspector() {
			if (EditVector2("Layout size", layout_.size, 1.0f, 1.0f, 8192.0f)) {
				MarkChanged();
			}
			ImGui::SeparatorText("Hierarchy");
			if (ImGui::Button("Add...")) {
				ImGui::OpenPopup("Add element");
			}
			if (ImGui::BeginPopup("Add element")) {
				for (std::size_t index = 0u; index < std::size(kElementTypes); ++index) {
					if (ImGui::MenuItem(kElementTypes[index])) {
						AddElement(static_cast<UIElementType>(index));
					}
				}
				ImGui::EndPopup();
			}
			DrawHierarchy({});
			if (!selectedIndex_) {
				return;
			}

			ImGui::SeparatorText("Properties");
			auto& element = layout_.elements[*selectedIndex_];
			std::string id = element.id;
			if (EditText("Id", id) && !id.empty() && !FindElement(id)) {
				for (auto& child : layout_.elements) {
					if (child.parentId == element.id) {
						child.parentId = id;
					}
				}
				element.id = std::move(id);
				MarkChanged();
			}
			if (element.type != UIElementType::Canvas && ImGui::BeginCombo("Parent", element.parentId.c_str())) {
				for (const auto& parent : layout_.elements) {
					if (&parent != &element && CanHaveChildren(parent.type) && !HasAncestor(parent, element.id) &&
						ImGui::Selectable(parent.id.c_str(), element.parentId == parent.id)) {
						element.parentId = parent.id;
						MarkChanged();
					}
				}
				ImGui::EndCombo();
			}

			bool changed = ImGui::Checkbox("Visible", &element.isVisible);
			changed |= ImGui::Checkbox("Enabled", &element.isEnabled);
			changed |= EditVector2("Position", element.transform.position);
			changed |= EditVector2("Size", element.transform.size, 1.0f, 0.0f, 8192.0f);
			changed |= EditVector2("Anchor", element.transform.anchor, 0.01f, 0.0f, 1.0f);
			changed |= EditVector2("Pivot", element.transform.pivot, 0.01f, 0.0f, 1.0f);
			if (element.type == UIElementType::Label || element.type == UIElementType::Button) {
				changed |= EditText("Text", element.text);
			}
			if (element.type == UIElementType::Image) {
				changed |= EditText("Texture", element.texturePath);
				if (ImGui::Button("Browse texture...")) {
					if (const auto path = PickFile(false, FileKind::Texture)) {
						const auto relative = path->lexically_relative(filePath_.parent_path());
						element.texturePath = GetPathText(relative.empty() ? *path : relative);
						changed = true;
					}
				}
			}
			if (element.type != UIElementType::Canvas) {
				float color[]{ element.color.x, element.color.y, element.color.z, element.color.w };
				if (ImGui::ColorEdit4("Color", color)) {
					element.color = { color[0], color[1], color[2], color[3] };
					changed = true;
				}
			}
			if (changed) {
				MarkChanged();
			}
			if (ImGui::Button("Remove subtree")) {
				const std::string removedId = element.id;
				std::vector<std::string> removedIds{ removedId };
				for (const auto& candidate : layout_.elements) {
					if (HasAncestor(candidate, removedId)) {
						removedIds.push_back(candidate.id);
					}
				}
				std::erase_if(layout_.elements, [&](const auto& candidate) {
					return std::ranges::find(removedIds, candidate.id) != removedIds.end();
				});
				selectedIndex_.reset();
				MarkChanged();
			}
		}

		void RebuildPreview() {
			rebuildPreview_ = false;
			previewError_.clear();
			try {
				preview_ = CreateUIWorld2D(layout_, font_, [&](std::string_view reference) {
					try {
						return Texture(ResolveTexturePath(reference), TextureLoadDesc::Color());
					} catch (const std::exception& error) {
						previewError_ = std::string(reference) + ": " + error.what();
						return Texture{};
					}
				});
				previewValid_ = true;
			} catch (const std::exception& error) {
				previewValid_ = false;
				previewError_ = error.what();
			}
		}

		void DrawPreview() {
			if (rebuildPreview_) {
				RebuildPreview();
			}
			ImGui::Checkbox("Test buttons", &testButtons_);
			if (!previewError_.empty()) {
				ImGui::TextWrapped("%s", previewError_.c_str());
			}
			if (!previewValid_) {
				return;
			}

			const ImVec2 available = ImGui::GetContentRegionAvail();
			const float scale = std::min(available.x / layout_.size.x, available.y / layout_.size.y);
			if (!std::isfinite(scale) || scale <= 0.0f) {
				return;
			}
			const ImVec2 origin = ImGui::GetCursorScreenPos();
			const ImVec2 size(layout_.size.x * scale, layout_.size.y * scale);
			if (!Graphics2D::SetDrawingViewport({ { origin.x, origin.y }, { size.x, size.y } }, layout_.size)) {
				return;
			}
			ImGui::Dummy(size);
			const ImVec2 end(origin.x + size.x, origin.y + size.y);
			const ImVec2 mouse = ImGui::GetMousePos();
			const bool hovered = ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(origin, end);
			const UIWorldInput input{
				.cursorPosition = { (mouse.x - origin.x) / scale, (mouse.y - origin.y) / scale },
				.isCursorPressed = testButtons_ && ImGui::IsMouseDown(ImGuiMouseButton_Left),
				.isCursorEnabled = testButtons_ && hovered,
			};
			preview_.world.Update(input, layout_.size);
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			drawList->AddRect(origin, end, IM_COL32(85, 100, 120, 255));
			if (!testButtons_ && selectedIndex_) {
				const UIHandle handle = preview_.handles[*selectedIndex_];
				if (auto rect = preview_.world.GetScreenRect(handle)) {
					drawList->PushClipRect(origin, end, true);
					auto& element = layout_.elements[*selectedIndex_];
					const bool movable = element.type != UIElementType::Canvas ||
						element.transform.size.x > 0.0f || element.transform.size.y > 0.0f;
					if (movable && DrawGizmo(drawList, origin, size, *rect, element)) {
						preview_.world.SetTransform(handle, element.transform);
						preview_.world.Update(input, layout_.size);
						rect = preview_.world.GetScreenRect(handle);
						isDirty_ = true;
					}
					if (rect) {
						DrawSelection(drawList, origin, scale, *rect);
					}
					drawList->PopClipRect();
				}
			}
			Rect2D{ {}, layout_.size }.Draw(Vector4{ 0.025f, 0.035f, 0.055f, 1.0f });
			preview_.world.Draw();
		}

		void DrawSelection(ImDrawList* drawList, const ImVec2& origin, float scale, const Rect2D& rect) const {
			const ImVec2 minimum(origin.x + rect.leftTop.x * scale, origin.y + rect.leftTop.y * scale);
			const ImVec2 maximum(minimum.x + rect.size.x * scale, minimum.y + rect.size.y * scale);
			drawList->AddRect(minimum, maximum, IM_COL32(255, 196, 92, 255));
		}

		bool DrawGizmo(ImDrawList* drawList, const ImVec2& origin, const ImVec2& size, const Rect2D& rect, UILayoutElement2D& element) {
			// レイアウトの下向きYをギズモ座標の上向きYに変換する。
			constexpr float view[] = {
				1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 1.0f,
			};
			const float projection[] = {
				2.0f / layout_.size.x, 0.0f, 0.0f, 0.0f,
				0.0f, 2.0f / layout_.size.y, 0.0f, 0.0f,
				0.0f, 0.0f, 0.5f, 0.0f,
				-1.0f, 1.0f, 0.0f, 1.0f,
			};
			const Vector2 center = rect.leftTop + rect.size * 0.5f;
			float matrix[] = {
				1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				center.x, -center.y, 0.0f, 1.0f,
			};
			ImGuizmo::SetDrawlist(drawList);
			ImGuizmo::SetRect(origin.x, origin.y, size.x, size.y);
			ImGuizmo::SetOrthographic(true);
			ImGuizmo::SetAxisMask(false, false, true);
			ImGuizmo::PushID(element.id.c_str());
			const bool moved = ImGuizmo::Manipulate(view, projection, ImGuizmo::TRANSLATE, ImGuizmo::WORLD, matrix);
			if (moved) {
				element.transform.position += Vector2{ matrix[12] - center.x, -matrix[13] - center.y };
			}
			ImGuizmo::PopID();
			return moved;
		}

		DocumentKind document_ = DocumentKind::Home;
		LevelData level_;
		LevelPreview levelPreview_;
		WorldEditor worldEditor_;
		UILayout2D layout_;
		UILayoutInstance2D preview_;
		Font2D font_;
		std::filesystem::path dataDirectory_;
		std::filesystem::path filePath_;
		std::optional<std::size_t> selectedIndex_;
		std::string status_;
		std::string previewError_;
		DocumentAction pendingAction_ = DocumentAction::NewUI;
		std::filesystem::path pendingReference_;
		bool isDirty_{};
		bool rebuildLevelPreview_ = true;
		bool rebuildPreview_ = true;
		bool previewValid_{};
		bool testButtons_{};
	};

}

void Main() {
	LevelEditorApp editor(g_launchOptions);
	while (System::Update()) {
		editor.Draw();
	}
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	int argumentCount = 0;
	LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
	// 相対パスは作業ディレクトリを実行ファイルの場所へ切り替える前に解決する。
	g_launchOptions = ParseLaunchOptions(argumentCount, arguments);
	LocalFree(arguments);

	std::array<wchar_t, 32768> executablePath{};
	const DWORD pathLength = GetModuleFileNameW(nullptr, executablePath.data(), static_cast<DWORD>(executablePath.size()));
	if (pathLength == 0u || pathLength >= executablePath.size()) {
		return EXIT_FAILURE;
	}

	const std::filesystem::path executableDirectory = std::filesystem::path(executablePath.data()).parent_path();
	if (!SetCurrentDirectoryW(executableDirectory.c_str())) {
		return EXIT_FAILURE;
	}
	// ファイルダイアログを使うメインスレッドのCOMを初期化する。
	if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) {
		return EXIT_FAILURE;
	}
	const int exitCode = LGF::Run(Main, executableDirectory / "RuntimeSetting.ini");
	CoUninitialize();
	return exitCode;
}
