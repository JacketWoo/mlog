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

struct MlogFile {
  MlogFile(FILE* f) : fp(f) {}
  virtual ~MlogFile() {
    if (fp) {
      fclose(fp);
    }
  }
  FILE* fp;
};

struct LogMeta {
	LogMeta();
	~LogMeta();

	std::map<LogLevel, std::string> log_level_prompts;
	std::map<LogLevel, std::string> log_level_strs;

  pthread_rwlock_t *log_level_files_rwlock;
	std::map<LogLevel, std::shared_ptr<MlogFile> > log_level_files;
	
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

  pthread_rwlockattr_t attr;
  pthread_rwlockattr_init(&attr);
  pthread_rwlockattr_setkind_np(&attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
  log_level_files_rwlock = new pthread_rwlock_t;
  pthread_rwlock_init(log_level_files_rwlock, &attr);
}

LogMeta::~LogMeta() {
  pthread_rwlock_destroy(log_level_files_rwlock);
}

LogLevel work_level;
LogMeta log_meta;

static inline void  WriteFormat(FILE* file, const char* format, ...) {
  char buf[1024]; 
  va_list vl;
  va_start(vl, format);
  int32_t len = vsnprintf(buf, sizeof(buf), format, vl); 
  va_end(vl);
  if (len < 0) {
    return;
  }
  fwrite(buf, strlen(buf), 1, file); 
}

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

  return status;
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
  log_meta.dir = log_path;

	std::string file_path, half_file_path;
  half_file_path = log_path + "/" + (file_prefix.empty() ? "" : (file_prefix + "_"));
	FILE *file;
	for (int32_t idx = kTrace; idx < kMaxLevel; ++idx) {
		file_path = half_file_path + log_meta.log_level_strs[static_cast<LogLevel>(idx)] + ".log";
		file = fopen(file_path.c_str(), "a+");
		if (file == NULL) {
      WriteFormat(stderr, "%s:%u, open %s failed\n", __FILE__, __LINE__, file_path.c_str());
			exit(-1);
		}
    std::shared_ptr<MlogFile> ptr = std::make_shared<MlogFile>(file);
		log_meta.log_level_files.insert(std::make_pair(static_cast<LogLevel>(idx), ptr));
	}
}

void BackupAndSwitchLog(const std::string& d) {
  std::string date = d;
  if (date.empty()) { 
    char buf[64];
    time_t now = time(NULL);
    strftime(buf, sizeof(buf), "%Y%m%d", localtime(&now));
    date.assign(buf, strlen(buf));
  }
  
  std::string backup_dir = log_meta.dir + "/backup";
  if (access(backup_dir.c_str(), F_OK)) {
    CreatePath(backup_dir.c_str(), 0775);
  }

  std::string file_path, back_file_path, file_name;
  std::string half_file_name = log_meta.file_prefix;
  if (!half_file_name.empty()) {
    half_file_name += "_";
  }
  
  for (int32_t idx = kTrace; idx < kMaxLevel; ++idx) {
    file_name = half_file_name + log_meta.log_level_strs[static_cast<LogLevel>(idx)] + ".log";
    file_path = log_meta.dir + "/" + file_name;   
    back_file_path = backup_dir + "/" + file_name + "." + date;
    /*
     * not care the successity of rename
     */
    rename(file_path.c_str(), back_file_path.c_str());
    FILE* fp =fopen(file_path.c_str(), "a+");
    if (!fp) {
      continue;
    }
    pthread_rwlock_wrlock(log_meta.log_level_files_rwlock);  
    log_meta.log_level_files[static_cast<LogLevel>(idx)] = std::make_shared<MlogFile>(fp); 
    pthread_rwlock_unlock(log_meta.log_level_files_rwlock);
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
    fwrite(str.c_str(), str.size(), 1,stdout);
	}

  std::shared_ptr<MlogFile> file_ptr = log_meta.log_level_files[self_level_];
	if (fwrite(str.c_str(), str.size(), 1, file_ptr->fp) < 1) {
    WriteFormat(stderr, "%s:%u, write %s failed\n", __FILE__, __LINE__, log_meta.log_level_strs[self_level_].c_str());
	}
}

int32_t Write(const LogLevel level, const std::string& str) {
  int32_t ret = 0;
  if (level < work_level) {
    return ret;
  }
	std::string line(log_meta.log_level_prompts[level]);
	line.append(str);
	if (log_meta.screen_out) {
    fwrite(line.c_str(), line.size(), 1, stdout); 
	}

  pthread_rwlock_rdlock(log_meta.log_level_files_rwlock);
  std::shared_ptr<MlogFile> file_ptr = log_meta.log_level_files[level];
  pthread_rwlock_unlock(log_meta.log_level_files_rwlock);
	ret = fwrite(line.c_str(), line.size(), 1, file_ptr->fp);
  #ifdef DEBUG	
	if (ret != -1) {
		fflush(file_ptr->fp);
	}
	#endif
  if (ret < 1) {
     WriteFormat(stderr, "%s:%u, write %s failed\n", __FILE__, __LINE__, log_meta.log_level_strs[level].c_str());
	}
  return ret;
}

int32_t Write(const LogLevel level, const char* format, ...) {
  int32_t ret = 0;
  if (level < work_level) {
    return ret;
  } 

  char buf[1024];
  va_list vl;
  va_start(vl, format);
  int32_t len = vsnprintf(buf, sizeof(buf), format, vl);
  va_end(vl); 
  if (len < 0) {
    return -1;
  }
  std::string line = log_meta.log_level_prompts[level]; 
  line.append(buf, len);
  if (log_meta.screen_out) {
    fwrite(line.c_str(), line.size(), 1, stdout);
  }

  pthread_rwlock_rdlock(log_meta.log_level_files_rwlock);
  std::shared_ptr<MlogFile> file_ptr = log_meta.log_level_files[level];
  pthread_rwlock_unlock(log_meta.log_level_files_rwlock);
	ret = fwrite(line.c_str(), line.size(), 1, file_ptr->fp);
#ifdef DEBUG
  if (ret != -1) {
    fflush(file_ptr->fp);
  }
#endif
  if (ret < 1) {
     WriteFormat(stderr, "%s:%u, write %s failed\n", __FILE__, __LINE__, log_meta.log_level_strs[level].c_str());
	}
  return ret;
}

}
