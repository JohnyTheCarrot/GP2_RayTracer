#include "Application.h"
#include <iostream>

int main() {
	Application application{};

	try {
		application.Run();
	} catch (const std::exception &exception) {
		std::cerr << exception.what() << '\n';
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
