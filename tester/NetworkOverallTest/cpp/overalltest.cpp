﻿#include "overalltest.h"

#include <QtTest>
#include <QElapsedTimer>
#include <QTime>
#include <QtConcurrent>
#include <QTcpSocket>
#include <QTcpServer>

#include "network.h"

#include "processortest1.hpp"
#include "processortest2.hpp"
#include "fusiontest1.hpp"
#include "fusiontest2.hpp"
void NetworkOverallTest::NetworkThreadPoolTest() {
	QMutex mutex;
	QMap<QThread*, int> flag;
	flag[QThread::currentThread()] = 2;
	{
		NetworkThreadPool threadPool(3);
		threadPool.runEach([&]() {
			mutex.lock();
			++flag[QThread::currentThread()];
			mutex.unlock();
			});
		threadPool.runEach([&]() {
			mutex.lock();
			++flag[QThread::currentThread()];
			mutex.unlock();
			});
	}
	QCOMPARE(flag.size(), 4);
	for (auto value : flag) {
		QCOMPARE(value, 2);
	}
	{
		NetworkThreadPool threadPool(1);
		int flag = 0;
		threadPool.waitRun([&]() { ++flag; });
		QCOMPARE(flag, 1);
	}
	{
		QMap<QThread*, int> flags;
		NetworkThreadPool threadPool(3);
		threadPool.waitRunEach([&flags]() { flags[QThread::currentThread()] = 0; });
		for (auto count = 0; count < 100000; ++count) {
			threadPool.run([&flags]() { ++flags[QThread::currentThread()]; });
		}
		QThread::msleep(1000);
		QCOMPARE((*(flags.begin() + 0) + *(flags.begin() + 1) + *(flags.begin() + 2)), 100000);
	}
}
void NetworkOverallTest::NetworkThreadPoolBenchmark1() {
	int number = 0;
	QBENCHMARK_ONCE
	{
		NetworkThreadPool threadPool(3);
		for (auto count = 0; count < 10000000; ++count) {
			threadPool.run([&]() { ++number; });
		}
	}
		//    qDebug() << number;
		if (QThreadPool::globalInstance()->maxThreadCount() > 1) {
			QCOMPARE((number != 10000000), true);
		} else {
			QCOMPARE((8000000 <= number) && (number <= 10000000), true);
		}
}
void NetworkOverallTest::NetworkThreadPoolBenchmark2() {
	int number = 0;
	QBENCHMARK_ONCE
	{
		NetworkThreadPool threadPool(3);
		for (auto count = 0; count < 30000; ++count) {
			threadPool.waitRunEach([&]() { ++number; });
		}
	}
		//    qDebug() << number;
	QCOMPARE(number, 90000);
}
void NetworkOverallTest::NetworkNodeMarkTest() {
	NetworkNodeMark nodeMark("test");
	//    qDebug() << "applicationStartTime:" << nodeMark.applicationStartTime();
	//    qDebug() << "applicationFilePath:" << nodeMark.applicationFilePath();
	//    qDebug() << "localHostName:" << nodeMark.localHostName();
	//    qDebug() << "nodeMarkCreatedTime:" << nodeMark.nodeMarkCreatedTime();
	//    qDebug() << "nodeMarkClassAddress:" << nodeMark.nodeMarkClassAddress();
	//    qDebug() << "dutyMark:" << nodeMark.dutyMark();
	//    qDebug() << "nodeMarkSummary:" << nodeMark.nodeMarkSummary();
	QCOMPARE(nodeMark.applicationStartTime() > 0, true);
	QCOMPARE(nodeMark.applicationFilePath().isEmpty(), false);
	QCOMPARE(nodeMark.localHostName().isEmpty(), false);
	QCOMPARE(nodeMark.nodeMarkCreatedTime() > 0, true);
	QCOMPARE(nodeMark.nodeMarkClassAddress().isEmpty(), false);
	QCOMPARE(nodeMark.dutyMark().isEmpty(), false);
	QCOMPARE(nodeMark.nodeMarkSummary().isEmpty(), false);
	const auto&& nodeMarkSummary1 = nodeMark.nodeMarkSummary();
	QThread::msleep(50);
	const auto&& nodeMarkSummary2 = NetworkNodeMark::calculateNodeMarkSummary("test");
	QCOMPARE(nodeMarkSummary1 != nodeMarkSummary2, true);
}
void NetworkOverallTest::NetworkConnectTest() {
	auto connectSettings = QSharedPointer<ConnectSettings>(new ConnectSettings);
	bool flag1 = false;
	bool flag2 = false;
	bool flag3 = false;
	bool flag4 = false;
	bool flag5 = false;
	connectSettings->connectToHostErrorCallback = [&](const auto&) {
		flag1 = true;
		//        qDebug( "connectToHostErrorCallback" );
	};
	connectSettings->connectToHostTimeoutCallback = [&](const auto&) {
		flag2 = true;
		//        qDebug( "connectToHostTimeoutCallback" );
	};
	connectSettings->connectToHostSucceedCallback = [&](const auto&) {
		flag3 = true;
		//        qDebug( "connectToHostSucceedCallback" );
	};
	connectSettings->remoteHostClosedCallback = [&](const auto&) {
		flag4 = true;
		//        qDebug( "remoteHostClosedCallback" );
	};
	connectSettings->readyToDeleteCallback = [&](const auto&) {
		flag5 = true;
		//        qDebug( "readyToDeleteCallback" );
	};
	{
		Connect::createConnect(
			[](const auto&) {
			},
			{},
			connectSettings,
			"127.0.0.1",
			0
			);
	}
	QCOMPARE(flag1, true);
	QCOMPARE(flag2, false);
	QCOMPARE(flag3, false);
	QCOMPARE(flag4, false);
	QCOMPARE(flag5, true);
	const auto&& lanAddressEntries = Lan::lanAddressEntries();
	if (lanAddressEntries.isEmpty() || (lanAddressEntries[0].ip == QHostAddress::LocalHost)) {
		QSKIP("lanAddressEntries is empty");
	}
	flag1 = false;
	flag2 = false;
	flag3 = false;
	flag4 = false;
	flag5 = false;
	{
		QSharedPointer<Connect> connect;
		Connect::createConnect(
			[&connect](const auto& connect_) { connect = connect_; },
			{},
			connectSettings,
			"www.baidu.com",
			80
		);
		QEventLoop eventLoop;
		QTimer::singleShot(3000, &eventLoop, &QEventLoop::quit);
		eventLoop.exec();
	}
	QCOMPARE(flag1, false);
	QCOMPARE(flag2, false);
	QCOMPARE(flag3, true);
	QCOMPARE(flag4, false);
	QCOMPARE(flag5, true);
	flag1 = false;
	flag2 = false;
	flag3 = false;
	flag4 = false;
	flag5 = false;
	{
		QSharedPointer<Connect> connect;
		connectSettings->maximumConnectToHostWaitTime = 1;
		Connect::createConnect(
			[&connect](const auto& connect_) { connect = connect_; },
			{},
			connectSettings,
			"www.baidu.com",
			80
		);
		QEventLoop eventLoop;
		QTimer::singleShot(3000, &eventLoop, &QEventLoop::quit);
		eventLoop.exec();
		connectSettings->maximumConnectToHostWaitTime = 15 * 1000;
	}
	QCOMPARE(flag1, false);
	QCOMPARE(flag2, true);
	QCOMPARE(flag3, false);
	QCOMPARE(flag4, false);
	QCOMPARE(flag5, true);
}
void NetworkOverallTest::jeNetworkPackageTest() {
	{
		QCOMPARE(Package::headSize(), 24);
		QCOMPARE(Package::checkDataIsReadyReceive(QByteArray::fromHex("")), 24);
		QCOMPARE(
			Package::checkDataIsReadyReceive(QByteArray::fromHex(
				"7e 01 04030201 01 02000000 02000000 01 03000000 03000000")), -1);
		QCOMPARE(
			Package::checkDataIsReadyReceive(QByteArray::fromHex(
				"7d 00 04030201 00 02000000 02000000 00 03000000 03000000")), -1);
		QCOMPARE(
			Package::checkDataIsReadyReceive(QByteArray::fromHex(
				"7d 01 04030201 01 02000000 02000000 01 03000000 03000000")), 5);
		QCOMPARE(
			Package::checkDataIsReadyReceive(QByteArray::fromHex(
				"7d 01 04030201 01 02000000 02000000 01 03000000 03000000 112233")), 2);
		QCOMPARE(
			Package::checkDataIsReadyReceive(QByteArray::fromHex(
				"7d 01 04030201 01 02000000 02000000 01 03000000 03000000 1122334455")), 0);
		{
			auto rawData = QByteArray::fromHex("7d 01 04030201 01 02000000 02000000 01 03000000 03000000 1122334455");
			const auto&& package = Package::readPackage(rawData);
			QCOMPARE(rawData.size(), 0);
			QCOMPARE(package->isCompletePackage(), true);
			QCOMPARE(package->isAbandonPackage(), false);
			QCOMPARE(package->bootFlag(), static_cast<qint8>(0x7d));
			QCOMPARE(package->packageFlag(), static_cast<qint8>(0x1));
			QCOMPARE(package->randomFlag(), 0x01020304);
			QCOMPARE(package->metaDataFlag(), static_cast<qint8>(0x1));
			QCOMPARE(package->metaDataTotalSize(), 0x2);
			QCOMPARE(package->metaDataCurrentSize(), 2);
			QCOMPARE(package->payloadDataFlag(), static_cast<qint8>(0x1));
			QCOMPARE(package->payloadDataTotalSize(), 3);
			QCOMPARE(package->payloadDataCurrentSize(), 3);
			QCOMPARE(package->metaData(), QByteArray::fromHex("1122"));
			QCOMPARE(package->payloadData(), QByteArray::fromHex("334455"));
		}
		{
			auto rawData1 = QByteArray::fromHex("7d 01 04030201 01 05000000 02000000 01 05000000 03000000 1122334455");
			auto rawData2 = QByteArray::fromHex("7d 01 04030201 01 05000000 03000000 01 05000000 02000000 1122334455");
			auto rawData3 = QByteArray::fromHex("7d 01 04030201 01 05000000 03000000 01 05000000 02000000 1122334455");
			const auto&& package1 = Package::readPackage(rawData1);
			const auto&& package2 = Package::readPackage(rawData2);
			const auto&& package3 = Package::readPackage(rawData3);
			QCOMPARE(package1->isCompletePackage(), false);
			QCOMPARE(package1->isAbandonPackage(), false);
			QCOMPARE(package2->isCompletePackage(), false);
			QCOMPARE(package2->isAbandonPackage(), false);
			QCOMPARE(package1->mixPackage(package2), true);
			QCOMPARE(package1->isCompletePackage(), true);
			QCOMPARE(package1->isAbandonPackage(), false);
			QCOMPARE(package1->metaData(), QByteArray::fromHex("1122112233"));
			QCOMPARE(package1->payloadData(), QByteArray::fromHex("3344554455"));
			QCOMPARE(package1->mixPackage(package3), false);
			QCOMPARE(package1->isAbandonPackage(), true);
			QCOMPARE(package3->isAbandonPackage(), true);
		}
		{
			auto packages = Package::createPayloadTransportPackages(
				{}, // empty targetActionFlag
				"12345",
				{}, // empty appendData
				1,
				2
			);
			QCOMPARE(packages.size(), 3);
			auto package1 = packages.at(0);
			auto package2 = packages.at(1);
			auto package3 = packages.at(2);
			QCOMPARE(package1->randomFlag(), 1);
			QCOMPARE(package2->randomFlag(), 1);
			QCOMPARE(package3->randomFlag(), 1);
			QCOMPARE(package1->payloadDataTotalSize(), 5);
			QCOMPARE(package2->payloadDataTotalSize(), 5);
			QCOMPARE(package3->payloadDataTotalSize(), 5);
			QCOMPARE(package1->payloadDataCurrentSize(), 2);
			QCOMPARE(package2->payloadDataCurrentSize(), 2);
			QCOMPARE(package3->payloadDataCurrentSize(), 1);
			QCOMPARE(package1->payloadData(), QByteArray("12"));
			QCOMPARE(package2->payloadData(), QByteArray("34"));
			QCOMPARE(package3->payloadData(), QByteArray("5"));
			QCOMPARE(package1->payloadDataSize(), 2);
			QCOMPARE(package2->payloadDataSize(), 2);
			QCOMPARE(package3->payloadDataSize(), 1);
			QCOMPARE(package2->isAbandonPackage(), false);
			QCOMPARE(package3->isAbandonPackage(), false);
			QCOMPARE(package1->isCompletePackage(), false);
			QCOMPARE(package1->mixPackage(package2), true);
			QCOMPARE(package1->isCompletePackage(), false);
			QCOMPARE(package1->mixPackage(package3), true);
			QCOMPARE(package1->isCompletePackage(), true);
			QCOMPARE(package2->isCompletePackage(), false);
			QCOMPARE(package3->isCompletePackage(), false);
			QCOMPARE(package1->payloadDataSize(), 5);
			QCOMPARE(package1->payloadData(), QByteArray("12345"));
		}
		{
			auto packages = Package::createPayloadTransportPackages(
				{}, // empty targetActionFlag
				"12345",
				{}, // empty appendData
				2,
				5
			);
			QCOMPARE(packages.size(), 1);
			auto package = packages.first();
			QCOMPARE(package->isAbandonPackage(), false);
			QCOMPARE(package->isCompletePackage(), true);
			QCOMPARE(package->payloadDataSize(), 5);
			QCOMPARE(package->payloadData(), QByteArray("12345"));
		}
		{
			auto packages = Package::createPayloadTransportPackages(
				{}, // empty targetActionFlag
				"12345",
				{}, // empty appendData
				2,
				5
			);
			QCOMPARE(packages.size(), 1);
			auto package = packages.first();
			QCOMPARE(package->isAbandonPackage(), false);
			QCOMPARE(package->isCompletePackage(), true);
			QCOMPARE(package->payloadDataSize(), 5);
			QCOMPARE(package->payloadData(), QByteArray("12345"));
		}
		{
			auto packages = Package::createPayloadTransportPackages(
				{}, // empty targetActionFlag
				{}, // empty payloadData
				{}, // empty appendData
				2
			);
			QCOMPARE(packages.size(), 1);
			auto package = packages.first();
			QCOMPARE(package->isAbandonPackage(), false);
			QCOMPARE(package->isCompletePackage(), true);
			QCOMPARE(package->payloadDataSize(), 0);
			QCOMPARE(package->payloadData(), QByteArray());
		}
	}
	{
		auto package1 = Package::createPayloadTransportPackages({}, "Jason", {}, 1, -1, false).first();
		auto package2 = Package::createPayloadTransportPackages({}, "Jason", {}, 1, -1, true).first();
		QCOMPARE(package1->payloadDataFlag(), NETWORKPACKAGE_UNCOMPRESSEDFLAG);
		QCOMPARE(package2->payloadDataFlag(), NETWORKPACKAGE_COMPRESSEDFLAG);
		QCOMPARE(package1->payloadData() == "Jason", true);
		QCOMPARE(package2->payloadData() != "Jason", true);
		QCOMPARE(package1->payloadDataSize() == 5, true);
		QCOMPARE(package2->payloadDataSize() > 5, true);
		package2->refreshPackage();
		QCOMPARE(package1->payloadDataFlag(), package2->payloadDataFlag());
		QCOMPARE(package1->payloadData(), package2->payloadData());
		QCOMPARE(package1->payloadDataSize(), package2->payloadDataSize());
	}
	{
		auto packagesForSource = Package::createPayloadTransportPackages({}, "12345", {}, 1, 1, true);
		QCOMPARE(packagesForSource.size(), 5);
		auto packageForTarget = packagesForSource.first();
		packagesForSource.pop_front();
		packageForTarget->refreshPackage();
		for (auto package : packagesForSource) {
			QCOMPARE(package->payloadDataSize() > 1, true);
			package->refreshPackage();
			QCOMPARE(packageForTarget->mixPackage(package), true);
		}
		QCOMPARE(packageForTarget->isAbandonPackage(), false);
		QCOMPARE(packageForTarget->isCompletePackage(), true);
		QCOMPARE(packageForTarget->payloadData(), QByteArray("12345"));
	}
	{
		auto packagesForSource = Package::createPayloadTransportPackages(
			{}, "12345", QVariantMap({ {"key", "value"} }), 1, 2, true);
		QCOMPARE(packagesForSource.size(), 3);
		for (auto index = 0; index < packagesForSource.size(); ++index) {
			if (index) {
				QCOMPARE(packagesForSource[index]->metaDataCurrentSize(), 0);
				QCOMPARE(packagesForSource[index]->metaData(), QByteArray());
			} else {
				QCOMPARE(packagesForSource[index]->metaDataCurrentSize(), 52);
				QCOMPARE(packagesForSource[index]->metaData(),
					QByteArray("{\"appendData\":{\"key\":\"value\"},\"targetActionFlag\":\"\"}"));
			}
		}
	}
}
void NetworkOverallTest::NetworkServerTest() {
	auto serverSettings = QSharedPointer<ServerSettings>(new ServerSettings);
	auto connectPoolSettings = QSharedPointer<ConnectPoolSettings>(new ConnectPoolSettings);
	auto connectSettings = QSharedPointer<ConnectSettings>(new ConnectSettings);
	{
		Server server(serverSettings, connectPoolSettings, connectSettings);
	}
	serverSettings->listenPort = 42821;
	Server server(serverSettings, connectPoolSettings, connectSettings);
	QCOMPARE(server.begin(), true);
	int succeedCount = 0;
	QtConcurrent::run(
		[
			&succeedCount
		]() {
			QTcpSocket socket;
			socket.connectToHost("127.0.0.1", 42821);
			succeedCount += socket.waitForConnected();
			for (auto count = 0; count < 3; ++count) {
				QThread::msleep(100);
				socket.write("Hello,Network!");
				socket.waitForBytesWritten();
			}
		}
	);
	QThread::sleep(1);
	QCOMPARE(succeedCount, 1);
}
void NetworkOverallTest::NetworkClientTest() {
	bool flag1 = false;
	bool flag2 = false;
	int count1 = 0;
	QTcpServer tcpServer;
	QCOMPARE(tcpServer.listen(QHostAddress::Any, 12345), true);
	QSharedPointer<ClientSettings> clientSettings(new ClientSettings);
	QSharedPointer<ConnectPoolSettings> connectPoolSettings(new ConnectPoolSettings);
	QSharedPointer<ConnectSettings> connectSettings(new ConnectSettings);
	clientSettings->readyToDeleteCallback = [&flag1, &count1](const auto&, const auto&, const auto&) {
		flag1 = true;
		++count1;
	};
	{
		Client client(clientSettings, connectPoolSettings, connectSettings);
		client.begin();
		QCOMPARE(client.sendPayloadData("127.0.0.1", 23456, { }), 0);
		QThread::sleep(1);
		QCOMPARE(client.waitForCreateConnect("127.0.0.1", 23456), false);
		QThread::sleep(1);
		count1 = 0;
		for (auto count = 0; count < 500; ++count) {
			client.createConnect("127.0.0.1", static_cast<quint16>(34567 + count));
		}
		for (auto i = 0; i < 30; ++i) {
			QThread::sleep(1);
			if (count1 == 500) {
				break;
			}
		}
		QCOMPARE(count1, 500);
	}
	connectSettings->maximumReceivePackageWaitTime = 800;
	{
		Client client(clientSettings, connectPoolSettings, connectSettings);
		client.begin();
		flag1 = false;
		QCOMPARE(client.waitForCreateConnect("Hello,world!", 12345), false);
		QThread::msleep(200);
		QCOMPARE(flag1, true);
		flag1 = false;
		QCOMPARE(client.waitForCreateConnect("127.0.0.1", 12345), true);
		QCOMPARE(client.waitForCreateConnect("127.0.0.1", 12345), true);
		QCOMPARE(client.waitForCreateConnect("127.0.0.1", 12345), true);
		QThread::msleep(200);
		QCOMPARE(flag1, false);
		client.sendPayloadData("127.0.0.1", 12345, "Test", nullptr, [&flag2](const auto&) { flag2 = true; });
		QThread::msleep(100);
		QCOMPARE(flag2, false);
		QThread::msleep(1500);
		QCOMPARE(flag2, true);
	}
	QCOMPARE(flag1, true);
	connectSettings->maximumReceivePackageWaitTime = 30 * 1000;
}
void NetworkOverallTest::NetworkServerAndClientTest1() {
	QString serverFlag;
	QString clientFlag;
	QThread* serverProcessThread = nullptr;
	QThread* clientProcessThread = nullptr;
	auto server = Server::createServer(23456);
	server->connectSettings()->cutPackageSize = 2;
	server->serverSettings()->globalCallbackThreadCount = 1;
	server->serverSettings()->packageSendingCallback = [&serverFlag, &serverProcessThread](
		const auto&,
		const auto& randomFlag,
		const auto& payloadCurrentIndex,
		const auto& payloadCurrentSize,
		const auto& payloadTotalSize
		) {
			//        qDebug() << "server packageSendingCallback:" << randomFlag << payloadCurrentIndex << payloadCurrentSize << payloadTotalSize;
			serverFlag += QString("server packageSendingCallback: %1 %2 %3 %4\n").arg(randomFlag).
				arg(payloadCurrentIndex).arg(payloadCurrentSize).arg(payloadTotalSize);
			QCOMPARE(QThread::currentThread(), serverProcessThread);
	};
	server->serverSettings()->packageReceivingCallback = [&serverFlag, &serverProcessThread](
		const auto&,
		const auto& randomFlag,
		const auto& payloadCurrentIndex,
		const auto& payloadCurrentSize,
		const auto& payloadTotalSize
		) {
			//        qDebug() << "server packageReceivingCallback:" << randomFlag << payloadCurrentIndex << payloadCurrentSize << payloadTotalSize;
			serverFlag += QString("server packageReceivingCallback: %1 %2 %3 %4\n").arg(randomFlag).
				arg(payloadCurrentIndex).arg(payloadCurrentSize).arg(payloadTotalSize);
			if (!serverProcessThread) {
				serverProcessThread = QThread::currentThread();
			}
			QCOMPARE(QThread::currentThread(), serverProcessThread);
	};
	server->serverSettings()->packageReceivedCallback = [&serverFlag, &serverProcessThread](
		const auto& connect,
		const auto& package
		) {
			//        qDebug() << "server packageReceivedCallback:" << package->payloadData();
			serverFlag += QString("server packageReceivedCallback: %1\n").arg(
				QString::fromLatin1(package->payloadData()));
			connect->replyPayloadData(package->randomFlag(), "67890");
			connect->sendPayloadData("abcd");
			QCOMPARE(QThread::currentThread(), serverProcessThread);
	};
	QCOMPARE(server->begin(), true);
	auto client = Client::createClient();
	client->connectSettings()->cutPackageSize = 2;
	client->clientSettings()->globalCallbackThreadCount = 1;
	client->clientSettings()->packageSendingCallback = [&clientFlag, &clientProcessThread](
		const auto&,
		const auto& hostName,
		const auto& port,
		const auto& randomFlag,
		const auto& payloadCurrentIndex,
		const auto& payloadCurrentSize,
		const auto& payloadTotalSize
		) {
			//        qDebug() << "client packageSendingCallback:" << hostName << port << randomFlag << payloadCurrentIndex << payloadCurrentSize << payloadTotalSize;
			clientFlag += QString("client packageSendingCallback: %1 %2 %3 %4 %5 %6\n").arg(hostName).arg(port).
				arg(randomFlag).arg(payloadCurrentIndex).arg(payloadCurrentSize).arg(payloadTotalSize);
			if (!clientProcessThread) {
				clientProcessThread = QThread::currentThread();
			}
			QCOMPARE(QThread::currentThread(), clientProcessThread);
	};
	client->clientSettings()->packageReceivingCallback = [&clientFlag, &clientProcessThread](
		const auto&,
		const auto& hostName,
		const auto& port,
		const auto& randomFlag,
		const auto& payloadCurrentIndex,
		const auto& payloadCurrentSize,
		const auto& payloadTotalSize
		) {
			//        qDebug() << "client packageReceivingCallback:" << hostName << port << randomFlag << payloadCurrentIndex << payloadCurrentSize << payloadTotalSize;
			clientFlag += QString("client packageReceivingCallback: %1 %2 %3 %4 %5 %6\n").arg(hostName).arg(port).
				arg(randomFlag).arg(payloadCurrentIndex).arg(payloadCurrentSize).arg(payloadTotalSize);
			QCOMPARE(QThread::currentThread(), clientProcessThread);
	};
	client->clientSettings()->packageReceivedCallback = [&clientFlag, &clientProcessThread](
		const auto&,
		const auto& hostName,
		const auto& port,
		const auto& package
		) {
			//        qDebug() << "client packageReceivedCallback:" << hostName << port << package->payloadData();
			clientFlag += QString("client packageReceivedCallback: %1 %2 %3\n").arg(hostName).arg(port).arg(
				QString::fromLatin1(package->payloadData()));
			QCOMPARE(QThread::currentThread(), clientProcessThread);
	};
	QCOMPARE(client->begin(), true);
	QCOMPARE(client->waitForCreateConnect("127.0.0.1", 23456), true);
	auto succeedCallback = [&clientFlag, &clientProcessThread]
	(
		const auto&,
		const auto& package
		) {
			//        qDebug() << "client succeedCallback:" << package->payloadData();
			clientFlag += QString("client succeedCallback: %1\n").arg(QString::fromLatin1(package->payloadData()));
			QCOMPARE(QThread::currentThread(), clientProcessThread);
	};
	QCOMPARE(client->sendPayloadData("127.0.0.1", 23456, "12345", succeedCallback), 1);
	QThread::msleep(1000);
	QCOMPARE(serverFlag.toLatin1().data(),
		"server packageReceivingCallback: 1 0 2 5\n"
		"server packageReceivingCallback: 1 2 2 5\n"
		"server packageReceivingCallback: 1 4 1 5\n"
		"server packageReceivedCallback: 12345\n"
		"server packageSendingCallback: 1 0 2 5\n"
		"server packageSendingCallback: 1000000000 0 2 4\n"
		"server packageSendingCallback: 1 2 2 5\n"
		"server packageSendingCallback: 1000000000 2 2 4\n"
		"server packageSendingCallback: 1 4 1 5\n"
	);
	QCOMPARE(clientFlag.toLatin1().data(),
		"client packageSendingCallback: 127.0.0.1 23456 1 0 2 5\n"
		"client packageSendingCallback: 127.0.0.1 23456 1 2 2 5\n"
		"client packageSendingCallback: 127.0.0.1 23456 1 4 1 5\n"
		"client packageReceivingCallback: 127.0.0.1 23456 1 0 2 5\n"
		"client packageReceivingCallback: 127.0.0.1 23456 1000000000 0 2 4\n"
		"client packageReceivingCallback: 127.0.0.1 23456 1 2 2 5\n"
		"client packageReceivingCallback: 127.0.0.1 23456 1000000000 2 2 4\n"
		"client packageReceivedCallback: 127.0.0.1 23456 abcd\n"
		"client packageReceivingCallback: 127.0.0.1 23456 1 4 1 5\n"
		"client succeedCallback: 67890\n"
	);
}
void NetworkOverallTest::NetworkServerAndClientTest2() {
	QString serverFlag;
	QString clientFlag;
	QByteArray testData;
	for (auto count = 0; count < 32 * 1024 * 1024; ++count) {
		testData.append(rand() % 16);
	}
	auto server = Server::createServer(34567);
	server->serverSettings()->globalCallbackThreadCount = 1;
	server->serverSettings()->packageReceivingCallback = [&serverFlag](
		const auto&,
		const auto& randomFlag,
		const auto& payloadCurrentIndex,
		const auto& payloadCurrentSize,
		const auto& payloadTotalSize
		) {
			//        qDebug() << "server packageReceivingCallback:" << randomFlag << payloadCurrentIndex << payloadCurrentSize << payloadTotalSize;
			serverFlag += QString("server packageReceivingCallback: %1 %2 %3 %4\n").arg(randomFlag).
				arg(payloadCurrentIndex).arg(payloadCurrentSize).arg(payloadTotalSize);
	};
	server->serverSettings()->packageReceivedCallback = [&serverFlag, testData](
		const auto&,
		const auto& package
		) {
			//        qDebug() << "server packageReceivedCallback:" << package->payloadDataSize();
			QCOMPARE(package->payloadData(), testData);
			serverFlag += QString("server packageReceivedCallback: %1\n").arg(package->payloadDataSize());
	};
	QCOMPARE(server->begin(), true);
	auto client = Client::createClient();
	client->connectSettings()->cutPackageSize = 8 * 1024 * 1024;
	client->connectSettings()->packageCompressionThresholdForConnectSucceedElapsed = 0;
	client->clientSettings()->globalCallbackThreadCount = 1;
	client->clientSettings()->packageSendingCallback = [&clientFlag](
		const auto&,
		const auto& hostName,
		const auto& port,
		const auto& randomFlag,
		const auto& payloadCurrentIndex,
		const auto& payloadCurrentSize,
		const auto& payloadTotalSize
		) {
			//        qDebug() << "client packageSendingCallback:" << hostName << port << randomFlag << payloadCurrentIndex << payloadCurrentSize << payloadTotalSize;
			clientFlag += QString("client packageSendingCallback: %1 %2 %3 %4 %5 %6\n").arg(hostName).arg(port).
				arg(randomFlag).arg(payloadCurrentIndex).arg(payloadCurrentSize).arg(payloadTotalSize);
	};
	QCOMPARE(client->begin(), true);
	QCOMPARE(client->waitForCreateConnect("127.0.0.1", 34567), true);
	QElapsedTimer time;
	time.start();
	QCOMPARE(client->sendPayloadData("127.0.0.1", 34567, testData), 1);
	const auto&& elapsed = time.elapsed();
	//    qDebug() << "elapsed:" << elapsed;
	QCOMPARE(elapsed > 200, true);
	QThread::msleep(8000);
	const auto&& alreadyWrittenBytes = client->getConnect("127.0.0.1", 34567)->alreadyWrittenBytes();
	//    qDebug() << "alreadyWrittenBytes:" << alreadyWrittenBytes;
	QCOMPARE(alreadyWrittenBytes < 30554432, true);
	QCOMPARE(serverFlag.toLatin1().data(),
		"server packageReceivingCallback: 1 0 8388608 33554432\n"
		"server packageReceivingCallback: 1 8388608 8388608 33554432\n"
		"server packageReceivingCallback: 1 16777216 8388608 33554432\n"
		"server packageReceivingCallback: 1 25165824 8388608 33554432\n"
		"server packageReceivedCallback: 33554432\n"
	);
	QCOMPARE(clientFlag.toLatin1().data(),
		"client packageSendingCallback: 127.0.0.1 34567 1 0 8388608 33554432\n"
		"client packageSendingCallback: 127.0.0.1 34567 1 8388608 8388608 33554432\n"
		"client packageSendingCallback: 127.0.0.1 34567 1 16777216 8388608 33554432\n"
		"client packageSendingCallback: 127.0.0.1 34567 1 25165824 8388608 33554432\n"
	);
}
void NetworkOverallTest::NetworkServerAndClientTest3() {
	auto server = Server::createServer(12569);
	QMutex mutex;
	auto succeedCount = 0;
	QByteArray testData;
	testData.resize(4 * 1024);
	memset(testData.data(), 0, static_cast<size_t>(testData.size()));
	server->serverSettings()->packageReceivedCallback = [testData, &mutex, &succeedCount
	](const auto& connect, const auto& package) {
		if (package->payloadData() == testData) {
			mutex.lock();
			++succeedCount;
			mutex.unlock();
		}
		connect->replyPayloadData(package->randomFlag(), "OK");
	};
	const auto&& lanAddressEntries = Lan::lanAddressEntries();
	if (lanAddressEntries.isEmpty() || (lanAddressEntries[0].ip == QHostAddress::LocalHost)) {
		QSKIP("lanAddressEntries is empty");
	}
	auto test = [targetIp = lanAddressEntries[0].ip.toString(), testData]() {
		auto client = Client::createClient();
		QCOMPARE(client->begin(), true);
		QCOMPARE(client->waitForCreateConnect("127.0.0.1", 12569), true);
		QCOMPARE(client->waitForCreateConnect(targetIp, 12569), true);
		for (auto count = 50; count; --count) {
			client->waitForSendPayloadData("127.0.0.1", 12569, testData);
			client->waitForSendPayloadData(targetIp, 12569, testData);
		}
	};
	QCOMPARE(server->begin(), true);
	test();
	test();
	test();
	test();
	test();
	for (auto count = 300; count; --count) {
		QtConcurrent::run(test);
	}
	QThreadPool::globalInstance()->waitForDone();
	QCOMPARE(succeedCount, 30500);
}
void NetworkOverallTest::NetworkLanTest() {
	bool flag1 = false;
	bool flag2 = false;
	bool flag3 = false;
	{
		auto lan = Lan::createLan(QHostAddress("228.12.23.34"), 12345);
		lan->lanSettings()->lanNodeOnlineCallback = [&flag1](const auto& /*lanNode*/) {
			//            qDebug() << "lanNodeOnlineCallback" << lanNode.nodeMarkSummary;
			flag1 = true;
		};
		lan->lanSettings()->lanNodeActiveCallback = [&flag2](const auto& /*lanNode*/) {
			//            qDebug() << "lanNodeActiveCallback" << lanNode.nodeMarkSummary;
			flag2 = true;
		};
		lan->lanSettings()->lanNodeOfflineCallback = [&flag3](const auto& /*lanNode*/) {
			//            qDebug() << "lanNodeOfflineCallback" << lanNode.nodeMarkSummary;
			flag3 = true;
		};
		QCOMPARE(lan->begin(), true);
		QThread::sleep(20);
		auto availableLanNodes = lan->availableLanNodes();
		QCOMPARE(availableLanNodes.size() >= 1, true);
		bool flag4 = false;
		for (const auto& availableLanNode : availableLanNodes) {
			if (availableLanNode.isSelf) {
				flag4 = true;
			}
		}
		QCOMPARE(flag4, true);
	}
	QCOMPARE(flag1, true);
	QCOMPARE(flag2, true);
	QCOMPARE(flag3, true);
	const auto&& lanAddressEntries = Lan::lanAddressEntries();
	QCOMPARE(lanAddressEntries.size() >= 1, true);
	{
		QVector<QSharedPointer<Lan>> lans;
		for (auto count = 0; count < 5; ++count) {
			auto lan = Lan::createLan(QHostAddress("228.12.23.34"), 12345);
			lan->setAppendData(count);
			QCOMPARE(lan->begin(), true);
			QThread::sleep(1);
			lans.push_back(lan);
		}
		QThread::sleep(1);
		for (const auto& lan : lans) {
			const auto&& availableLanNodes = lan->availableLanNodes();
			QCOMPARE(availableLanNodes.size(), 5);
			QSet<int> flag;
			for (const auto& lanNode : availableLanNodes) {
				flag.insert(lanNode.appendData.toInt());
			}
			QCOMPARE(flag.size(), 5);
		}
	}
}
void NetworkOverallTest::NetworkProcessorTest1() {
	ProcessorTest1::TestProcessor myProcessor;
	QCOMPARE(myProcessor.availableSlots(), QSet< QString >({ "actionFlag" }));
	auto test = [&myProcessor]() {
		auto package = Package::createPayloadTransportPackages(
			"actionFlag",
			"{\"key\":\"value\"}",
			{}, // Empty appendData
			0x1234
		).first();
		package->refreshPackage();
		return myProcessor.handlePackage(
			nullptr,
			package
		);
	};
	QCOMPARE(test(), false);
	myProcessor.setReceivedPossibleThreads({ QThread::currentThread() });
	QCOMPARE(test(), true);
	QCOMPARE(myProcessor.testData_, QVariantMap({ { "key", "value" } }));
	QCOMPARE(myProcessor.testData2_, QThread::currentThread());
}
void NetworkOverallTest::NetworkProcessorTest2() {
	QFile::remove(ProcessorTest2::TestProcessor::testFileInfo(1).filePath());
	QFile::remove(ProcessorTest2::TestProcessor::testFileInfo(2).filePath());
	QFile::remove(ProcessorTest2::TestProcessor::testFileInfo(3).filePath());
	QFile::remove(ProcessorTest2::TestProcessor::testFileInfo(4).filePath());
	QThread::sleep(1);
	ProcessorTest2::TestProcessor::createTestFile(1);
	ProcessorTest2::TestProcessor::createTestFile(3);
	auto server = Server::createServer(33445, QHostAddress::LocalHost, true);
	auto processor = QSharedPointer<ProcessorTest2::TestProcessor>(new ProcessorTest2::TestProcessor);
	server->registerProcessor(processor.data());
	auto filePathProvider = [](const auto&, const auto&, const auto& fileName) {
		return QString("%1/NetworkOverallTest/%2").arg(
			QStandardPaths::writableLocation(QStandardPaths::TempLocation),
			(fileName == "myprocessor2_testfile1") ? ("myprocessor2_testfile2") : ("myprocessor2_testfile4")
		);
	};
	server->connectSettings()->filePathProvider = filePathProvider;
	QCOMPARE(server->availableProcessorMethodNames().size(), 13);
	QCOMPARE(server->begin(), true);
	auto client = Client::createClient(true);
	client->connectSettings()->filePathProvider = filePathProvider;
	QCOMPARE(client->begin(), true);
	QCOMPARE(client->waitForCreateConnect("127.0.0.1", 33445), true);
	int succeedCounter = 0;
	int keySucceedCounter = 0;
	int appendKey2SucceedCounter = 0;
	auto succeedCallback = [&](const QPointer<Connect>&, const QSharedPointer<Package>& package) {
		++succeedCounter;
		const auto&& receivedData = QJsonDocument::fromJson(package->payloadData()).object().toVariantMap();
		const auto&& receivedAppendData = package->appendData();
		if (receivedData["key"] == "value") {
			++keySucceedCounter;
		}
		if (receivedAppendData["key2"] == "value2") {
			++appendKey2SucceedCounter;
		}
	};
	client->sendPayloadData("127.0.0.1", 33445, "receivedByteArray", "test", succeedCallback);
	client->sendPayloadData("127.0.0.1", 33445, "receivedByteArraySendByteArray", "test", succeedCallback);
	client->sendPayloadData("127.0.0.1", 33445, "receivedByteArraySendVariantMap", "test", succeedCallback);
	client->sendPayloadData("127.0.0.1", 33445, "receivedByteArraySendFile", "test", succeedCallback);
	client->sendVariantMapData("127.0.0.1", 33445, "receivedVariantMap", { {"key", "value"} }, succeedCallback);
	client->sendVariantMapData("127.0.0.1", 33445, "receivedVariantMapSendByteArray", { {"key", "value"} },
		succeedCallback);
	client->sendVariantMapData("127.0.0.1", 33445, "receivedVariantMapSendVariantMap", { {"key", "value"} },
		succeedCallback);
	client->sendVariantMapData("127.0.0.1", 33445, "receivedVariantMapSendFile", { {"key", "value"} }, succeedCallback);
	client->sendFileData("127.0.0.1", 33445, "receiveFile", ProcessorTest2::TestProcessor::testFileInfo(1),
		succeedCallback);
	client->sendFileData("127.0.0.1", 33445, "receiveFileSendByteArray", ProcessorTest2::TestProcessor::testFileInfo(1),
		succeedCallback);
	client->sendFileData("127.0.0.1", 33445, "receiveFileSendVariantMap",
		ProcessorTest2::TestProcessor::testFileInfo(1), succeedCallback);
	client->sendFileData("127.0.0.1", 33445, "receiveFileSendFile", ProcessorTest2::TestProcessor::testFileInfo(1),
		succeedCallback);
	client->sendVariantMapData("127.0.0.1", 33445, "receivedVariantMapAndAppendSendVariantMapAndAppend",
		{ {"key2", "value2"} }, { {"key3", "value3"} }, succeedCallback);
	QThread::sleep(2);
	QCOMPARE(processor->counter_.size(), 13);
	QCOMPARE(succeedCounter, 10);
	QCOMPARE(keySucceedCounter, 4);
	QCOMPARE(appendKey2SucceedCounter, 1);
	QCOMPARE(ProcessorTest2::TestProcessor::testFileInfo(1).exists(), true);
	QCOMPARE(ProcessorTest2::TestProcessor::testFileInfo(2).exists(), true);
	QCOMPARE(ProcessorTest2::TestProcessor::testFileInfo(3).exists(), true);
	QCOMPARE(ProcessorTest2::TestProcessor::testFileInfo(4).exists(), true);
}
void NetworkOverallTest::NetworkSendFile() {
	QEventLoop eventLoop;
	auto flag1 = false;
	auto server = Server::createServer(12457, QHostAddress::Any, true);
	server->serverSettings()->packageReceivedCallback = [&flag1, &eventLoop](
		const QPointer<Connect>&, const QSharedPointer<Package>& package) {
			eventLoop.quit();
			flag1 = true;
			QCOMPARE(package->containsFile(), true);
			QCOMPARE(package->metaData().isEmpty(), false);
			QCOMPARE(package->payloadData().isEmpty(), true);
	};
	QCOMPARE(server->begin(), true);
	auto client = Client::createClient(true);
	QCOMPARE(client->begin(), true);
	QCOMPARE(client->waitForCreateConnect("127.0.0.1", 12457), true);
	const auto&& testFileDir = QString("%1/NetworkTestFile").arg(
		QStandardPaths::writableLocation(QStandardPaths::TempLocation));
	if (!QDir(testFileDir).exists()) {
		QCOMPARE(QDir().mkdir(testFileDir), true);
	}
	const auto&& testSourceFilePath = QString("%1/testfile").arg(testFileDir);
	const auto&& testSourceFileInfo = QFileInfo(testSourceFilePath);
	if (!testSourceFileInfo.exists() || (testSourceFileInfo.size() != (64 * 1024 * 1024))) {
		QFile testSourceFile(testSourceFilePath);
		QCOMPARE(testSourceFile.open(QIODevice::WriteOnly), true);
		QCOMPARE(testSourceFile.resize(64 * 1024 * 1024), true);
	}
	QCOMPARE(client->sendFileData("127.0.0.1", 12457, QFileInfo(testSourceFilePath)) > 0, true);
	QTimer::singleShot(10 * 1000, &eventLoop, &QEventLoop::quit);
	eventLoop.exec();
	QCOMPARE(flag1, true);
}
void NetworkOverallTest::fusionTest1() {
	auto server = Server::createServer(24680);
	auto userProcessor = QSharedPointer<FusionTest1::UserProcessor>(new FusionTest1::UserProcessor);
	auto dataProcessor = QSharedPointer<FusionTest1::DataProcessor>(new FusionTest1::DataProcessor);
	server->registerProcessor(userProcessor.data());
	server->registerProcessor(dataProcessor.data());
	QCOMPARE(server->availableProcessorMethodNames().size(), 3);
	QCOMPARE(server->begin(), true);
	auto client = Client::createClient();
	QCOMPARE(client->begin(), true);
	{
		QElapsedTimer time;
		time.start();
		{
			const auto&& sendReply = client->waitForSendPayloadData(
				"127.0.0.1",
				24680,
				"accountLogin",
				QByteArray()
			);
			QCOMPARE(sendReply, 1);
		}
		{
			bool flag2 = false;
			const auto&& sendReply = client->waitForSendPayloadData(
				"127.0.0.1",
				24680,
				"accountLogin",
				{},
				nullptr,
				[&flag2](const auto&) { flag2 = true; }
			);
			QCOMPARE(sendReply, 2);
			QCOMPARE(flag2, false);
		}
		{
			bool flag1 = false;
			const auto&& sendReply = client->waitForSendPayloadData(
				"127.0.0.1",
				24680,
				"accountLogin",
				{},
				[&flag1](const auto&, const auto&) { flag1 = true; },
				nullptr
			);
			QCOMPARE(sendReply, 3);
			QCOMPARE(flag1, true);
		}
		{
			bool flag1 = false;
			bool flag2 = false;
			const auto&& sendReply = client->waitForSendPayloadData(
				"127.0.0.1",
				24680,
				"accountLogin",
				{},
				[&flag1](const auto&, const auto&) { flag1 = true; },
				[&flag2](const auto&) { flag2 = true; }
			);
			QCOMPARE(sendReply, 4);
			QCOMPARE(flag1, true);
			QCOMPARE(flag2, false);
		}
		QCOMPARE((time.elapsed() < 100), true);
		{
			const auto&& sendReply = client->waitForSendVariantMapData(
				"127.0.0.1",
				24680,
				"msleep",
				{ {"msleep", 500} }
			);
			QCOMPARE(sendReply, 5);
		}
		QCOMPARE((time.elapsed() > 500), true);
	}
	{
		client->waitForSendVariantMapData(
			"127.0.0.1",
			24680,
			"accountLogin",
			{},
			[](const auto&, const auto& package) {
				const auto&& received = QJsonDocument::fromJson(package->payloadData()).object().toVariantMap();
				QCOMPARE(received.isEmpty(), false);
				QCOMPARE(received["succeed"].toBool(), false);
				QCOMPARE(received["message"].toString(), QString("error: handle is empty"));
				QCOMPARE(received["userName"].toString(), QString(""));
				QCOMPARE(received["userPhoneNumber"].toString(), QString(""));
			},
			[](const auto&) {
			}
		);
		client->waitForSendVariantMapData(
			"127.0.0.1",
			24680,
			"accountLogin",
			{ {"handle", "test"} },
			[](const auto&, const auto& package) {
				const auto&& received = QJsonDocument::fromJson(package->payloadData()).object().toVariantMap();
				QCOMPARE(received.isEmpty(), false);
				QCOMPARE(received["succeed"].toBool(), false);
				QCOMPARE(received["message"].toString(), QString("error: password is empty"));
				QCOMPARE(received["userName"].toString(), QString(""));
				QCOMPARE(received["userPhoneNumber"].toString(), QString(""));
			},
			[](const auto&) {
			}
		);
		client->waitForSendVariantMapData(
			"127.0.0.1",
			24680,
			"accountLogin",
			{ {"handle", "test"}, {"password", "123456"} },
			[](const auto&, const auto& package) {
				const auto&& received = QJsonDocument::fromJson(package->payloadData()).object().toVariantMap();
				QCOMPARE(received.isEmpty(), false);
				QCOMPARE(received["succeed"].toBool(), true);
				QCOMPARE(received["message"].toString(), QString(""));
				QCOMPARE(received["userName"].toString(), QString(""));
				QCOMPARE(received["userPhoneNumber"].toString(), QString(""));
			},
			[](const auto&) {
			}
		);
		client->waitForSendVariantMapData(
			"127.0.0.1",
			24680,
			"accountLogin",
			{ {"handle", "test"}, {"password", "123456"}, {"userName", ""}, {"userPhoneNumber", ""} },
			[](const auto&, const auto& package) {
				const auto&& received = QJsonDocument::fromJson(package->payloadData()).object().toVariantMap();
				QCOMPARE(received.isEmpty(), false);
				QCOMPARE(received["succeed"].toBool(), true);
				QCOMPARE(received["message"].toString(), QString(""));
				QCOMPARE(received["userName"].toString(), QString("Jason"));
				QCOMPARE(received["userPhoneNumber"].toString(), QString("18600000000"));
			},
			[](const auto&) {
			}
		);
	}
	{
		client->waitForSendVariantMapData(
			"127.0.0.1",
			24680,
			"someRecords",
			{ {"action", "download"} },
			[](const auto&, const auto& package) {
				const auto&& received = QJsonDocument::fromJson(package->payloadData()).object().toVariantMap();
				QCOMPARE(received["someRecords"].toList(), QVariantList());
			},
			[](const auto&) {
			}
		);
		client->waitForSendVariantMapData(
			"127.0.0.1",
			24680,
			"someRecords",
			{ {"action", "upload"}, {"someRecords", QVariantList({QVariant("tests")})} },
			[](const auto&, const auto& package) {
				const auto&& received = QJsonDocument::fromJson(package->payloadData()).object().toVariantMap();
				QCOMPARE(received["someRecords"].toList(), QVariantList());
			},
			[](const auto&) {
			}
		);
		client->waitForSendVariantMapData(
			"127.0.0.1",
			24680,
			"someRecords",
			{ {"action", "download"} },
			[](const auto&, const auto& package) {
				const auto&& received = QJsonDocument::fromJson(package->payloadData()).object().toVariantMap();
				QCOMPARE(received["someRecords"].toList(), QVariantList({ QVariant("tests") }));
			},
			[](const auto&) {
			}
		);
	}
}
void NetworkOverallTest::fusionTest2() {
	FusionTest2::ServerProcessor serverProcessor;
	auto server = Server::createServer(36412);
	server->registerProcessor(&serverProcessor);
	QCOMPARE(server->begin(), true);
	QVector<QSharedPointer<FusionTest2::ClientProcessor>> clientProcessors;
	QVector<QSharedPointer<Client>> clients;
	for (auto i = 0; i < 10; ++i) {
		auto client = Client::createClient();
		QSharedPointer<FusionTest2::ClientProcessor> clientProcessor(new FusionTest2::ClientProcessor);
		clients.push_back(client);
		clientProcessors.push_back(clientProcessor);
		client->registerProcessor(clientProcessor.data());
		QCOMPARE(client->begin(), true);
		QCOMPARE(client->waitForCreateConnect("127.0.0.1", 36412, 1000), true);
		QCOMPARE(
			client->waitForSendVariantMapData("127.0.0.1", 36412, "registerClient", { { "clientId", i + 1 } }) > 0,
			true);
	}
	clients.first()->waitForSendVariantMapData("127.0.0.1", 36412, "broadcastToAll", { {"message", "hello"} });
	QThread::msleep(500);
	for (const auto& clientProcessor : clientProcessors) {
		QCOMPARE(clientProcessor->receivedMessageCount(), 1);
		QCOMPARE(clientProcessor->lastReceived(), QVariantMap({ { "message", "hello" } }));
	}
	for (auto i = 0; i < 1000; ++i) {
		clients[i % 10]->sendVariantMapData("127.0.0.1", 36412, "broadcastToAll", { {"message", "hello"} });
		clients[(i + 5) % 10]->waitForSendVariantMapData("127.0.0.1", 36412, "broadcastToAll", { {"message", "hello"} });
	}
	QThread::msleep(1000);
	for (const auto& clientProcessor : clientProcessors) {
		QCOMPARE(clientProcessor->receivedMessageCount(), 2001);
		QCOMPARE(clientProcessor->lastReceived(), QVariantMap({ { "message", "hello" } }));
	}
	QSemaphore semaphore;
	for (auto i = 0; i < 500; ++i) {
		QtConcurrent::run([&]() {
			for (auto i = 0; i < 10; ++i) {
				clients[i % 10]->sendVariantMapData("127.0.0.1", 36412, "broadcastToAll", { {"message", "hello-1"} });
				clients[i % 10]->sendVariantMapData("127.0.0.1", 36412, "broadcastToAll", { {"message", "hello-2"} });
				clients[i % 10]->sendVariantMapData("127.0.0.1", 36412, "broadcastToOne", { {"clientId", i + 1} });
				clients[i % 10]->sendVariantMapData("127.0.0.1", 36412, "broadcastToOne", { {"clientId", i + 1} });
				clients[i % 10]->sendVariantMapData("127.0.0.1", 36412, "broadcastToAll", { {"message", "hello-3"} });
				clients[(i + 5) % 10]->waitForSendVariantMapData("127.0.0.1", 36412, "broadcastToAll",
					{ {"message", "hello-4"} });
			}
			semaphore.release(1);
			});
	}
	semaphore.acquire(500);
	QThread::msleep(1000);
	clients[0]->waitForSendVariantMapData("127.0.0.1", 36412, "broadcastToAll", { {"message", "hello-end"} });
	QThread::msleep(1000);
	for (const auto& clientProcessor : clientProcessors) {
		QCOMPARE(clientProcessor->receivedMessageCount(), 2001 + 5000 + 5000 + 500 + 500 + 5000 + 5000 + 1);
		if (clientProcessor->lastReceived() != QVariantMap({ {"message", "hello-end"} })) {
			qDebug() << clientProcessor->lastReceived();
			QFAIL("111");
		}
		QCOMPARE(clientProcessor->lastReceived(), QVariantMap({ { "message", "hello-end" } }));
	}
}
