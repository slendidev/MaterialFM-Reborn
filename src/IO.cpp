#include <IO.h>

#include <array>

#ifdef PSP
#include <pspiofilemgr.h>
#endif

namespace MaterialFM
{

auto find_available_mountpoints() -> std::vector<std::string>
{
	std::vector<std::string> detected {};
#ifdef PSP
	static constexpr auto device_array = std::to_array<std::string_view>({
	    // Memory stick
	    "ms0:/",
	    // Internal storage
	    "ef0:/",
	    // System mountpoints
	    "flash0:/",
	    "flash1:/",
	    "flash2:/",
	    "flash3:/",
	    // UMD
	    "disc0:/",
	    "umd0:/",
	});

	for (auto const &device : device_array) {
		SceUID d = sceIoDopen(std::string(device).c_str());
		if (d >= 0) {
			detected.emplace_back(device);
			sceIoDclose(d);
		}
	}
#else
#error "find_available_mountpoints() not supported on this platform! (yet)"
#endif
	return detected;
}

auto is_system_mountpoints(std::string_view const mountpoint) -> bool
{
#ifdef PSP
	return mountpoint.starts_with("flash");
#else
#error "is_system_mountpoint() not supported on this platform! (yet)"
#endif
}

} // namespace MaterialFM
