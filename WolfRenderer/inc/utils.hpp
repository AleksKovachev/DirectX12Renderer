#include <format> // format
#include <string> // string

#include "Logger.hpp"
#include <Windows.h> // FAILED, HRESULT

#define CHECK_HR(msg, hr, log) checkHR( std::format("{}\n[{}, {}]", msg, __func__, __LINE__), hr, log );

/// Checks an HRESULT and thread-safely logs an error if needed. Terminates the program.
void checkHR( const std::string& msg, HRESULT hr, Logger& logger, LogLevel level = LogLevel::Critical );

std::string wideStrToUTF8( const std::wstring& );

/// Converts a color value from sRGB to Linear.
float SRGBToLinear( int );

std::wstring ConvertStringToWstring( const std::string& str );