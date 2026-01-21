#ifndef GEOMETRY_HPP
#define GEOMETRY_HPP

#include "Logger.hpp"

#include <DirectXMath.h>
#include <iostream>

struct Vertex {
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT3 normal;
	//DirectX::XMFLOAT3 m_uv; ///< texCoord
	//DirectX::XMFLOAT3 m_tangent;
};


struct Mesh {
	std::string name;
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices; ///< Triangle indices (triplets).
	// Material material;
	// uint32_t materialId{};
	// DirectX::XMFLOAT4x4 transform; ///< Row-major.

	void BuildSmoothNormals( float epsilon = 1e-6f ) {
		// Reset normals so this functino can be used for RE-building.
		for ( Vertex& vertex : vertices )
			vertex.normal = { 0.f, 0.f, 0.f };

		using namespace DirectX;

		// accumulate face normals (area-weighted)
		for ( size_t i{}; i + 2 < indices.size(); i += 3 ) {
			uint32_t i0 = indices[i];
			uint32_t i1 = indices[i + 1];
			uint32_t i2 = indices[i + 2];
			XMFLOAT3 p0 = vertices[i0].position;
			XMFLOAT3 p1 = vertices[i1].position;
			XMFLOAT3 p2 = vertices[i2].position;

			XMVECTOR vert0 = XMLoadFloat3( &p0 );
			XMVECTOR vert1 = XMLoadFloat3( &p1 );
			XMVECTOR vert2 = XMLoadFloat3( &p2 );

			XMVECTOR edge1 = XMVectorSubtract( vert1, vert0 );
			XMVECTOR edge2 = XMVectorSubtract( vert2, vert0 );
			XMVECTOR faceN = XMVector3Cross( edge1, edge2 );

			// Check for degenerates.
			float lengthSq = XMVectorGetX( XMVector3LengthSq( faceN ) );
			if ( lengthSq <= epsilon * epsilon )
				continue;

			XMFLOAT3 fn;
			XMStoreFloat3( &fn, faceN );

			vertices[i0].normal.x += fn.x;
			vertices[i0].normal.y += fn.y;
			vertices[i0].normal.z += fn.z;

			vertices[i1].normal.x += fn.x;
			vertices[i1].normal.y += fn.y;
			vertices[i1].normal.z += fn.z;

			vertices[i2].normal.x += fn.x;
			vertices[i2].normal.y += fn.y;
			vertices[i2].normal.z += fn.z;
		}

		// Normalize accumulated normals.
		for ( Vertex& vert : vertices ) {
			XMVECTOR n = XMLoadFloat3( &vert.normal );
			float lenSq = XMVectorGetX( XMVector3LengthSq( n ) );

			if ( lenSq > epsilon * epsilon ) {
				n = XMVector3Normalize( n );
				XMStoreFloat3( &vert.normal, n );
			} else {
				// Fallback normal, e.g. (0, 1, 0) or compute from geometry.
				vert.normal = { 0.f, 1.f, 0.f };
			}
		}
	}
};



#endif // GEOMETRY_HPP
