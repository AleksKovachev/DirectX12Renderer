#include <chrono> // high_resolution_clock, microseconds
#include <iostream> // cout, endl

#include "Logger.hpp" // LogLevel
#include "Renderer.hpp"

int main() {
	Renderer renderer{};
	renderer.SetLoggerMinLevel( LogLevel::Error );

	std::chrono::high_resolution_clock::time_point start{
		std::chrono::high_resolution_clock::now() };

	const float redColor[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
	renderer.PrepareForRendering();
	renderer.RenderFrame( redColor );
	renderer.WriteImageToFile( "output.ppm" );

	std::chrono::high_resolution_clock::time_point stop{
		std::chrono::high_resolution_clock::now() };

	std::chrono::microseconds duration{
		std::chrono::duration_cast<std::chrono::microseconds>( stop - start ) };

	const double seconds{ duration.count() / 1'000'000.0 };
	std::cout << "Execution time: " << seconds << " seconds." << std::endl;

	return 0;
}