#include "lnepch.h"
#include "SafePtr.h"
#include "Engine/Core/Utils/Log.h"

namespace lne
{
#ifdef LNE_DEBUG
void RefCountBase::Capture() const
{
    ++m_Count;
    LNE_TRACE("Capture {0}: {1}", typeid(*this).name(), m_Count.load());
}

uint32_t RefCountBase::Release() const
{
    assert(m_Count > 0);
    --m_Count;
    LNE_TRACE("Release {0}: {1}", typeid(*this).name(), m_Count.load());
    return m_Count.load();
}
#endif
}
