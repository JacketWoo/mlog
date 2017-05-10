/*
 * test for Write api
 */
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include <vector>
#include <pthread.h>
#include <iostream>
#include <initializer_list>

#include "log.h"

int32_t Log(const mlog::LogLevel level, const std::initializer_list<std::string>& args) {
  time_t now = time(NULL);
  char* time_ptr = ctime(&now);
  std::string line = time_ptr;
  line.append("|");

  for (const std::string& arg : args) {
    line.append(arg);
    line.append("|");
  }
  line.append("\n");
  mlog::Write(level, line);
  return 0;
}

void* log_out(void *arg) {
	for (int i = 0; i < 1; ++i) {
    //mlog::Write(MINFO, "buyunff I am a info log\n");
		Log(MDOT, {"a1@123", "6919022924116746787", "send"});
	}
	return NULL;
}

uint64_t MicroTimes() {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec*1000000 + tv.tv_usec;
}

int main() {
	mlog::Init(mlog::kInfo, "./my_log_path", "sample", false);

	uint64_t start_us = MicroTimes();	
	std::vector<pthread_t> thread_ids;
	pthread_t thread_id;
	for (int tid = 0; tid < 5; ++tid) {
		if (pthread_create(&thread_id, NULL, log_out, reinterpret_cast<void*>(&tid))< 0) {
			std::cerr << "create thread failed at tid: " << tid << std::endl;
			exit(-1);
		}
		thread_ids.push_back(thread_id);
	}
	for (int tid = 0; tid < 5; ++tid) {
		if (pthread_join(thread_ids[tid], NULL) < 0) {
			std::cerr << "thread join failed at tid: " << tid << std::endl;
			exit(-1);
		}
	}

	std::cerr << "finished ...." << std::endl;
	std::cerr << "time elapsed: " << MicroTimes() - start_us << " us" << std::endl;
	sleep(3000);
	return 0;
}
