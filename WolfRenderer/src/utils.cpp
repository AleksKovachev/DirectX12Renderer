#include "Logger.hpp" // Logger
#include "utils.hpp"

#include <format> // format
#include <iostream> // endl

void checkHR( const std::string& msg, HRESULT hr, Logger& logger, LogLevel level ) {
	if ( FAILED( hr ) && level >= logger.GetLevel() ) {
		std::lock_guard<std::mutex> lock( logger.GetMutex() );
		std::string msg{ std::format( "{} HRESULT: {:#x}", msg, hr ) };
		logger.GetStream() << logger.FormatLog( level, msg ) << std::endl;

		throw;
	}
}

std::string wideStrToUTF8( const std::wstring& wide ) {
	int size_needed = WideCharToMultiByte(
		CP_UTF8, 0, wide.data(), (int)wide.size(), nullptr, 0, nullptr, nullptr );
	std::string utf8( size_needed, 0 );
	WideCharToMultiByte( CP_UTF8, 0, wide.data(), (int)wide.size(),
		utf8.data(), size_needed, nullptr, nullptr );
	return utf8;
}

float SRGBToLinear( int value ) {
	return (value <= 0.04045f) ? value / 12.92f : std::pow( (value + 0.055f) / 1.055f, 2.4f );
}
