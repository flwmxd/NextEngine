#pragma once

#include "core/core.h"
#include "core/container/vector.h"
#include "core/container/string_view.h"
#include "core/job_system/thread.h"

struct CORE_API Profile {
	double start_time;
	double end_time;

	const char* name;
	bool ended;

	Profile(const char* name);

	double duration() {
		return end_time - start_time;
	}

	void end();
	void log();

	~Profile();
};

struct ProfileData {
	const char* name;
	double start;
	double duration;
	int profile_depth;
};

struct Frame {
	double start_of_frame;
	double frame_duration;
	double frame_swap_duration; //includes time waiting for vSync
	vector<ProfileData> profiles;
};

struct Profiler {
	static bool CORE_API paused;
	static vector<Frame> CORE_API frames[MAX_THREADS];
	static int CORE_API profile_depth[MAX_THREADS];
	static uint CORE_API frame_sample_count;

	static void CORE_API begin_frame();
	static void CORE_API end_frame();

	static void CORE_API set_frame_sample_count(uint);
	static void CORE_API begin_profile();
	static void CORE_API record_profile(const Profile&);
};