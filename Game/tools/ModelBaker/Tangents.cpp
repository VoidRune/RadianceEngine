#include "Tangents.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>

#include "mikktspace/mikktspace.h"

namespace Game
{
	namespace
	{
		struct Context
		{
			const std::vector<ModelVertex>* Vertices = nullptr;
			const std::vector<uint32_t>* Indices = nullptr;
			std::vector<glm::vec4> Corners;
		};

		const ModelVertex& CornerVertex(const SMikkTSpaceContext* context, int face, int corner)
		{
			const Context& data = *static_cast<const Context*>(context->m_pUserData);
			return (*data.Vertices)[(*data.Indices)[size_t(face) * 3 + size_t(corner)]];
		}

		int FaceCount(const SMikkTSpaceContext* context)
		{
			return int(static_cast<const Context*>(context->m_pUserData)->Indices->size() / 3);
		}

		int CornerCount(const SMikkTSpaceContext*, int)
		{
			return 3;
		}

		void Position(const SMikkTSpaceContext* context, float out[], int face, int corner)
		{
			const glm::vec3& position = CornerVertex(context, face, corner).Position;
			out[0] = position.x;
			out[1] = position.y;
			out[2] = position.z;
		}

		void Normal(const SMikkTSpaceContext* context, float out[], int face, int corner)
		{
			const glm::vec3& normal = CornerVertex(context, face, corner).Normal;
			out[0] = normal.x;
			out[1] = normal.y;
			out[2] = normal.z;
		}

		void TexCoord(const SMikkTSpaceContext* context, float out[], int face, int corner)
		{
			const ModelVertex& vertex = CornerVertex(context, face, corner);
			out[0] = vertex.U;
			out[1] = 1.0f - vertex.V;
		}

		void Store(const SMikkTSpaceContext* context, const float tangent[], float sign, int face, int corner)
		{
			Context& data = *static_cast<Context*>(context->m_pUserData);
			data.Corners[size_t(face) * 3 + size_t(corner)] = glm::vec4(tangent[0], tangent[1], tangent[2], sign);
		}

		uint32_t Snorm10(float value)
		{
			return uint32_t(int32_t(std::lround(std::clamp(value, -1.0f, 1.0f) * 511.0f))) & 0x3FFu;
		}
	}

	uint32_t PackTangent(const glm::vec4& tangent)
	{
		const float length = glm::length(glm::vec3(tangent));
		if (!(length > 1e-6f))
			return 0;
		const glm::vec3 t = glm::vec3(tangent) / length;
		return Snorm10(t.x) | (Snorm10(t.y) << 10) | (Snorm10(t.z) << 20) | (tangent.w < 0.0f ? 1u << 30 : 0u);
	}

	void GenerateTangents(std::vector<ModelVertex>& vertices, std::vector<uint32_t>& indices)
	{
		if (indices.size() < 3)
			return;
		Context data{ &vertices, &indices, std::vector<glm::vec4>(indices.size(), glm::vec4(0.0f)) };
		SMikkTSpaceInterface callbacks{};
		callbacks.m_getNumFaces = FaceCount;
		callbacks.m_getNumVerticesOfFace = CornerCount;
		callbacks.m_getPosition = Position;
		callbacks.m_getNormal = Normal;
		callbacks.m_getTexCoord = TexCoord;
		callbacks.m_setTSpaceBasic = Store;
		SMikkTSpaceContext context{ &callbacks, &data };
		genTangSpaceDefault(&context);

		std::vector<ModelVertex> welded;
		std::vector<uint32_t> remapped(indices.size());
		std::unordered_map<uint64_t, uint32_t> seen;
		welded.reserve(vertices.size());
		for (size_t i = 0; i < indices.size(); i++)
		{
			const uint32_t packed = PackTangent(data.Corners[i]);
			const uint64_t key = (uint64_t(indices[i]) << 32) | packed;
			const auto [it, inserted] = seen.emplace(key, uint32_t(welded.size()));
			if (inserted)
			{
				ModelVertex vertex = vertices[indices[i]];
				vertex.Tangent = packed;
				welded.push_back(vertex);
			}
			remapped[i] = it->second;
		}
		vertices = std::move(welded);
		indices = std::move(remapped);
	}
}
