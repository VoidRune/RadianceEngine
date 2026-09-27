#pragma once
#include "Scene.h"
#include <filesystem>
#include <optional>

std::optional<SceneImport> LoadGltf(Scene& scene, const std::filesystem::path& path, const glm::mat4& transform);
