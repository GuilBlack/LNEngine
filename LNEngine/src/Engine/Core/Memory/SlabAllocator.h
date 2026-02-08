#pragma once
#include "Engine/Core/Utils/Defines.h"

namespace lne
{
class SlabAllocator
{
public:
    SlabAllocator(std::size_t blockSize, std::size_t alignment, std::size_t slabBytes = 0);

    ~SlabAllocator();

    /**
     * Allocates a block of memory of the blockSize that you inputed into the constructor.
     * This block size is aligned by the alignment that you gave.
     * @return a void ptr of at least the block size that you gave with the correct alignment.
     */
    void*               Allocate();

    /**
     * Deallocates a memory block. !!!! Expects that you handled the destruction
     * logic of your object before returning the block to the allocator.
     * @param block is a block of memory that was returned by the Allocate method.
     */
    void                Deallocate(void* block);

private:
    struct SlabNode
    {
        SlabNode* Next;
    };

    struct FreeBlockNode
    {
        FreeBlockNode* Next;
    };

    std::size_t         m_BlockSize;
    std::size_t         m_Alignment;
    std::size_t         m_SlabSize;
    SlabNode*           m_CurrentSlab{};
    u8*                 m_EndOfSlab{};
    FreeBlockNode*      m_FreedBlockHead{};
    u8*                 m_CurrentBlock{};
    std::mutex          m_Mutex;

    void                Grow();
};
}
