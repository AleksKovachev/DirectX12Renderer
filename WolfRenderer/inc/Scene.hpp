#ifndef SCENE_HPP
#define SCENE_HPP

#include <DirectXMath.h>
#include <iostream> // cout
#include <string> // string
#include <vector> // vector

#include "rapidjson/document.h" // Document, Value, Value::ConstArray

#include "Geometry.hpp" // Vertex
#include "Logger.hpp" // Logger, LogLevel
#include "Settings.hpp" // Settings

using rapidjson::Value;

class Scene {
public:
	Settings settings; ///< Global scene settings
	Logger log{ std::cout };

	Scene();

	/// @param[in] sceneFilePath  Path to the scene file.
	Scene( const std::string& );

	/// Parse the scene file to get all data.
	void ParseSceneFile();

	/// Gets all the meshes in the scene.
	/// @return  A collection of meshes, ready to iterate.
	const std::vector<Mesh>& GetMeshes() const;

	/// Set the name of the scene file to be processed and rendered.
	/// @param[in] filePath  The path to the scene file.
	void SetRenderScene( const std::string& );

	/// Returns the path to the current scene file.
	const std::string GetRenderScenePath() const;

	/// Cleans up all loaded scene data.
	void Cleanup();
private:
	std::string m_filePath{ "../rsc/scene1.crtscene" };
	std::vector<Mesh> m_meshes;

// crtscene file parsing (json)
private:
	/// Internal function for parsing the settings tag of a crtscene file.
	/// @param[in] doc  A rapidjson document object with the parsed json file.
	void ParseSettingsTag( const rapidjson::Document& );

	/// Internal function for parsing the objects tag of a crtscene file.
	/// @param[in] doc  A rapidjson document object with the parsed json file.
	void ParseObjectsTag( const rapidjson::Document& );

	/// Loads all vertices and triangle indices of a given mesh.
	/// @param[in] vertArr  The vertex array to traverse.
	/// @param[in] indArr  The triangle index array to traverse.
	void LoadMesh( const Value::ConstArray& , const Value::ConstArray& );
};

namespace Raster {
	enum class OutputAlbedoPS : uint32_t {
		Face,				///< Get albedo color from face color / texture.
		RandomColors,		///< Color every triangle in a random color.
		DiscoMode,			///< Quickly switch between 2 colors every N frames.
		ShadowOverlayDebug,	///< Render black and white image to show the shadows only.
		UVChecker,			///< Render a checker texture on all meshes.
		UVGrid				///< Render a grid texture on all meshes.
	};

	struct alignas(16) SceneDataCB {
		// Not worth it packing colors in CBV unless there are a lot of colors.
		DirectX::XMFLOAT4 geometryColor{ 1.f, 1.f, 1.f, 1.f };
		DirectX::XMFLOAT4 textureColorA{ 1.f, 1.f, 1.f, 1.f };
		DirectX::XMFLOAT4 textureColorB{ 0.f, 0.f, 0.f, 1.f };
		uint32_t outputAlbedo{ static_cast<uint32_t>(OutputAlbedoPS::Face) };
		uint32_t discoSpeed{ 200 };
		uint32_t shadeMode{ 0 }; // Defaults to "Lit".
		float textureTiling{ 0.1f };
		float textureProportionsX{ 0.5f };
		float textureProportionsY{ 0.5f };
		uint32_t _pad[2];
	};
}

#endif // SCENE_HPP