#ifndef GEOMETRY_HPP
#define GEOMETRY_HPP

#include "Logger.hpp"

#include <iostream>

/// Simple vertex structure with 2D position.
struct Vertex2D {
	float x;
	float y;
};

/// Simple vertex structure with 3D position.
struct Vertex3D {
	float x;
	float y;
	float z;
};

/// Triangle class with position vertices.
class Triangle {
public:
	static constexpr int vertsInTriangle{ 3 };

	Triangle() = default;

	/// @param[in] vert0  The first triplet vertex.
	/// @param[in] vert1  The next counter-cloackwise vertex.
	/// @param[in] vert2  The last remaining vertex.
	Triangle( const Vertex3D& v0, const Vertex3D& v1, const Vertex3D& v2 ) {
		m_verts[0] = v0;
		m_verts[1] = v1;
		m_verts[2] = v2;
	}

	/// Returns the vertex at the requested index.
	/// @param[in] idx  The index of the requested vertex.
	const Vertex3D GetVertex( unsigned idx ) const {
		if ( idx > 2 ) {
			Logger::log(
				"Provided index exceeds number of vertices. Returning the last one.", 
				std::cout,
				LogLevel::Warning
			);
			return m_verts[2];
		}
		return m_verts[idx];
	}
private:
	/// v0, v1, v2 are the indices of this array. The order matters
	///for cross product and Triangle normal vector calculation.
	Vertex3D m_verts[vertsInTriangle];
};

#endif // GEOMETRY_HPP
