#include "lnepch.h"
#include "SafePtr.h"
#include "Engine/Core/Utils/Log.h"

namespace lne
{
#ifdef LNE_DEBUG
void RefCountBase::Capture() const
{
    if (m_Count++ == 0)
        LNE_TRACE("Reference {}: {}", typeid(*this).name(), GetDebugName());
}

uint32_t RefCountBase::Release() const
{
    assert(m_Count > 0);
    m_Count--;
    if (m_Count == 0)
        LNE_TRACE("Delete {}: {}", typeid(*this).name(), GetDebugName());
    return m_Count.load();
}
#endif
}
