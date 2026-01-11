#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <chrono> // system_clock
#include <format> // format
#include <iostream> // endl
#include <mutex> // mutex, lock_guard
#include <ostream> // ostream
#include <string> // string
#include <windows.h> // HRESULT


/// Logging level. Can't use all caps for members because of conflict with Windows.h macros.
enum class LogLevel {
	Debug,
	Info,
	Warning,
	Error,
	Critical
};


class Logger {
public:
	explicit Logger( std::ostream& os )
		: m_os{ os },
#ifdef _DEBUG
		m_minLevel{ LogLevel::Debug }
#else
		m_minLevel{ LogLevel::Info }
#endif // DEBUG
	{}

	explicit Logger( std::ostream& os, LogLevel minLevel )
		: m_os{ os }, m_minLevel{ minLevel } {}

	/// Sets the minimum logging level. Messages below this level will be ignored.
	void SetMinLevel( LogLevel minLevel ) {
		m_minLevel = minLevel;
	}

	/// Thread-safely logs a message to the output stream.
	void operator()( const std::string& message, LogLevel level = LogLevel::Debug ) {
		if ( level >= m_minLevel ) {
			std::lock_guard<std::mutex> lock( m_mutex );
			m_os << FormatLog( level, message ) << std::endl;
		}
	}

	LogLevel GetLevel() const {
		return m_minLevel;
	}

	std::ostream& GetStream() const {
		return m_os;
	}

	std::mutex& GetMutex() {
		return m_mutex;
	}

	/// Static convenience method to log a message without creating a Logger instance.
	static void log(
		const std::string& message,
		std::ostream& outStream,
		LogLevel level = LogLevel::Info
	) {
		Logger logger{ outStream };
		logger( message, level );
	}

	std::string FormatLog( LogLevel level, const std::string& message ) {
		std::string levelStr;
		switch ( level ) {
			case LogLevel::Debug:
				levelStr = "DEBUG";
				break;
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

private:
	std::ostream& m_os;
	std::mutex m_mutex;
	LogLevel m_minLevel;
};


#endif // LOGGER_HPP