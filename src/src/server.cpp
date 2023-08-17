
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
QWeakPointer<NetworkThreadPool> Server::globalServerThreadPool_;
QWeakPointer<NetworkThreadPool> Server::globalSocketThreadPool_;
QWeakPointer<NetworkThreadPool> Server::globalCallbackThreadPool_;
Server::Server(
	const QSharedPointer<ServerSettings> serverSettings,
	const QSharedPointer<ConnectPoolSettings> connectPoolSettings,
	const QSharedPointer<ConnectSettings> connectSettings
) :
	serverSettings_(serverSettings),
	connectPoolSettings_(connectPoolSettings),
	connectSettings_(connectSettings) {
}
Server::~Server() {
	if (!this->tcpServer_) {
		return;
	}
	serverThreadPool_->waitRun(
		[
			this
		]() {
			tcpServer_->close();
			tcpServer_.clear();
		}
	);
	socketThreadPool_->waitRunEach(
		[&]() {
			connectPools_[QThread::currentThread()].clear();
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
	nodeMarkSummary_ = NetworkNodeMark::calculateNodeMarkSummary(serverSettings_->dutyMark);
	if (globalServerThreadPool_) {
		serverThreadPool_ = globalServerThreadPool_.toStrongRef();
	} else {
		serverThreadPool_ = QSharedPointer<NetworkThreadPool>(
			new NetworkThreadPool(serverSettings_->globalServerThreadCount));
		globalServerThreadPool_ = serverThreadPool_.toWeakRef();
	}
	if (globalSocketThreadPool_) {
		socketThreadPool_ = globalSocketThreadPool_.toStrongRef();
	} else {
		socketThreadPool_ = QSharedPointer<NetworkThreadPool>(
			new NetworkThreadPool(serverSettings_->globalSocketThreadCount));
		globalSocketThreadPool_ = socketThreadPool_.toWeakRef();
	}
	if (globalCallbackThreadPool_) {
		callbackThreadPool_ = globalCallbackThreadPool_.toStrongRef();
	} else {
		callbackThreadPool_ = QSharedPointer<NetworkThreadPool>(
			new NetworkThreadPool(serverSettings_->globalCallbackThreadCount));
		globalCallbackThreadPool_ = callbackThreadPool_.toWeakRef();
	}
	if (!processors_.isEmpty()) {
		QSet<QThread*> receivedPossibleThreads;
		callbackThreadPool_->waitRunEach([&receivedPossibleThreads]() {
			receivedPossibleThreads.insert(QThread::currentThread());
			});
		for (const auto& processor : processors_) {
			processor->setReceivedPossibleThreads(receivedPossibleThreads);
		}
	}
	bool listenSucceed = false;
	serverThreadPool_->waitRun(
		[
			this,
			&listenSucceed
		]() {
			this->tcpServer_ = QSharedPointer<QTcpServer>(new ServerHelper([this](auto socketDescriptor) {
				this->incomingConnection(socketDescriptor);
				}));
			listenSucceed = this->tcpServer_->listen(
				this->serverSettings_->listenAddress,
				this->serverSettings_->listenPort
			);
		}
	);
	if (!listenSucceed) {
		return false;
	}
	socketThreadPool_->waitRunEach(
		[
			this
		]() {
			QSharedPointer<ConnectPoolSettings> connectPoolSettings(
				new ConnectPoolSettings(*this->connectPoolSettings_));
			QSharedPointer<ConnectSettings>
				connectSettings(new ConnectSettings(*this->connectSettings_));
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
			connectPools_[QThread::currentThread()] = QSharedPointer<ConnectPool>(
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
	if (tcpServer_) {
		qDebug() << "Server::registerProcessor: please use registerProcessor befor begin()";
		return;
	}
	const auto&& availableSlots = processor->availableSlots();
	auto counter = 0;
	for (const auto& currentSlot : availableSlots) {
		if (processorCallbacks_.contains(currentSlot)) {
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
		processorCallbacks_[currentSlot] = callback;
		++counter;
	}
	processors_.insert(processor);
	if (!counter) {
		qDebug() << "Server::registerProcessor: no available slots in processor:" << processor->metaObject()->
			className();
	}
}
void Server::incomingConnection(const qintptr& socketDescriptor) {
	const auto&& rotaryIndex = socketThreadPool_->nextRotaryIndex();
	auto runOnConnectThreadCallback =
		[
			this,
			rotaryIndex
		](const std::function<void()>& callback) {
		this->socketThreadPool_->run(callback, rotaryIndex);
	};
	socketThreadPool_->run(
		[
			this,
			runOnConnectThreadCallback,
			socketDescriptor
		]() {
			this->connectPools_[QThread::currentThread()]->createConnect(
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
	if (processorCallbacks_.isEmpty()) {
		NETWORK_NULLPTR_CHECK(serverSettings_->packageReceivedCallback);
		callbackThreadPool_->run(
			[
				connect,
				package,
				callback = serverSettings_->packageReceivedCallback
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
		const auto&& it = processorCallbacks_.find(package->targetActionFlag());
		if (it == processorCallbacks_.end()) {
			qDebug() <<
				"Server::onPackageReceived: processor is enable, but package targetActionFlag not match:" <<
				package->targetActionFlag();
			return;
		}
		callbackThreadPool_->run(
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
