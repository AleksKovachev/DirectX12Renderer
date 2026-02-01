#ifndef APP_DATA_HPP
#define APP_DATA_HPP

#include "Logger.hpp"
#include "Scene.hpp"

#include <iostream>

namespace Core {
	/// Application-level settings and data.
	struct AppData {
		Logger log{ std::cout }; ///< Logger instance for logging messages.
		Scene scene{};           ///< Holds all scene-related data.
		float deltaTime{};       ///< Keeps time delta between frames.
	};
}

#endif // APP_DATA_HPP