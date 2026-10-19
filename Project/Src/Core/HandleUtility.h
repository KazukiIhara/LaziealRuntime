#pragma once

#include <cstdint>

namespace LGF::Internal {

	inline uint32_t NextGeneration(uint32_t generation) {
		++generation;
		return generation == 0u ? 1u : generation;
	}

}
