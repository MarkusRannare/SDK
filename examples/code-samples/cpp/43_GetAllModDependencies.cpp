#include "modio.h"
#include <iostream>

int main(void)
{
	modio::Instance modio_instance(MODIO_ENVIRONMENT_TEST, 7, "e91c01b8882f4affeddd56c96111977b");

	volatile static bool finished = false;

	auto wait = [&]() {
		while (!finished)
		{
			modio_instance.sleep(10);
			modio_instance.process();
		}
	};

	auto finish = [&]() {
		finished = true;
	};

	u32 mod_id;
	std::cout << "Enter the mod id: " << std::endl;
	std::cin >> mod_id;

	// We request the list of dependencies by providing the Mod's id
	modio_instance.getAllModDependencies(mod_id, [&](const modio::Response &response, std::vector<modio::Dependency> dependencies) {
		std::cout << "Get metadata response: " << response.code << std::endl;
		if (response.code == 200)
		{
			std::cout << "Listing Dependencies:" << std::endl;
			std::cout << "=====================" << std::endl;
			for (auto &dependency : dependencies)
			{
				std::cout << dependency.mod_id << std::endl;
			}
		}
		finish();
	});

	wait();

	std::cout << "Process finished" << std::endl;

	return 0;
}
