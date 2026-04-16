#pragma once

#include <string>
#include <vector>

namespace MaterialFM
{

auto find_available_partitions() -> std::vector<std::string>;
auto is_system_partition(std::string_view const partition) -> bool;

} // namespace MaterialFM
