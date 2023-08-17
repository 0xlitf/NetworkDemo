#ifndef CPP_NETWORK_OVERALLTEST_H_
#define CPP_NETWORK_OVERALLTEST_H_

#include <QObject>
//#define PRIVATEMACRO public
#define PRIVATEMACRO private
class NetworkOverallTest : public QObject {
	Q_OBJECT
	Q_DISABLE_COPY(NetworkOverallTest)
public:
	NetworkOverallTest() = default;
	~NetworkOverallTest() override = default;
	// PRIVATEMACRO slots:
	void NetworkThreadPoolTest();
	// PRIVATEMACRO slots:
	void NetworkThreadPoolBenchmark1();
	// PRIVATEMACRO slots:
	void NetworkThreadPoolBenchmark2();
	// PRIVATEMACRO slots:
	void NetworkNodeMarkTest();
	// PRIVATEMACRO slots:
	void NetworkConnectTest();
	// PRIVATEMACRO slots:
	void jeNetworkPackageTest();
	// PRIVATEMACRO slots:
	void NetworkServerTest();
	// PRIVATEMACRO slots:
	void NetworkClientTest();
	// PRIVATEMACRO slots:
	void NetworkServerAndClientTest1();
	// PRIVATEMACRO slots:
	void NetworkServerAndClientTest2();
	// PRIVATEMACRO slots:
	void NetworkServerAndClientTest3();
	// PRIVATEMACRO slots:
	void NetworkLanTest();
	PRIVATEMACRO slots :
	void NetworkProcessorTest1();
	PRIVATEMACRO slots :
	void NetworkProcessorTest2();
	PRIVATEMACRO slots :
	void NetworkSendFile();
	PRIVATEMACRO slots :
	void fusionTest1();
	PRIVATEMACRO slots :
	void fusionTest2();
};
#endif//CPP_NETWORK_OVERALLTEST_H_
