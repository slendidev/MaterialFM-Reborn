set(MCU_DIR "${CMAKE_CURRENT_LIST_DIR}/../vendor/material-color-utilities")

function(mcu_add_target target_name)
	if(NOT EXISTS "${MCU_DIR}/cpp")
		message(FATAL_ERROR "material-color-utilities not found at ${MCU_DIR}")
	endif()

	file(GLOB_RECURSE mcu_sources CONFIGURE_DEPENDS
		"${MCU_DIR}/cpp/*.cc"
	)

	list(FILTER mcu_sources EXCLUDE REGEX ".*_test\\.cc$")
	list(FILTER mcu_sources EXCLUDE REGEX ".*/test/.*")
	list(FILTER mcu_sources EXCLUDE REGEX ".*/tests/.*")

	add_library(${target_name} STATIC
		${mcu_sources}
	)

	set_property(TARGET ${target_name} PROPERTY CXX_STANDARD 23)

	target_include_directories(${target_name} PUBLIC
		"${MCU_DIR}"
	)

	target_compile_options(${target_name} PRIVATE
		-Wno-sign-conversion
		-Wno-conversion
		-Wno-float-conversion
	)
endfunction()
