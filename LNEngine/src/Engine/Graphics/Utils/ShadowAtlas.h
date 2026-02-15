#pragma once
#include "Engine/Core/Utils/Defines.h"
#include "Engine/Core/SafePtr.h"
#include "Engine/ECS/Types.h"
#include "Engine/Core/DataStructures/FlatHashClasses.h"
#include "Engine/Core/DataStructures/ObjectPool.h"

namespace lne
{
class GfxContext;

using ShadowAtlasChunkID = s32;

struct ShadowAtlasChunk
{
    ShadowAtlasChunkID  ID;
    u32                 Dimension;
    glm::uvec2          Offset;
};

constexpr ShadowAtlasChunk INVALID_SHADOW_ATLAS_CHUNK{ -1, 0xffffffff, { 0xffffffff, 0xffffffff } };

/**
 * Logical reprentation of a shadow atlas.
 */
class ShadowAtlas
{
struct QuadNode
{
    ObjectPoolHandle    OwnHandle = INVALID_OBJECT_POOL_HANDLE; // because I'm lazy
    glm::uvec2          Offset{};
    u32                 Dimension{};
    bool                Taken{ false };

    bool                HasChildren{ false };
    QuadNode*           Parent{};
    QuadNode*           Children[4]{};

};

public:
    /**
     * Constructor for the ShadowAtlas class. Expects to allocate
     * squared textures with power of 2 dimensions. Only handle the logical
     * part of the allocation and will now really have a texture in it.
     * @param dimension is the total dimension of the shadow atlas. It must
     * be a power of two and if it isn't, it will be rounded to the higher
     * power of two.
     * @param minSizeLimit is the minimum size limit of a texture in the atlas.
     * It must be a power of two and if it isn't, it will be rounded to
     * the higher power of two.
     */
    ShadowAtlas(u32 dimension, u32 minSizeLimit);

    /**
     * Will TRY to reserve a portion of the atlas.
     * @param requestedSize size of the portion of the atlas that you 
     * want to reserve. Expects that it's a power of 2 and if it isn't,
     * it will be ceilled to the next power of 2.
     * @return an AtlasChunk that contains the dimension of the Chunk
     * (normally it's the requested size except if it wasn't a power of
     * 2, a 2D offset in the texture as well as an AtlasChunkID which is
     * used to Release the chunk.
     */
    ShadowAtlasChunk    Reserve(u32 requestedSize);
    /**
     * Will TRY to release a ShadowAtlasChunkID that's found in the 
     * ShadowAtlasChunk. If it's an invalid ID, it will release nothing
     */
    void                Release(ShadowAtlasChunkID id);
    u32                 GetAtlasDimension() { return m_Dimension; }
    u32                 GetMinSizeLimit() { return m_MinSizeLimit; }

private:
    u32                     m_Dimension;
    u32                     m_MinSizeLimit;
    ObjectPool<QuadNode>    m_NodePool;
    QuadNode*               m_Root{ nullptr };

    struct TakenNodeCacheElem
    {
        union
        {
            s32         NextFreeIndex = 0;
            QuadNode*   Node;
        };
        enum Type : u8
        {
            eFreeIndex,
            eNode
        };
        Type            Type;
    };
    std::vector<TakenNodeCacheElem>  m_TakenNodes; // pretty much a cache for faster removal
    s32                     m_NextFreeTakenElem{ -1 };

private:
    QuadNode*           FindFittingChunk(u32 requestedSize, QuadNode* currentNode);
    QuadNode*           InitQuadNode(glm::uvec2 offset, u32 dimension, QuadNode* parent);
    void                Release(QuadNode* node);
};
}

