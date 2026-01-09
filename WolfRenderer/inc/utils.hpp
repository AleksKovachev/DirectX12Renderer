#include <format> // format
#include <iostream> // endl
#include <string> // string
#include <Windows.h> // FAILED

#include "Logger.hpp" // Logger


/// Checks an HRESULT and thread-safely logs an error if needed. Terminates the program.
void checkHR(
	const std::string& message,
	HRESULT hr,
	Logger& logger,
	LogLevel level = LogLevel::Critical )
{
	if ( FAILED( hr ) && level >= logger.GetLevel() ) {
		std::lock_guard<std::mutex> lock( logger.GetMutex() );
		std::string msg{ std::format( "{} HRESULT: {:#x}", message, hr ) };
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
