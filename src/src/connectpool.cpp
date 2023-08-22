
#include "connectpool.h"

#include <QDebug>
#include <QTimer>

#include "connect.h"
using namespace std;
using namespace std::placeholders;
ConnectPool::ConnectPool(
	QSharedPointer<ConnectPoolSettings> connectPoolSettings,
	QSharedPointer<ConnectSettings> connectSettings
) :
	m_connectPoolSettings(connectPoolSettings),
	m_connectSettings(connectSettings) {
	m_connectSettings->connectToHostErrorCallback = bind(&ConnectPool::onConnectToHostError, this, _1);
	m_connectSettings->connectToHostTimeoutCallback = bind(&ConnectPool::onConnectToHostTimeout, this, _1);
	m_connectSettings->connectToHostSucceedCallback = bind(&ConnectPool::onConnectToHostSucceed, this, _1);
	m_connectSettings->remoteHostClosedCallback = bind(&ConnectPool::onRemoteHostClosed, this, _1);
	m_connectSettings->readyToDeleteCallback = bind(&ConnectPool::onReadyToDelete, this, _1);
	m_connectSettings->packageSendingCallback = bind(&ConnectPool::onPackageSending, this, _1, _2, _3, _4, _5);
	m_connectSettings->packageReceivingCallback = bind(&ConnectPool::onPackageReceiving, this, _1, _2, _3, _4,
		_5);
	m_connectSettings->packageReceivedCallback = bind(&ConnectPool::onPackageReceived, this, _1, _2);
	m_connectSettings->waitReplyPackageSucceedCallback = bind(&ConnectPool::onWaitReplyPackageSucceed, this, _1,
		_2, _3);
	m_connectSettings->waitReplyPackageFailCallback = bind(&ConnectPool::onWaitReplyPackageFail, this, _1, _2);
}

ConnectPool::~ConnectPool() {
	QVector<QSharedPointer<Connect>> waitForCloseConnects;
	for (const auto& connect : m_connectForConnecting) {
		waitForCloseConnects.push_back(connect);
	}
	for (const auto& connect : m_connectForConnected) {
		waitForCloseConnects.push_back(connect);
	}
	for (const auto& connect : waitForCloseConnects) {
		connect->close();
	}
}

void ConnectPool::createConnect(
	const std::function<void(std::function<void()>)> runOnConnectThreadCallback,
	const QString& hostName,
	const quint16& port
) {
	auto connectKey = QString("%1:%2").arg(hostName).arg(port);
	mutex_.lock();
	if (m_bimapForHostAndPort1.contains(connectKey)) {
		mutex_.unlock();
		return;
	}
	mutex_.unlock();
	Connect::createConnect(
		[
			this,
			connectKey,
			hostName
		](const auto& connect) {
			this->mutex_.lock();
			this->m_connectForConnecting[connect.data()] = connect;
			this->m_bimapForHostAndPort1[connectKey] = connect.data();
			this->m_bimapForHostAndPort2[connect.data()] = connectKey;
			this->mutex_.unlock();
		},
		runOnConnectThreadCallback,
		m_connectSettings,
		hostName,
		port
	);
}

void ConnectPool::createConnect(
	const std::function<void(std::function<void()>)> runOnConnectThreadCallback,
	const qintptr& socketDescriptor
) {
	mutex_.lock();
	if (m_bimapForSocketDescriptor1.contains(socketDescriptor)) {
		mutex_.unlock();
		return;
	}
	mutex_.unlock();
	Connect::createConnect(
		[
			this,
			socketDescriptor
		](const auto& connect) {
			this->mutex_.lock();
			this->m_connectForConnecting[connect.data()] = connect;
			this->m_bimapForSocketDescriptor1[socketDescriptor] = connect.data();
			this->m_bimapForSocketDescriptor2[connect.data()] = socketDescriptor;
			this->mutex_.unlock();
		},
		runOnConnectThreadCallback,
		m_connectSettings,
		socketDescriptor
	);
}

