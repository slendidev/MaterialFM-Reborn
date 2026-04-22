#pragma once

#include <string>
#include <vector>

namespace MaterialFM
{

auto find_available_mountpoints() -> std::vector<std::string>;
auto is_system_mountpoint(std::string_view const mountpoint) -> bool;

} // namespace MaterialFM
