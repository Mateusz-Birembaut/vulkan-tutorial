#pragma once

#include <fstream>
#include <iostream>
#include <vector>

namespace FileReader {
	std::vector<char> readSPV(const std::string& filename) {
		std::ifstream file(filename, std::ios::ate | std::ios::binary);
		// ios::ate => on se place a la fin, on peut recup le nombre d'éléments a mettre dans le buffer

		if (!file.is_open()) {
			std::cout << filename << std::endl;
			throw std::runtime_error("failed to open file!");
		}

		size_t fileSize = static_cast<size_t>(file.tellg());
		std::vector<char> buffer(fileSize);

		file.seekg(0);
		file.read(buffer.data(), fileSize);
		file.close();

		return buffer;
	}
} // namespace FileReader
