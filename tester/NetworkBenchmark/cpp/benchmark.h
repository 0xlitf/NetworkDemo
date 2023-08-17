#ifndef __CPP_Network_BENCHMARK_H__
#define __CPP_Network_BENCHMARK_H__

#include <QObject>
class NetworkPersisteneTest : public QObject {
	Q_OBJECT
	Q_DISABLE_COPY(NetworkPersisteneTest)
public:
	NetworkPersisteneTest() = default;
	~NetworkPersisteneTest() override = default;
	void test1();
	void test2();
	void test3();
	void test4();
	void test5();
};
#endif//__CPP_Network_BENCHMARK_H__
