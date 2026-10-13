#define NOMINMAX
#include "compat/path_hook.h"

namespace compat {

namespace {

    std::string identityResolver(const char* path) { return path ? path : ""; }

    PathResolver g_resolver = identityResolver;

}

void SetPathResolver(PathResolver resolver)
{
    g_resolver = resolver ? resolver : identityResolver;
}

std::string ResolvePath(const char* path)
{
    return g_resolver(path);
}

}
