#include "SlabAllocator.h"
#include "Engine/Core/Memory/Alloc.h"
#include "Engine/Core/Utils/_Defines.h"

namespace lne
{
SlabAllocator::SlabAllocator(std::size_t blockSize, std::size_t alignment, std::size_t slabBytes /*= 0*/) : m_BlockSize{ std::max(blockSize, sizeof(void*)) }, m_Alignment{ std::max(alignment, alignof(void*)) },
m_SlabSize{ std::max(slabBytes, sizeof(void*)) }
{
    m_BlockSize = GlobalUtils::AlignmentRoundUp(m_BlockSize, m_Alignment);

    if (m_SlabSize == 0)
    {
        m_SlabSize = GetKiloByte(2);
        if (m_BlockSize > (m_SlabSize >> 4) + sizeof(SlabNode))
            m_SlabSize = m_BlockSize * 16 + sizeof(SlabNode); // arbitrary
    }
    m_SlabSize = RoundUpToVPages(m_SlabSize);
}

SlabAllocator::~SlabAllocator()
{
    while (m_CurrentSlab != nullptr)
    {
        m_SlabSize /= 2;
        SlabNode* tmp = m_CurrentSlab->Next;
        LneVirtualFree(m_CurrentSlab, m_SlabSize);
        m_CurrentSlab = tmp;
    }
}

void* SlabAllocator::Allocate()
{
    if (m_CurrentBlock >= m_EndOfSlab && m_FreedBlockHead == nullptr)
        Grow();
    void* ret = nullptr;

    if (m_FreedBlockHead != nullptr)
    {
        ret = m_FreedBlockHead;
        m_FreedBlockHead = m_FreedBlockHead->Next;
        return ret;
    }
    ret = m_CurrentBlock;
    m_CurrentBlock = m_CurrentBlock + m_BlockSize;
    return ret;
}

void SlabAllocator::Deallocate(void* block)
{
    FreeBlockNode* tmp = (FreeBlockNode*)block;
    tmp->Next = m_FreedBlockHead;
    m_FreedBlockHead = tmp;
}

void SlabAllocator::Grow()
{
    SlabNode* allocation = (SlabNode*)LneVirtualAlloc(m_SlabSize);
    allocation->Next = m_CurrentSlab;
    m_CurrentSlab = allocation;

    m_CurrentBlock = (u8*)m_CurrentSlab + sizeof(SlabNode);
    m_EndOfSlab = (u8*)m_CurrentSlab + m_SlabSize;

    std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(m_CurrentBlock);
    std::size_t mis = addr % m_Alignment;
    if (mis) 
        m_CurrentBlock += (m_Alignment - mis);

    std::size_t usable = (std::size_t)(m_EndOfSlab - m_CurrentBlock);
    std::size_t count = usable / m_BlockSize;

    m_EndOfSlab = m_CurrentBlock + m_BlockSize * count;
    m_SlabSize *= 2;
}
}
