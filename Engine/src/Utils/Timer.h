#pragma once
#include <chrono>

class Timer
{
public:
	void reset()
	{
		start_time = std::chrono::steady_clock::now();
	}

	double getSeconds() const
	{
		return std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count();
	}

private:
	std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
};
