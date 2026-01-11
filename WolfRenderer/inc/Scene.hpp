#ifndef SCENE_HPP
#define SCENE_HPP

#include <iostream> // cout
#include <string> // string
#include <vector> // vector

#include "rapidjson/document.h" // Document, Value, Value::ConstArray

#include "Logger.hpp" // Logger, LogLevel
#include "RenderSettings.hpp" // Settings
#include "Geometry.hpp" // Triangle, Vertex3D


class Scene {
public:
	Settings settings; ///< Global scene settings
	Logger log{ std::cout };

	Scene();

	/// @param[in] sceneFilePath  Path to the scene file.
	Scene( const std::string& );

	/// Parse the scene file to get all data.
	void ParseSceneFile();

	/// Gets all the triangles in the scene.
	/// @return  A collection of triangles, ready to iterate.
	const std::vector<Triangle>& GetTriangles() const;

	/// Set the name of the scene file to be processed and rendered.
	/// @param[in] filePath  The path to the scene file.
	void SetRenderScene( const std::string& );
private:
	std::string m_filePath;
	std::vector<Triangle> m_triangles; ///< All scene triangles
	std::vector<int> m_triIndices; ///< Indices of all scene triangles

// crtscene file parsing (json)
private:
	/// Internal function for parsing the settings tag of a crtscene file.
	/// @param[in] doc  A rapidjson document object with the parsed json file.
	void ParseSettingsTag( const rapidjson::Document& );

	/// Internal function for parsing the objects tag of a crtscene file.
	/// @param[in] doc  A rapidjson document object with the parsed json file.
	void ParseObjectsTag( const rapidjson::Document& );

	/// Internal function for loading vertices from an array.
	/// @param[in] arr  The array to traverse.
	/// @return  A collection of Vertex3D objects representing the vertices.
	std::vector<Vertex3D> LoadVertices( const rapidjson::Value::ConstArray& );

	/// Loads all triangles of a given mesh.
	/// @param[in] arr  The array to traverse.
	/// @param[in] meshVerts  A collection of vertex positions for a given mesh.
	/// @return  A collection of the mesh's triangles.
	void LoadMesh(
		const rapidjson::Value::ConstArray&,
		const std::vector<Vertex3D>&
	);
};

#endif // SCENE_HPP