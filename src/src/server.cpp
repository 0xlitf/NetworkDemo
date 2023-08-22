
#include "server.h"

#include <QDebug>
#include <QTcpServer>
#include <QThread>
#include <QMetaObject>

#include "package.h"
#include "connectpool.h"
#include "connect.h"
#include "processor.h"
using namespace std;
using namespace std::placeholders;
// ServerHelper
class ServerHelper : public QTcpServer {
public:
	ServerHelper(const std::function<void(qintptr socketDescriptor)>& onIncomingConnectionCallback) :
		onIncomingConnectionCallback_(onIncomingConnectionCallback) {
	}
	~ServerHelper() override = default;
private:
	void incomingConnection(qintptr socketDescriptor) override {
		onIncomingConnectionCallback_(socketDescriptor);
	}
private:
	std::function<void(qintptr socketDescriptor)> onIncomingConnectionCallback_;
};
// Server
QWeakPointer<NetworkThreadPool> Server::m_globalServerThreadPool;
QWeakPointer<NetworkThreadPool> Server::m_globalSocketThreadPool;
QWeakPointer<NetworkThreadPool> Server::m_globalCallbackThreadPool;
Server::Server(
	const QSharedPointer<ServerSettings> serverSettings,
	const QSharedPointer<ConnectPoolSettings> connectPoolSettings,
	const QSharedPointer<ConnectSettings> connectSettings
) :
	m_serverSettings(serverSettings),
	m_connectPoolSettings(connectPoolSettings),
	m_connectSettings(connectSettings) {
}
Server::~Server() {
	if (!this->m_tcpServer) {
		return;
	}
	m_serverThreadPool->waitRun(
		[
			this
		]() {
			m_tcpServer->close();
			m_tcpServer.clear();
		}
	);
	m_socketThreadPool->waitRunEach(
		[&]() {
			m_connectPools[QThread::currentThread()].clear();
		}
	);
}
QSharedPointer<Server> Server::createServer(
	const quint16& listenPort,
	const QHostAddress& listenAddress,
	const bool& fileTransferEnabled
) {
	QSharedPointer<ServerSettings> serverSettings(new ServerSettings);
	QSharedPointer<ConnectPoolSettings> connectPoolSettings(new ConnectPoolSettings);
	QSharedPointer<ConnectSettings> connectSettings(new ConnectSettings);
	serverSettings->listenAddress = listenAddress;
	serverSettings->listenPort = listenPort;
	if (fileTransferEnabled) {
		connectSettings->fileTransferEnabled = true;
		connectSettings->setFilePathProviderToDefaultDir();
	}
	return QSharedPointer<Server>(new Server(serverSettings, connectPoolSettings, connectSettings));
}
bool Server::begin() {
	NETWORK_THISNULL_CHECK("Server::begin", false);
	m_nodeMarkSummary = NetworkNodeMark::calculateNodeMarkSummary(m_serverSettings->dutyMark);
	if (m_globalServerThreadPool) {
		m_serverThreadPool = m_globalServerThreadPool.toStrongRef();
	} else {
		m_serverThreadPool = QSharedPointer<NetworkThreadPool>(
			new NetworkThreadPool(m_serverSettings->globalServerThreadCount));
		m_globalServerThreadPool = m_serverThreadPool.toWeakRef();
	}
	if (m_globalSocketThreadPool) {
		m_socketThreadPool = m_globalSocketThreadPool.toStrongRef();
	} else {
		m_socketThreadPool = QSharedPointer<NetworkThreadPool>(
			new NetworkThreadPool(m_serverSettings->globalSocketThreadCount));
		m_globalSocketThreadPool = m_socketThreadPool.toWeakRef();
	}
	if (m_globalCallbackThreadPool) {
		m_callbackThreadPool = m_globalCallbackThreadPool.toStrongRef();
	} else {
		m_callbackThreadPool = QSharedPointer<NetworkThreadPool>(
			new NetworkThreadPool(m_serverSettings->globalCallbackThreadCount));
		m_globalCallbackThreadPool = m_callbackThreadPool.toWeakRef();
	}
	if (!m_processors.isEmpty()) {
		QSet<QThread*> receivedPossibleThreads;
		m_callbackThreadPool->waitRunEach([&receivedPossibleThreads]() {
			receivedPossibleThreads.insert(QThread::currentThread());
			});
		for (const auto& processor : m_processors) {
			processor->setReceivedPossibleThreads(receivedPossibleThreads);
		}
	}
	bool listenSucceed = false;
	m_serverThreadPool->waitRun(
		[
			this,
			&listenSucceed
		]() {
			this->m_tcpServer = QSharedPointer<QTcpServer>(new ServerHelper([this](auto socketDescriptor) {
				this->incomingConnection(socketDescriptor);
				}));
			listenSucceed = this->m_tcpServer->listen(
				this->m_serverSettings->listenAddress,
				this->m_serverSettings->listenPort
			);
		}
	);
	if (!listenSucceed) {
		return false;
	}
	m_socketThreadPool->waitRunEach(
		[
			this
		]() {
			QSharedPointer<ConnectPoolSettings> connectPoolSettings(
				new ConnectPoolSettings(*this->m_connectPoolSettings));
			QSharedPointer<ConnectSettings>
				connectSettings(new ConnectSettings(*this->m_connectSettings));
			connectPoolSettings->connectToHostErrorCallback =
				bind(&Server::onConnectToHostError, this, _1, _2);
			connectPoolSettings->connectToHostTimeoutCallback = bind(&Server::onConnectToHostTimeout, this, _1,
				_2);
			connectPoolSettings->connectToHostSucceedCallback = bind(&Server::onConnectToHostSucceed, this, _1,
				_2);
			connectPoolSettings->remoteHostClosedCallback = bind(&Server::onRemoteHostClosed, this, _1, _2);
			connectPoolSettings->readyToDeleteCallback = bind(&Server::onReadyToDelete, this, _1, _2);
			connectPoolSettings->packageSendingCallback = bind(&Server::onPackageSending, this, _1, _2, _3, _4,
				_5, _6);
			connectPoolSettings->packageReceivingCallback = bind(&Server::onPackageReceiving, this, _1, _2, _3,
				_4, _5, _6);
			connectPoolSettings->packageReceivedCallback = bind(&Server::onPackageReceived, this, _1, _2, _3);
			connectSettings->randomFlagRangeStart = 1000000000;
			connectSettings->randomFlagRangeEnd = 1999999999;
			m_connectPools[QThread::currentThread()] = QSharedPointer<ConnectPool>(
				new ConnectPool(
					connectPoolSettings,
					connectSettings
				)
			);
		}
	);
	return true;
}
void Server::registerProcessor(const QPointer<Processor>& processor) {
	NETWORK_THISNULL_CHECK("Server::registerProcessor");
	if (m_tcpServer) {
		qDebug() << "Server::registerProcessor: please use registerProcessor befor begin()";
		return;
	}
	const auto&& availableSlots = processor->availableSlots();
	auto counter = 0;
	for (const auto& currentSlot : availableSlots) {
		if (m_processorCallbacks.contains(currentSlot)) {
			qDebug() << "Server::registerProcessor: double register:" << currentSlot;
			continue;
		}
		const auto&& callback = [processor](const QPointer<Connect>& connect,
			const QSharedPointer<Package>& package) {
				if (!processor) {
					qDebug() << "Server::registerProcessor: processor is null";
					return;
				}
				processor->handlePackage(connect, package);
		};
		m_processorCallbacks[currentSlot] = callback;
		++counter;
	}
	m_processors.insert(processor);
	if (!counter) {
		qDebug() << "Server::registerProcessor: no available slots in processor:" << processor->metaObject()->
			className();
	}
}
void Server::incomingConnection(const qintptr& socketDescriptor) {
	const auto&& rotaryIndex = m_socketThreadPool->nextRotaryIndex();
	auto runOnConnectThreadCallback =
		[
			this,
			rotaryIndex
		](const std::function<void()>& callback) {
		this->m_socketThreadPool->run(callback, rotaryIndex);
	};
	m_socketThreadPool->run(
		[
			this,
			runOnConnectThreadCallback,
			socketDescriptor
		]() {
			this->m_connectPools[QThread::currentThread()]->createConnect(
				runOnConnectThreadCallback,
				socketDescriptor
			);
		},
		rotaryIndex
	);
}
void Server::onPackageReceived(
	const QPointer<Connect>& connect,
	const QPointer<ConnectPool>&,
	const QSharedPointer<Package>& package
) {
	if (m_processorCallbacks.isEmpty()) {
		NETWORK_NULLPTR_CHECK(m_serverSettings->packageReceivedCallback);
		m_callbackThreadPool->run(
			[
				connect,
				package,
				callback = m_serverSettings->packageReceivedCallback
			]() {
				callback(connect, package);
			}
		);
	} else {
		if (package->targetActionFlag().isEmpty()) {
			qDebug() <<
				"Server::onPackageReceived: processor is enable, but package targetActionFlag is empty";
			return;
		}
		const auto&& it = m_processorCallbacks.find(package->targetActionFlag());
		if (it == m_processorCallbacks.end()) {
			qDebug() <<
				"Server::onPackageReceived: processor is enable, but package targetActionFlag not match:" <<
				package->targetActionFlag();
			return;
		}
		m_callbackThreadPool->run(
			[
				connect,
				package,
				callback = *it
			]() {
				callback(connect, package);
			}
		);
	}
}
