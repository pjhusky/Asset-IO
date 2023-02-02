#pragma once
#include <vector>
#include <string>

namespace FileLoader
{
    struct float2 {
        float x, y;
    };

    struct float3 {
        float x, y, z;
    };
    struct float4 {
        float x, y, z, w;
    };
    
    struct VertexData {
        float3 position;
        float3 normal;
        float2 texCoord;
    };

    struct ObjModel
    {
        using boundingSphere_t = float4;

        void loadModel(const std::string& geometryPath, const std::string& texturePath);

        const std::vector<VertexData>& getVertexBuffer() const { return m_vertexBuffer; }
        const std::vector<uint32_t>&   getIndexBuffer() const { return m_indexBuffer; }
        
        const boundingSphere_t&        getBoundingSphere() const { return m_boundingSphere; }

    private:
        std::vector<VertexData> m_vertexBuffer;
        std::vector<uint32_t>   m_indexBuffer;

        boundingSphere_t        m_boundingSphere; // center.xyz, radius.w
    };
} 
