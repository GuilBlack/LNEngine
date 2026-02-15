#include "lnepch.h"
#include "ShadowAtlas.h"
#include "Core/Utils/Log.h"

namespace lne
{
ShadowAtlas::ShadowAtlas(u32 dimension, u32 minSizeLimit)
    : m_Dimension(dimension), m_MinSizeLimit(minSizeLimit)
{
    if (GlobalUtils::IsPowOfTwo(m_Dimension) == false)
    {
        LNE_WARN("ShadowAtlas dimensions should be a power of two for better packing efficiency. Rounding up to the next power of two.");
        m_Dimension = (u32)GlobalUtils::NextPowOfTwo(m_Dimension);
    }
    if (GlobalUtils::IsPowOfTwo(m_MinSizeLimit) == false)
    {
        LNE_WARN("ShadowAtlas min size limit should be a power of two for better packing efficiency. Rounding up to the next power of two.");
        m_MinSizeLimit = (u32)GlobalUtils::NextPowOfTwo(m_MinSizeLimit);
    }
    if (m_MinSizeLimit <= m_Dimension)
    {
        LNE_WARN("ShadowAtlas min size limit should be less than or equal to the atlas dimensions. Making the min size limit = to the dimension.");
    }
    if (m_MinSizeLimit > m_Dimension)
    {
        LNE_WARN("The min size limit was > than the dimension. Clamping it to dimension.");
        m_MinSizeLimit = m_Dimension;
    }
    m_Root = InitQuadNode({ 0,0 }, m_Dimension, nullptr);
}

lne::ShadowAtlasChunk ShadowAtlas::Reserve(u32 requestedSize)
{
    if (GlobalUtils::IsPowOfTwo(requestedSize) == false)
    {
        LNE_WARN("ShadowAtlas requested size should be a power of two for better packing efficiency. Rounding up to the next power of two.");
        requestedSize = (u32)GlobalUtils::NextPowOfTwo(requestedSize);
    }
    if (requestedSize < m_MinSizeLimit)
    {
        LNE_WARN("ShadowAtlas requested size is smaller than the minimum size limit.");
        return INVALID_SHADOW_ATLAS_CHUNK;
    }
    if (requestedSize > m_Dimension)
        return INVALID_SHADOW_ATLAS_CHUNK;

    QuadNode* node = FindFittingChunk(requestedSize, m_Root);
    if (node == nullptr)
        return INVALID_SHADOW_ATLAS_CHUNK;

    s32 elemIndex;
    if (m_NextFreeTakenElem == -1)
    {
        m_TakenNodes.emplace_back(TakenNodeCacheElem{ 
            .Node = node, 
            .Type = TakenNodeCacheElem::eNode
        });
        elemIndex = (s32)m_TakenNodes.size() - 1;
    }
    else
    {
        elemIndex                           = m_NextFreeTakenElem;
        m_NextFreeTakenElem                 = m_TakenNodes[elemIndex].NextFreeIndex;
        m_TakenNodes[elemIndex]             = TakenNodeCacheElem{ .Node = node, .Type = TakenNodeCacheElem::eNode };
    }

    return ShadowAtlasChunk{ elemIndex, requestedSize, node->Offset };
}

void ShadowAtlas::Release(ShadowAtlasChunkID id)
{
    if (id < 0 || id >= m_TakenNodes.size() || m_TakenNodes[id].Type != TakenNodeCacheElem::eNode)
    {
        LNE_WARN("THIS ID IS INVALID. NO RELEASE WILL BE DONE!");
        return;
    }
    QuadNode* node = m_TakenNodes[id].Node;
    m_TakenNodes[id] = TakenNodeCacheElem{ .NextFreeIndex = m_NextFreeTakenElem, .Type = TakenNodeCacheElem::eFreeIndex };
    m_NextFreeTakenElem = id;
    Release(node);
}

lne::ShadowAtlas::QuadNode* ShadowAtlas::FindFittingChunk(u32 requestedSize, QuadNode* currentNode)
{
    bool isValid = currentNode->Dimension == requestedSize
                   && currentNode->HasChildren == false
                   && currentNode->Taken == false;

    if (isValid)
    {
        currentNode->Taken = true;
        return currentNode;
    }

    // can't go down the tree further since it's the limit of the requestedSize
    // or it's the minimum size limit in which case, we can't go down further
    if (requestedSize >= currentNode->Dimension 
        || currentNode->Dimension == m_MinSizeLimit 
        || currentNode->Taken)
        return nullptr;

    glm::uvec2 currentOffset = { currentNode->Offset };
    u32 childDimension = currentNode->Dimension >> 1;
    for (u32 i = 0; i < 4; ++i)
    {
        // if we go in this if, it's sure that the allocation will succeed
        if (currentNode->Children[i] == nullptr)
        {
            currentNode->HasChildren = true;
            currentNode->Children[i] = InitQuadNode(
                {
                    currentOffset.x + childDimension * (i % 2),
                    currentOffset.y + childDimension * (i / 2)
                },
                childDimension,
                currentNode
            );
        }
        QuadNode* node = FindFittingChunk(requestedSize, currentNode->Children[i]);
        if (node != nullptr) // found it
            return node;
    }

    return nullptr;
}

lne::ShadowAtlas::QuadNode* ShadowAtlas::InitQuadNode(glm::uvec2 offset, u32 dimension, QuadNode* parent)
{
    ObjectPoolHandle handle = m_NodePool.Allocate();
    QuadNode* newNode = m_NodePool.Access(handle);
    newNode->OwnHandle = handle;
    newNode->Offset = offset;
    newNode->Dimension = { dimension };
    newNode->Parent = parent;
    return newNode;
}

void ShadowAtlas::Release(QuadNode* node)
{
    QuadNode* parentNode = node->Parent;
    bool parentStillHasChildren = false;
    for (u32 i = 0; i < 4; ++i)
    {
        if (parentNode->Children[i] == node)
            parentNode->Children[i] = nullptr;

        if (parentNode->Children[i] != nullptr)
            parentStillHasChildren = true;
    }
    if (parentStillHasChildren == false && parentNode != m_Root)
        Release(parentNode);
    m_NodePool.Deallocate(node->OwnHandle);
}
}
