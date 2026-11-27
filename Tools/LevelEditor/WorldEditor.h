#pragma once

#include <LGF/Graphics/Camera3D.h>
#include <LGF/Graphics/RenderTargetTexture.h>
#include <LGF/WorldLayout.h>

#include <functional>
#include <optional>

struct ImVec2;

class WorldEditor {
public:
	using AssetPicker = std::function<std::optional<std::filesystem::path>(bool model)>;

	void Open(LGF::WorldLayout layout);
	LGF::WorldLayout& GetLayout();
	bool Draw(const std::filesystem::path& directory, const AssetPicker& pickAsset);

private:
	enum class Operation { Move, Rotate, Scale };

	void AddGameObject();
	bool DrawObjectList();
	bool DrawInspector(const AssetPicker& pickAsset);
	bool DrawAddComponent(LGF::GameObjectDesc& object, const AssetPicker& pickAsset);
	bool DrawPreview(const std::filesystem::path& directory);
	bool DrawGizmo(const ImVec2& origin, const ImVec2& size, LGF::GameObjectDesc& object);
	void RebuildPreview(const std::filesystem::path& directory);
	void SyncObject(const LGF::GameObjectDesc& description);

	LGF::WorldLayout layout_;
	LGF::GameObjectWorld preview_;
	LGF::RenderTargetTexture target_;
	LGF::Camera3D camera_;
	LGF::Vector3 cameraTarget_{};
	LGF::Vector2 canvasSize_{ 640.0f, 360.0f };
	float cameraDistance_ = 8.5f;
	std::optional<std::size_t> selected_;
	Operation operation_ = Operation::Move;
	bool show3D_ = true;
	bool rebuildPreview_ = true;
	std::string previewError_;
};
