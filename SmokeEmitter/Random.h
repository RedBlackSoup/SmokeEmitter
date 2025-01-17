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
		static std::random_device rd;                          // ������������
		static std::mt19937 gen(rd());                         // Mersenne Twister ����
		static std::uniform_real_distribution<float> dist(0.0f, 1.0f); // ����0��1֮��ĸ�����
		return dist(gen);  // ���������
	}
};