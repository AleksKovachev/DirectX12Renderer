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

std::wstring ConvertStringToWstring( const std::string& str ) {
	if ( str.empty() )
		return L"";

	// Get the size required for the wide string (including null terminator).
	int wSize{ MultiByteToWideChar( CP_UTF8, 0, str.c_str(), -1, nullptr, 0 ) };

	if ( wSize == 0 )
		throw std::runtime_error( "Failed to compute wide string size." );

	// Allocate a std::wstring of the required size (minus 1 for null terminator).
	std::wstring result( wSize - 1, L'\0' );

	// Perform the conversion.
	int converted = MultiByteToWideChar( CP_UTF8, 0, str.c_str(), -1, &result[0], wSize);

	if ( converted == 0 )
		throw std::runtime_error( "Failed to convert UTF-8 string to wide string." );

	return result;
}
