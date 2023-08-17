#ifndef __CPP_NETWORK_PERSISTENETEST_H__
#define __CPP_NETWORK_PERSISTENETEST_H__

#include <QObject>
class NetworkPersisteneTest : public QObject {
	Q_OBJECT
		Q_DISABLE_COPY(NetworkPersisteneTest)
public:
	NetworkPersisteneTest() = default;
	~NetworkPersisteneTest() override = default;
	void test();
};
#endif//__CPP_NETWORK_PERSISTENETEST_H__
