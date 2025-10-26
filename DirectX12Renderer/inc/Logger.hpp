#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <chrono> // system_clock
#include <format> // format
#include <iostream> // endl
#include <mutex> // mutex, lock_guard
#include <ostream> // ostream
#include <string> // string


// Logging level. Can't use all caps for members because of conflict with Windows.h macros.
enum class LogLevel {
	Info,
	Warning,
	Error,
	Critical
};


class Logger {
public:
	explicit Logger( std::ostream& os )
		: m_os{ os }, m_minLevel{ LogLevel::Info } {}

	void setMinLevel( LogLevel level ) {
		m_minLevel = level;
	}

	void operator()( const std::string& message, LogLevel level = LogLevel::Info ) {
		if ( level >= m_minLevel ) {
			std::lock_guard<std::mutex> lock( m_mutex );
			m_os << formatLog( level, message ) << std::endl;
		}
	}

	static void log(
		const std::string& message,
		std::ostream& outStream,
		LogLevel level = LogLevel::Info
	) {
		Logger logger{ outStream };
		logger( message, level );
	}

private:
	std::ostream& m_os;
	std::mutex m_mutex;
	LogLevel m_minLevel;

	std::string formatLog( LogLevel level, const std::string& message ) {
		std::string levelStr;
		switch ( level ) {
			case LogLevel::Info:
				levelStr = "INFO";
				break;
			case LogLevel::Warning:
				levelStr = "WARNING";
				break;
			case LogLevel::Error:
				levelStr = "ERROR";
				break;
			case LogLevel::Critical:
				levelStr = "CRITICAL";
				break;
		}

		std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
		std::string timeStr{ std::format( "{:%d.%m.%Y %H:%M:%S}", now ) };

		return "[" + levelStr + "] " + "[" + timeStr + "] " + message;
	}
};


#endif // LOGGER_HPP