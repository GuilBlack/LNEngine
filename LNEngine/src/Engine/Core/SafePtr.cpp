#include "lnepch.h"
#include "SafePtr.h"
#include "Engine/Core/Utils/Log.h"
#include "Engine/Core/DataStructures/FlatHashClasses.h"
//#define SAFEPTR_DEBUG

namespace lne
{

FlatHashMap<const void*, std::string> g_RefCountDebugNames{};
std::mutex g_RefCountDebugNamesMutex;

#ifdef LNE_DEBUG
void RefCountBase::Capture() const
{
    if (m_Count.fetch_add(1, std::memory_order_acq_rel) == 0)
    {
#ifdef SAFEPTR_DEBUG
        LNE_TRACE("Reference {}: {}", typeid(*this).name(), GetDebugName());
#endif // LNE_DEBUG
        std::lock_guard<std::mutex> lock(g_RefCountDebugNamesMutex);
        if (g_RefCountDebugNames.find((const void*)this) != g_RefCountDebugNames.end())
            LNE_ERROR("Reference count debug name already exists for {}: {}", typeid(*this).name(), GetDebugName());
        g_RefCountDebugNames[(const void*)this] = std::string(GetDebugName());
    }
}

u32 RefCountBase::Release() const
{
    assert(m_Count.load(std::memory_order_acquire) > 0);
    u32 newCount = m_Count.fetch_sub(1, std::memory_order_acq_rel) - 1;
    if (newCount == 0)
    {
#ifdef SAFEPTR_DEBUG
        LNE_TRACE("Delete {}: {}", typeid(*this).name(), GetDebugName());
#endif //
        std::lock_guard<std::mutex> lock(g_RefCountDebugNamesMutex);
        if (g_RefCountDebugNames.find((const void*)this) == g_RefCountDebugNames.end())
            LNE_ERROR("Reference count debug name not found for {}: {}", typeid(*this).name(), GetDebugName());
        g_RefCountDebugNames.erase((const void*)this);
    }
    return newCount;
}
#endif
}
