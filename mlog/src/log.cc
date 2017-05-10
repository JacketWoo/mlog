#include "log.h"

#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>

#include <iostream>
#include <utility>


namespace mlog {


struct LogMeta {
	LogMeta();
	~LogMeta();

	std::map<LogLevel, std::string> log_level_prompts;
	std::map<LogLevel, std::string> log_level_strs;
	std::map<LogLevel, FILE*> log_level_files;
	std::map<LogLevel, pthread_mutex_t*> log_level_mutexes_;
	pthread_mutex_t *screen_mutex_;
	
	std::string dir;
	std::string file_prefix;
	bool screen_out;
};

LogMeta::LogMeta() {

	log_level_prompts.insert(std::make_pair(kTrace,  "[TRACE]\t"));
	log_level_prompts.insert(std::make_pair(kDebug,  "[DEBUG]\t"));
	log_level_prompts.insert(std::make_pair(kInfo,   "[INFO ]\t"));
	log_level_prompts.insert(std::make_pair(kWarn,   "[WARN ]\t"));
	log_level_prompts.insert(std::make_pair(kError,  "[ERROR]\t"));
	log_level_prompts.insert(std::make_pair(kDot,    "[ DOT ]\t"));
	log_level_prompts.insert(std::make_pair(kFatal,  "[FATAL]\t"));

	log_level_strs.insert(std::make_pair(kTrace, "trace" ));
	log_level_strs.insert(std::make_pair(kDebug, "debug" ));
	log_level_strs.insert(std::make_pair(kInfo,  "info" ));
	log_level_strs.insert(std::make_pair(kWarn,  "warn" ));
	log_level_strs.insert(std::make_pair(kError, "error" ));
	log_level_strs.insert(std::make_pair(kDot,   "dot"));
	log_level_strs.insert(std::make_pair(kFatal, "fatal"));

	pthread_mutex_t* mutex_p = NULL;
	for (uint32_t level = kTrace; level < kMaxLevel; ++level) {
		mutex_p = new pthread_mutex_t;
		pthread_mutex_init(mutex_p, NULL);
		log_level_mutexes_.insert(std::make_pair(static_cast<LogLevel>(level), mutex_p));
	}
	screen_mutex_ = new pthread_mutex_t;
	pthread_mutex_init(screen_mutex_, NULL);
}

LogMeta::~LogMeta() {

	LogLevel level;
	for (uint32_t idx = kTrace; idx < kMaxLevel; ++idx) {	
		level = static_cast<LogLevel>(idx);
		if (log_level_files[level]) {
			fclose(log_level_files[level]);
		}
		pthread_mutex_destroy(log_level_mutexes_[level]);
		delete log_level_mutexes_[level];
	}
	pthread_mutex_destroy(screen_mutex_);

}

LogLevel work_level;
LogMeta log_meta;


static int DoCreatePath(const char *path, mode_t mode) {
  struct stat st;
  int status = 0;

  if (stat(path, &st) != 0) {
    /* Directory does not exist. EEXIST for race
     * condition */
    if (mkdir(path, mode) != 0 && errno != EEXIST)
      status = -1;
  } else if (!S_ISDIR(st.st_mode)) {
    errno = ENOTDIR;
    status = -1;
  }

  return (status);
}

static int CreatePath(const std::string& path, mode_t mode) {
  char           *pp;
  char           *sp;
  int             status;
  char           *copypath = strdup(path.c_str());

  status = 0;
  pp = copypath;
  while (status == 0 && (sp = strchr(pp, '/')) != 0) {
    if (sp != pp) {
      /* Neither root nor double slash in path */
      *sp = '\0';
      status = DoCreatePath(copypath, mode);
      *sp = '/';
    }
    pp = sp + 1;
  }
  if (status == 0)
    status = DoCreatePath(path.c_str(), mode);
  free(copypath);
  return (status);		
}

void Init(const LogLevel level, const std::string &log_dir, const std::string &file_prefix, const bool screen_out) {
	log_meta.file_prefix = file_prefix;
	work_level = level;
	log_meta.screen_out = screen_out;

	if (access(log_dir.c_str(), F_OK)) {
		CreatePath(log_dir, 0775);	
	}
	std::string log_path = log_dir;
	if (!log_path.empty() && *log_path.rbegin() == '/') {
		log_path.erase(log_path.size()-1);
	}

	std::string file_name;
	FILE *file;
	for (int32_t idx = kTrace; idx < kMaxLevel; ++idx) {
		file_name = file_prefix.empty() ? "" : (file_prefix + "_");
		file_name = log_path + "/" + file_name; 
		file_name += log_meta.log_level_strs[static_cast<LogLevel>(idx)] + ".log";
		file = fopen(file_name.c_str(), "a+");
		if (file == NULL) {
			std::cerr << __FILE__ << ":"  << __LINE__ << ", Open " << file_name << " failed" << std::endl;
			exit(-1);
		}

		log_meta.log_level_files.insert(std::make_pair(static_cast<LogLevel>(idx), file));
	}
}

static LogLevel InterpretLogLevel(const std::string level_str) {
	for (int32_t idx = kInfo; idx < kMaxLevel; ++idx) {
		if (strcasecmp(log_meta.log_level_strs[static_cast<LogLevel>(idx)].c_str(), level_str.c_str()) == 0) {
			return static_cast<LogLevel>(idx);
		}
  }
  return kMaxLevel;
}

std::string GetLevelStr() {
	return log_meta.log_level_strs[work_level];
}

bool SetLogLevel(const std::string &level_str) {
	bool ret = true;
	LogLevel level = InterpretLogLevel(level_str);
	if (level == kMaxLevel) {
		ret = false;
		level = kInfo;
	}

	/*
	 * to be pretected
	 */

	work_level = level;
	return ret;
}

Log::Log(const LogLevel level) {
	self_level_ = level;
	strm_ << log_meta.log_level_prompts[self_level_];
}

Log::~Log() {
	std::string str = strm_.str();
  if (str.empty()) {
    return;
  }

	if (log_meta.screen_out) {
		pthread_mutex_lock(log_meta.screen_mutex_);
		std::cerr << str;
		pthread_mutex_unlock(log_meta.screen_mutex_);
	}

	pthread_mutex_lock(log_meta.log_level_mutexes_[self_level_]);
	if (fwrite(str.c_str(), str.size(), 1, log_meta.log_level_files[self_level_]) < 1) {
		 std::cerr << __FILE__ << ":"  << __LINE__ << ", write " << log_meta.log_level_strs[self_level_] << " failed" << std::endl;
	}
	pthread_mutex_unlock(log_meta.log_level_mutexes_[self_level_]);
}

int32_t Write(const LogLevel level, const std::string& str) {
  int32_t ret = 0;
  if (level < work_level) {
    return ret;
  }
	std::string line(log_meta.log_level_prompts[level]);
	line.append(str);
	if (log_meta.screen_out) {
		pthread_mutex_lock(log_meta.screen_mutex_);
		std::cerr << line;
		pthread_mutex_unlock(log_meta.screen_mutex_);
	}

	pthread_mutex_lock(log_meta.log_level_mutexes_[level]);
	ret = fwrite(line.c_str(), line.size(), 1, log_meta.log_level_files[level]);
  #ifdef DEBUG	
	if (ret != -1) {
		fflush(log_meta.log_level_files[level]);
	}
	#endif
	pthread_mutex_unlock(log_meta.log_level_mutexes_[level]); 
  if (ret < 1) {
		 std::cerr << __FILE__ << ":"  << __LINE__ << ", write " << log_meta.log_level_strs[level] << " failed" << std::endl;
	}
  return ret;
}

int32_t Write(const LogLevel level, const char* format, ...) {
  static char buf[1024];

  int32_t ret = 0;
  if (level < work_level) {
    return ret;
  }
 	memcpy(buf, log_meta.log_level_prompts[level].data(), log_meta.log_level_prompts[level].size());
  if (log_meta.screen_out) {
    va_list vl;
    va_start(vl, format);
		pthread_mutex_lock(log_meta.screen_mutex_);
    fprintf(stderr, format, vl); 
		pthread_mutex_unlock(log_meta.screen_mutex_);
    va_end(vl);
  }
  
  va_list vl;
  va_start(vl, format);
  int32_t len = vsnprintf(buf+log_meta.log_level_prompts[level].size(), sizeof(buf) - log_meta.log_level_prompts[level].size(), format, vl);
  va_end(vl); 
  if (len < 0) {
    return -1;
  }

	pthread_mutex_lock(log_meta.log_level_mutexes_[level]);
	ret = fwrite(buf, len, 1, log_meta.log_level_files[level]);
	pthread_mutex_unlock(log_meta.log_level_mutexes_[level]); 
  if (ret < 1) {
		 std::cerr << __FILE__ << ":"  << __LINE__ << ", write " << log_meta.log_level_strs[level] << " failed" << std::endl;
	}
  
  return ret;
}
}
