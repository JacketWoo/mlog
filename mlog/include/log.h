#ifndef LOG_H
#define LOG_H

#include <pthread.h>
#include <stdint.h>

#include <string>
#include <sstream>
#include <map>
#include <memory>

namespace mlog {

enum LogLevel {
	kTrace,
	kDebug,
	kInfo,
	kWarn,
	kError,
	kDot,
	kFatal,
	kMaxLevel
};

extern LogLevel work_level;
extern pthread_mutex_t mlock;

class Log {
public:
	Log(const LogLevel level);
  ~Log();

	std::stringstream& strm() {
		return strm_;
	}

private:
	std::stringstream strm_;
	LogLevel self_level_;
};

void Init(const LogLevel level = kInfo, const std::string &log_dir = "./log", const std::string &file_prefix = "", const bool screen_out = true);
void BackupAndSwitchLog(const std::string& date=""); 
bool SetLogLevel(const std::string &level_str);
bool SetLogDir(const std::string &level_dir);

std::string GetLevelStr();
int32_t Write(const LogLevel level, const std::string& str);
int32_t Write(const LogLevel level, const char* format, ...);
}

#define MTRACE   mlog::kTrace
#define MDEBUG   mlog::kDebug
#define MINFO    mlog::kInfo
#define MWARN    mlog::kWarn
#define MERROR   mlog::kError
#define MDOT     mlog::kDot
#define MFATAL   mlog::kFatal

#define LOG(level) \
	if (level >= mlog::work_level) \
		mlog::Log(level).strm()
#endif
