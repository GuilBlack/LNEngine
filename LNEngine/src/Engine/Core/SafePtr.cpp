#include "lnepch.h"
#include "SafePtr.h"
#include "Engine/Core/Utils/Log.h"
//#define SAFEPTR_DEBUG

namespace lne
{
#ifdef LNE_DEBUG
void RefCountBase::Capture() const
{
    if (m_Count.fetch_add(1, std::memory_order_acq_rel) == 0)
    {
#ifdef SAFEPTR_DEBUG
        LNE_TRACE("Reference {}: {}", typeid(*this).name(), GetDebugName());
#endif // LNE_DEBUG

    }
}

uint32_t RefCountBase::Release() const
{
    assert(m_Count.load(std::memory_order_acquire) > 0);
    uint32_t newCount = m_Count.fetch_sub(1, std::memory_order_acq_rel) - 1;
    if (newCount == 0)
    {
#ifdef SAFEPTR_DEBUG
        LNE_TRACE("Delete {}: {}", typeid(*this).name(), GetDebugName());
#endif // 

    }
    return newCount;
}
#endif
}
