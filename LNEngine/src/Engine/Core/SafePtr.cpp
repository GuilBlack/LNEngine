#include "lnepch.h"
#include "SafePtr.h"
#include "Engine/Core/Utils/Log.h"

namespace lne
{
#ifdef LNE_DEBUG
void RefCountBase::Capture() const
{
    if (m_Count++ == 0)
        LNE_TRACE("Reference {0}", typeid(*this).name());
}

uint32_t RefCountBase::Release() const
{
    assert(m_Count > 0);
    if (m_Count-- == 0)
        LNE_TRACE("Delete {0}", typeid(*this).name());
    return m_Count.load();
}
#endif
}
