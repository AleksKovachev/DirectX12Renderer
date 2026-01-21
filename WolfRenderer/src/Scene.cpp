#include "Scene.hpp" // Scene

#include "rapidjson/istreamwrapper.h" // IStreamWrapper

#include <fstream> // ifstream
#include <iostream> // cerr, cout


Scene::Scene() : log{ std::cout } {}

Scene::Scene( const std::string& sceneFilePath )
	: m_filePath{ sceneFilePath }, log{ std::cout } {
}

void Scene::ParseSceneFile() {
	std::ifstream ifs( m_filePath );
	if ( !ifs.is_open() )
		log( "Could not open scene file: " + m_filePath, LogLevel::Critical );

	rapidjson::IStreamWrapper isw( ifs );
	rapidjson::Document doc;
	doc.ParseStream( isw );
	if ( doc.HasParseError() )
		log( "Parse errors found in scene file: " + m_filePath, LogLevel::Critical );

	ParseSettingsTag( doc );
	ParseObjectsTag( doc );
}

const std::vector<Mesh>& Scene::GetMeshes() const {
	return m_meshes;
}

void Scene::ParseSettingsTag( const rapidjson::Document& doc ) {
	// JSON Tags to look for.
	constexpr char t_settings[]{ "settings" };
	constexpr char t_bgColor[]{ "background_color" };
	constexpr char t_imgSettings[]{ "image_settings" };
	constexpr char t_width[]{ "width" };
	constexpr char t_height[]{ "height" };
	constexpr char t_bucketSize[]{ "bucket_size" };

	if ( !doc.HasMember( t_settings ) || !doc[t_settings].IsObject() ) {
		log( "No settings specified in scene file.", LogLevel::Critical );
		return;
	}

	const rapidjson::Value& settings_{ doc[t_settings] };
	if ( settings_.HasMember( t_bgColor ) && settings_[t_bgColor].IsArray() ) {
		log( "Ignoring input background color.", LogLevel::Debug );
	} else {
		log( "No/wrong background color specified in scene file. "
			"Using default (black).", LogLevel::Warning );
	}

	if ( !settings_.HasMember( t_imgSettings ) || !settings_[t_imgSettings].IsObject() ) {
		log( "No/wrong image settings specified in scene file.", LogLevel::Critical );
		return;
	}
	const rapidjson::Value& imgSettings{ settings_[t_imgSettings] };

	if ( !imgSettings.HasMember( t_width ) || !imgSettings[t_width].IsInt() ) {
		log( "No/wrong resolution width specified in scene file.",
			LogLevel::Critical );
		return;
	}
	settings.renderWidth = imgSettings[t_width].GetUint();

	if ( !imgSettings.HasMember( t_height ) || !imgSettings[t_height].IsInt() ) {
		log( "No/wrong resolution height specified in scene file.",
			LogLevel::Critical );
		return;
	}
	settings.renderHeight = imgSettings[t_height].GetUint();

	if ( imgSettings.HasMember( t_bucketSize ) && imgSettings[t_bucketSize].IsInt() ) {
		log( "Ignoring bucket size information in scene file.", LogLevel::Debug );
	} else {
		log( "Bucket size not specified in scene file." );
	}
}

void Scene::ParseObjectsTag( const rapidjson::Document& doc ) {
	// JSON Tags to look for
	constexpr char t_objects[]{ "objects" };
	constexpr char t_vertices[]{ "vertices" };
	constexpr char t_triangles[]{ "triangles" };

	if ( !doc.HasMember( t_objects ) || !doc[t_objects].IsArray() ) {
		log( "No objects found in scene file.", LogLevel::Critical );
		return;
	}

	const rapidjson::Value::ConstArray& objArr = doc[t_objects].GetArray();

	for ( unsigned i{}; i < objArr.Size(); ++i ) {
		log( "Parsing object: " + std::to_string( i ) );
		if ( !objArr[i].IsObject() ) {
			log( "Non-object found in objects array. Skipping.", LogLevel::Error );
			continue;
		}

		const rapidjson::Value& mesh{ objArr[i] };
		if ( !mesh.HasMember( t_vertices ) || !mesh[t_vertices].IsArray() ) {
			log( "No/wrong format vertices found. Skipping object.", LogLevel::Error );
			continue;
		}
		if ( !mesh.HasMember( t_triangles ) || !mesh[t_triangles].IsArray() ) {
			log( "No/wrong format triangles found. Skipping object.", LogLevel::Error );
			continue;
		}
		LoadMesh( mesh[t_vertices].GetArray(), mesh[t_triangles].GetArray() );
		m_meshes.back().name = "object_" + std::to_string( i );
	}
}


void Scene::LoadMesh( const Value::ConstArray& vertArr, const Value::ConstArray& indArr ) {
	m_meshes.emplace_back();
	Mesh& mesh = m_meshes.back();
	mesh.vertices.reserve( vertArr.Size() / 3 );

	// Load vertices.
	for ( unsigned i{}; i + 2 < vertArr.Size(); i += 3 ) {
		Vertex vertex{ {
			static_cast<float>(vertArr[i].GetDouble()),
			static_cast<float>(vertArr[i + 1].GetDouble()),
			static_cast<float>(vertArr[i + 2].GetDouble())
		}, {} };

		mesh.vertices.push_back( vertex );
	}

	// Build index buffer.
	mesh.indices.reserve( indArr.Size() );
	for ( unsigned i{}; i < indArr.Size(); ++i ) {
		uint32_t triIdx = indArr[i].GetUint(); // Indices can't be negative.
		if ( triIdx >= mesh.vertices.size() ) {
			log( "Triangle index out of bounds. Skipping index: " + std::to_string( triIdx ),
				LogLevel::Error );
			continue;
		}

		mesh.indices.push_back( triIdx );
	}

	mesh.BuildSmoothNormals();
}

void Scene::SetRenderScene( const std::string& filePath ) {
	m_filePath = filePath;
}

const std::string Scene::GetRenderScenePath() const {
	return m_filePath;
}

void Scene::Cleanup() {
	m_meshes.clear();
}
