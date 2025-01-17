#pragma once
#include <random>

//class Random
//{
//public:
//	static void Init()
//	{
//		s_RandomEngine.seed(std::random_device()());
//	}
//	static float Float()
//	{
//		return (float)s_Distribution(s_RandomEngine) / (float)(std::numeric_limits<uint32_t>::max)();
//	}
//
//private:
//	static std::mt19937 s_RandomEngine;
//	static std::uniform_int_distribution<std::mt19937::result_type> s_Distribution;
//};

class Random
{
public:
	static float Float()
	{
		static std::random_device rd;                          // 用于生成种子
		static std::mt19937 gen(rd());                         // Mersenne Twister 引擎
		static std::uniform_real_distribution<float> dist(0.0f, 1.0f); // 生成0到1之间的浮点数
		return dist(gen);  // 返回随机数
	}
};