#include "config.h"

#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

config_t config = {};

bool loadConfig(const std::string &filename) {
	try {
		std::ifstream file(filename);
		if (!file)
			return false;

		const json document = json::parse(file);
		if (!document.is_object() || !document.contains("devices") ||
				!document["devices"].is_array())
			return false;

		config_t loadedConfig;
        uint8_t configIdCounter = 0;
		for (const json &device : document["devices"]) {
			if (!device.is_object() || !device.contains("vendor_id") ||
					!device.contains("product_id"))
				return false;

			const auto readId = [](const json &value) -> uint16_t {
				unsigned long id;
				if (value.is_string())
					id = std::stoul(value.get<std::string>(), nullptr, 0);
				else if (value.is_number_unsigned())
					id = value.get<unsigned long>();
				else
					throw std::invalid_argument("device ID must be a number or string");

				if (id > std::numeric_limits<uint16_t>::max())
					throw std::out_of_range("device ID is too large");
				return static_cast<uint16_t>(id);
			};

			device_config_t deviceConfig = {.configId = configIdCounter};
			deviceConfig.vendor_id = readId(device["vendor_id"]);
			deviceConfig.product_id = readId(device["product_id"]);
			deviceConfig.brightnessCorrection[0] = 255;
				if (device.contains("brightnessCorrection")) {
					const json &correction = device["brightnessCorrection"];
					if (!correction.is_array() || correction.size() > 256)
						return false;

					for (std::size_t index = 0; index < correction.size(); ++index) {
						if (!correction[index].is_number_unsigned() ||
								correction[index].get<unsigned int>() >
										std::numeric_limits<uint8_t>::max())
							return false;
						deviceConfig.brightnessCorrection[index] =
								correction[index].get<uint8_t>();
					}
				}
			loadedConfig.devices.push_back(deviceConfig);
            configIdCounter++;
		}

		config = std::move(loadedConfig);
		return true;
	} catch (const std::exception &) {
		config.devices.clear();
		return false;
	}
}
