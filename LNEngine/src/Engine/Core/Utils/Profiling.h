#pragma once

#if defined(TRACY_ENABLE) && defined(LNE_DEBUG)
#   define LNE_PROFILE_SCOPE(name) ZoneScopedN(name);
#   define LNE_PROFILE_SCOPE_C(name, color) ZoneScopedNC(name, color);
#   define LNE_PROFILE_FUNCTION() LNE_PROFILE_SCOPE(__FUNCTION__);
#   define LNE_PROFILE_FUNCTION_C(color) LNE_PROFILE_SCOPE_C(__FUNCTION__, color);
#   define LNE_PROFILE_SCOPE_STR(name) ZoneScoped; ZoneName(name.c_str(), name.size());
#   define LNE_PROFILE_SCOPE_STR_C(name, color) ZoneScoped; ZoneName(name.c_str(), name.size()); ZoneColor(color);
#   define LNE_PROFILE_FRAME FrameMark;
#   define SET_PROFILING_THREAD_NAME(name) tracy::SetThreadName(name);
#else
#   define LNE_PROFILE_SCOPE(name)
#   define LNE_PROFILE_SCOPE_C(name, color)
#   define LNE_PROFILE_FUNCTION()
#   define LNE_PROFILE_FUNCTION_C(color)
#   define LNE_PROFILE_SCOPE_STR(name)
#   define LNE_PROFILE_SCOPE_STR_C(name, color)
#   define LNE_PROFILE_FRAME
#   define SET_PROFILING_THREAD_NAME(name)
#endif
