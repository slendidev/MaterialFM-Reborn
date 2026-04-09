#pragma once

#include <smath.hpp>

namespace Engine
{

template<typename T = float> struct Rect
{
	smath::Vec<2, T> position {};
	smath::Vec<2, T> size {};
};

} // namespace Engine
