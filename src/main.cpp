#include "Application.h"
#include <iostream>

int main() {
	std::array<roing::ModelLoadData, 1> modelLoadDataEntries{
	        {"medieval_building.obj",
	         glm::rotate(
	                 glm::translate(glm::mat4{1.f}, glm::vec3{0, -2, -10.f}), glm::radians(90.0f), glm::vec3{0, 1, 0}
	         )}
	};


	roing::Application application{
	        [&](const roing::vk::Device &device, const roing::vk::CommandPool &commandPool,
	            VkPhysicalDevice physicalDevice) {
		        std::vector<roing::Model> models{};
		        models.reserve(modelLoadDataEntries.size());

		        for (const auto &loadData: modelLoadDataEntries) {
			        models.emplace_back(roing::Model::LoadModel(device, commandPool, physicalDevice, loadData.fileName)
			        );
		        };

		        return models;
	        },
	        [&](const std::vector<roing::Model> &models) {
		        std::vector<roing::ModelInstance> instances{};

		        for (size_t i = 0; i < models.size(); ++i) {
			        const auto &model{models[i]};
			        const auto  modelLoadData{modelLoadDataEntries[i]};
			        instances.emplace_back(&model, modelLoadData.transform, i);
		        }

		        return instances;
	        }
	};

	try {
		application.Run();
	} catch (const std::exception &exception) {
		std::cerr << exception.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
