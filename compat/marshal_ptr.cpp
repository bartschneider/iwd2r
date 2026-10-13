#include "compat/marshal_ptr.h"

#include <mutex>
#include <unordered_map>
#include <vector>

namespace iwd2r {

namespace {

    std::mutex g_mutex;
    std::vector<const void*> g_pointers; // handle - 1 -> pointer
    std::unordered_map<const void*, uint32_t> g_handles;

} // namespace

uint32_t MarshalPtr(const void* p)
{
    if (p == nullptr)
        return 0;
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_handles.find(p);
    if (it != g_handles.end())
        return it->second;
    g_pointers.push_back(p);
    const uint32_t handle = static_cast<uint32_t>(g_pointers.size());
    g_handles.emplace(p, handle);
    return handle;
}

void* UnmarshalPtr(uint32_t handle)
{
    if (handle == 0)
        return nullptr;
    std::lock_guard<std::mutex> lock(g_mutex);
    if (handle > g_pointers.size())
        return nullptr;
    return const_cast<void*>(g_pointers[handle - 1]);
}

void ResetMarshalPtrs()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_pointers.clear();
    g_handles.clear();
}

} // namespace iwd2r
