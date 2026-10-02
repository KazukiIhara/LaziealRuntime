#pragma once

namespace LGF {

	using MainFunction = void(*)();

	int Run(MainFunction mainFunction);

}

void Main();
