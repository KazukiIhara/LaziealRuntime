#pragma once

#include <cstdint>
#include <limits>

namespace LGF {

	// ユーザー側のオブジェクトを識別する世代付きハンドル
	struct ObjectHandle {
		static constexpr uint32_t InvalidIndex = std::numeric_limits<uint32_t>::max();

		uint32_t index = InvalidIndex;
		uint32_t generation{};

		bool IsValid() const {
			return index != InvalidIndex && generation != 0u;
		}
		bool operator==(const ObjectHandle&) const = default;
	};

}
