/*
 * test for Write format operator
 */

#include <sys/time.h>

#include <vector>
#include <pthread.h>
#include <iostream>

#include "log.h"


void* log_out(void *arg) {
	uint32_t tid = *reinterpret_cast<uint32_t*>(arg);
	for (int i = 0; i < 5; ++i) {
    mlog::Write(MINFO, "[%u] I am a info log, no is: %d\n", tid, i);
    std::cerr << i << std::endl;
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
	for (int tid = 0; tid < 3; ++tid) {
		if (pthread_join(thread_ids[tid], NULL) < 0) {
			std::cerr << "thread join failed at tid: " << tid << std::endl;
			exit(-1);
		}
	}

	std::cerr << "finished ...." << std::endl;
	std::cerr << "time elapsed: " << MicroTimes() - start_us << " us" << std::endl;
	return 0;
}