QPair<QString, quint16> ConnectPool::getHostAndPortByConnect(const QPointer<Connect>& connect) {
	QPair<QString, quint16> reply;
	mutex_.lock();
	{
		auto it = m_bimapForHostAndPort2.find(connect.data());
		if (it != m_bimapForHostAndPort2.end()) {
			auto index = it.value().lastIndexOf(":");
			if ((index > 0) && ((index + 1) < it.value().size())) {
				reply.first = it.value().mid(0, index);
				reply.second = it.value().mid(index + 1).toUShort();
			}
		}
	}
	mutex_.unlock();
	return reply;
}

qintptr ConnectPool::getSocketDescriptorByConnect(const QPointer<Connect>& connect) {
	qintptr reply = {};
	mutex_.lock();
	{
		auto it = m_bimapForSocketDescriptor2.find(connect.data());
		if (it != m_bimapForSocketDescriptor2.end()) {
			reply = it.value();
		}
	}
	mutex_.unlock();
	return reply;
}

QPointer<Connect> ConnectPool::getConnectByHostAndPort(const QString& hostName, const quint16& port) {
	QPointer<Connect> reply;
	mutex_.lock();
	{
		auto it = m_bimapForHostAndPort1.find(QString("%1:%2").arg(hostName, QString::number(port)));
		if (it != m_bimapForHostAndPort1.end()) {
			reply = it.value();
		}
	}
	mutex_.unlock();
	return reply;
}

QPointer<Connect> ConnectPool::getConnectBySocketDescriptor(const qintptr& socketDescriptor) {
	QPointer<Connect> reply;
	mutex_.lock();
	{
		auto it = m_bimapForSocketDescriptor1.find(socketDescriptor);
		if (it != m_bimapForSocketDescriptor1.end()) {
			reply = it.value();
		}
	}
	mutex_.unlock();
	return reply;
}

void ConnectPool::onConnectToHostSucceed(const QPointer<Connect>& connect) {
	//    qDebug() << "ConnectPool::onConnectToHostSucceed:" << connect.data();
	mutex_.lock();
	auto containsInConnecting = m_connectForConnecting.contains(connect.data());
	if (!containsInConnecting) {
		mutex_.unlock();
		qDebug() << "ConnectPool::onConnectToHostSucceed: error: connect not contains" << connect.data();
		return;
	}
	m_connectForConnected[connect.data()] = m_connectForConnecting[connect.data()];
	m_connectForConnecting.remove(connect.data());
	mutex_.unlock();
	NETWORK_NULLPTR_CHECK(m_connectPoolSettings->connectToHostSucceedCallback);
	m_connectPoolSettings->connectToHostSucceedCallback(connect, this);
}

void ConnectPool::onReadyToDelete(const QPointer<Connect>& connect) {
	//    qDebug() << "ConnectPool::onReadyToDelete:" << connect.data();
	NETWORK_NULLPTR_CHECK(m_connectPoolSettings->readyToDeleteCallback);
	m_connectPoolSettings->readyToDeleteCallback(connect, this);
	mutex_.lock();
	auto containsInConnecting = m_connectForConnecting.contains(connect.data());
	auto containsInConnected = m_connectForConnected.contains(connect.data());
	auto containsInBimapForHostAndPort = m_bimapForHostAndPort2.contains(connect.data());
	auto containsInBimapForSocketDescriptor = m_bimapForSocketDescriptor2.contains(connect.data());
	if ((!containsInConnecting && !containsInConnected) || (!containsInBimapForHostAndPort && !
		containsInBimapForSocketDescriptor)) {
		mutex_.unlock();
		qDebug() << "ConnectPool::onReadyToDelete: error: connect not contains" << connect.data();
		return;
	}
	if (containsInConnecting) {
		QTimer::singleShot(0, [connect = m_connectForConnecting[connect.data()]]() {
			});
		m_connectForConnecting.remove(connect.data());
	}
	if (containsInConnected) {
		QTimer::singleShot(0, [connect = m_connectForConnected[connect.data()]]() {
			});
		m_connectForConnected.remove(connect.data());
	}
	if (containsInBimapForHostAndPort) {
		m_bimapForHostAndPort1.remove(m_bimapForHostAndPort2[connect.data()]);
		m_bimapForHostAndPort2.remove(connect.data());
	}
	if (containsInBimapForSocketDescriptor) {
		m_bimapForSocketDescriptor1.remove(m_bimapForSocketDescriptor2[connect.data()]);
		m_bimapForSocketDescriptor2.remove(connect.data());
	}
	mutex_.unlock();
}
