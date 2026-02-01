#include <chrono> // high_resolution_clock, microseconds
#include <iostream> // cout, endl

#include "Logger.hpp" // LogLevel
#include "Renderer.hpp"

int main() {
	Core::AppData appData{};
	Core::WolfRenderer renderer{ appData };
	renderer.SetLoggerMinLevel( LogLevel::Error );

	std::chrono::high_resolution_clock::time_point start{
		std::chrono::high_resolution_clock::now() };

	//renderer.PrepareForRendering();
	RT::CameraInput inputs{};
	renderer.RenderFrame( inputs );
	renderer.WriteImageToFile( "output.ppm" );

	std::chrono::high_resolution_clock::time_point stop{
		std::chrono::high_resolution_clock::now() };

	std::chrono::microseconds duration{
		std::chrono::duration_cast<std::chrono::microseconds>( stop - start ) };

	const double seconds{ duration.count() / 1'000'000.0 };
	std::cout << "Execution time: " << seconds << " seconds." << std::endl;

	return 0;
}