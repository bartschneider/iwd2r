#pragma once

#include <string>

namespace compat {

using PathResolver = std::string (*)(const char*);

// Installs the resolver used by every file access; nullptr restores the identity.
void SetPathResolver(PathResolver resolver);
std::string ResolvePath(const char* path);

}
