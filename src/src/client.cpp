
#include "client.h"

#include <QDebug>
#include <QThread>
#include <QSemaphore>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

#include "connectpool.h"
#include "connect.h"
#include "package.h"
#include "processor.h"

using namespace std;
using namespace std::placeholders;

QWeakPointer<NetworkThreadPool> Client::m_globalSocketThreadPool;
QWeakPointer<NetworkThreadPool> Client::m_globalCallbackThreadPool;

Client::Client(
	const QSharedPointer<ClientSettings>& clientSettings,
	const QSharedPointer<ConnectPoolSettings> connectPoolSettings,
	const QSharedPointer<ConnectSettings> connectSettings
) :
	m_clientSettings(clientSettings),
	m_connectPoolSettings(connectPoolSettings),
	m_connectSettings(connectSettings) {
}

Client::~Client() {
	if (!m_socketThreadPool) {
		return;
	}
	m_socketThreadPool->waitRunEach(
		[this]() {
			this->m_connectPools[QThread::currentThread()].clear();
		}
	);
}

QSharedPointer<Client> Client::createClient(const bool& fileTransferEnabled) {
	QSharedPointer<ClientSettings> clientSettings(new ClientSettings);
	QSharedPointer<ConnectPoolSettings> connectPoolSettings(new ConnectPoolSettings);
	QSharedPointer<ConnectSettings> connectSettings(new ConnectSettings);
	if (fileTransferEnabled) {
		connectSettings->fileTransferEnabled = true;
		connectSettings->setFilePathProviderToDefaultDir();
	}
	return QSharedPointer<Client>(new Client(clientSettings, connectPoolSettings, connectSettings));
}

bool Client::begin() {
	NETWORK_THISNULL_CHECK("Client::begin", false);
	m_nodeMarkSummary = NetworkNodeMark::calculateNodeMarkSummary(m_clientSettings->dutyMark);
	if (m_globalSocketThreadPool) {
		m_socketThreadPool = m_globalSocketThreadPool.toStrongRef();
	} else {
		m_socketThreadPool = QSharedPointer<NetworkThreadPool>(
			new NetworkThreadPool(m_clientSettings->globalSocketThreadCount));
		m_globalSocketThreadPool = m_socketThreadPool.toWeakRef();
	}
	if (m_globalCallbackThreadPool) {
		m_callbackThreadPool = m_globalCallbackThreadPool.toStrongRef();
	} else {
		m_callbackThreadPool = QSharedPointer<NetworkThreadPool>(
			new NetworkThreadPool(m_clientSettings->globalCallbackThreadCount));
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
	m_socketThreadPool->waitRunEach(
		[
			this
		]() {
			QSharedPointer<ConnectPoolSettings> connectPoolSettings(
				new ConnectPoolSettings(*this->m_connectPoolSettings)
			);
			QSharedPointer<ConnectSettings> connectSettings(
				new ConnectSettings(*this->m_connectSettings)
			);
			connectPoolSettings->connectToHostErrorCallback =
				bind(&Client::onConnectToHostError, this, _1, _2);
			connectPoolSettings->connectToHostTimeoutCallback = bind(&Client::onConnectToHostTimeout, this, _1,
				_2);
			connectPoolSettings->connectToHostSucceedCallback = bind(&Client::onConnectToHostSucceed, this, _1,
				_2);
			connectPoolSettings->remoteHostClosedCallback = bind(&Client::onRemoteHostClosed, this, _1, _2);
			connectPoolSettings->readyToDeleteCallback = bind(&Client::onReadyToDelete, this, _1, _2);
			connectPoolSettings->packageSendingCallback = bind(&Client::onPackageSending, this, _1, _2, _3, _4,
				_5, _6);
			connectPoolSettings->packageReceivingCallback = bind(&Client::onPackageReceiving, this, _1, _2, _3,
				_4, _5, _6);
			connectPoolSettings->packageReceivedCallback = bind(&Client::onPackageReceived, this, _1, _2, _3);
			connectPoolSettings->waitReplyPackageSucceedCallback = bind(&Client::onWaitReplySucceedPackage,
				this, _1, _2, _3, _4);
			connectPoolSettings->waitReplyPackageFailCallback = bind(&Client::onWaitReplyPackageFail, this, _1,
				_2, _3);
			connectSettings->randomFlagRangeStart = 1;
			connectSettings->randomFlagRangeEnd = 999999999;
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

void Client::registerProcessor(const QPointer<Processor>& processor) {
	NETWORK_THISNULL_CHECK("Client::registerProcessor");
	if (!m_connectPools.isEmpty()) {
		qDebug() << "Client::registerProcessor: please use registerProcessor befor begin()";
		return;
	}
	const auto&& availableSlots = processor->availableSlots();
	auto counter = 0;
	for (const auto& currentSlot : availableSlots) {
		if (m_processorCallbacks.contains(currentSlot)) {
			qDebug() << "Client::registerProcessor: double register:" << currentSlot;
			continue;
		}
		const auto&& callback = [processor](const QPointer<Connect>& connect,
			const QSharedPointer<Package>& package) {
				if (!processor) {
					qDebug() << "Client::registerProcessor: processor is null";
					return;
				}
				processor->handlePackage(connect, package);
			};
		m_processorCallbacks[currentSlot] = callback;
		++counter;
	}
	m_processors.insert(processor);
	if (!counter) {
		qDebug() << "Client::registerProcessor: no available slots in processor:" << processor->metaObject()->
			className();
	}
}

void Client::createConnect(const QString& hostName, const quint16& port) {
	NETWORK_THISNULL_CHECK("Client::createConnect");
	if (!m_socketThreadPool) {
		qDebug() << "Client::createConnect: this client need to begin:" << this;
		return;
	}
	const auto&& rotaryIndex = m_socketThreadPool->nextRotaryIndex();
	auto runOnConnectThreadCallback =
		[
			this,
				rotaryIndex
		](const std::function<void()>& runCallback) {
		this->m_socketThreadPool->run(runCallback, rotaryIndex);
		};
			m_socketThreadPool->run(
				[
					this,
						runOnConnectThreadCallback,
						hostName,
						port
				]() {
					this->m_connectPools[QThread::currentThread()]->createConnect(
						runOnConnectThreadCallback,
						hostName,
						port
					);
				},
						rotaryIndex
						);
}

bool Client::waitForCreateConnect(
	const QString& hostName,
	const quint16& port,
	const int& maximumConnectToHostWaitTime
) {
	NETWORK_THISNULL_CHECK("Client::waitForCreateConnect", false);
	if (!m_socketThreadPool) {
		qDebug() << "Client::waitForCreateConnect: this client need to begin:" << this;
		return false;
	}
	if (this->containsConnect(hostName, port)) {
		return true;
	}
	QSharedPointer<QSemaphore> semaphore(new QSemaphore);
	const auto&& hostKey = QString("%1:%2").arg(hostName, QString::number(port));
	m_mutex.lock();
	m_waitConnectSucceedSemaphore[hostKey] = semaphore.toWeakRef();
	this->createConnect(hostName, port);
	m_mutex.unlock();
	auto acquireSucceed = semaphore->tryAcquire(
		1,
		(maximumConnectToHostWaitTime == -1)
		? (m_connectSettings->maximumConnectToHostWaitTime)
		: (maximumConnectToHostWaitTime)
	);
	m_mutex.lock();
	m_waitConnectSucceedSemaphore.remove(hostKey);
	m_mutex.unlock();
	if (!acquireSucceed) {
		return false;
	}
	acquireSucceed = semaphore->tryAcquire(1);
	//    qDebug() << "-->" << acquireSucceed;
	return acquireSucceed;
}

qint32 Client::sendPayloadData(
	const QString& hostName,
	const quint16& port,
	const QString& targetActionFlag,
	const QByteArray& payloadData,
	const QVariantMap& appendData,
	const ConnectPointerAndPackageSharedPointerFunction& succeedCallback,
	const ConnectPointerFunction& failCallback
) {
	NETWORK_THISNULL_CHECK("Client::sendPayloadData", 0);
	if (!m_socketThreadPool) {
		qDebug() << "Client::sendPayloadData: this client need to begin:" << this;
		if (failCallback) {
			failCallback(nullptr);
		}
		return 0;
	}
	auto connect = this->getConnect(hostName, port);
	if (!connect) {
		if (failCallback) {
			failCallback(nullptr);
		}
		return 0;
	}
	return connect->sendPayloadData(
		targetActionFlag,
		payloadData,
		appendData,
		succeedCallback,
		failCallback
	);
}

qint32 Client::sendVariantMapData(
	const QString& hostName,
	const quint16& port,
	const QString& targetActionFlag,
	const QVariantMap& variantMap,
	const QVariantMap& appendData,
	const ConnectPointerAndPackageSharedPointerFunction& succeedCallback,
	const ConnectPointerFunction& failCallback
) {
	NETWORK_THISNULL_CHECK("Client::sendVariantMapData", 0);
	if (!m_socketThreadPool) {
		qDebug() << "Client::sendVariantMapData: this client need to begin:" << this;
		if (failCallback) {
			failCallback(nullptr);
		}
		return 0;
	}
	return this->sendPayloadData(
		hostName,
		port,
		targetActionFlag,
		QJsonDocument(QJsonObject::fromVariantMap(variantMap)).toJson(QJsonDocument::Compact),
		appendData,
		succeedCallback,
		failCallback
	);
}

qint32 Client::sendFileData(
	const QString& hostName,
	const quint16& port,
	const QString& targetActionFlag,
	const QFileInfo& fileInfo,
	const QVariantMap& appendData,
	const ConnectPointerAndPackageSharedPointerFunction& succeedCallback,
	const ConnectPointerFunction& failCallback
) {
	NETWORK_THISNULL_CHECK("Client::sendFileData", 0);
	if (!m_socketThreadPool) {
		qDebug() << "Client::sendFileData: this client need to begin:" << this;
		if (failCallback) {
			failCallback(nullptr);
		}
		return 0;
	}
	auto connect = this->getConnect(hostName, port);
	if (!connect) {
		if (failCallback) {
			failCallback(nullptr);
		}
		return 0;
	}
	return connect->sendFileData(
		targetActionFlag,
		fileInfo,
		appendData,
		succeedCallback,
		failCallback
	);
}

qint32 Client::waitForSendPayloadData(
	const QString& hostName,
	const quint16& port,
	const QString& targetActionFlag,
	const QByteArray& payloadData,
	const QVariantMap& appendData,
	const ConnectPointerAndPackageSharedPointerFunction& succeedCallback,
	const ConnectPointerFunction& failCallback
) {
	NETWORK_THISNULL_CHECK("Client::waitForSendPayloadData", 0);
	if (!m_socketThreadPool) {
		qDebug() << "Client::waitForSendPayloadData: this client need to begin:" << this;
		if (failCallback) {
			failCallback(nullptr);
		}
		return 0;
	}
	QSemaphore semaphore;
	const auto&& sendReply = this->sendPayloadData(
		hostName,
		port,
		targetActionFlag,
		payloadData,
		appendData,
		[
			&semaphore,
				succeedCallback
		]
		(const auto& connect, const auto& package) {
			if (succeedCallback) {
				succeedCallback(connect, package);
			}
			semaphore.release(2);
		},
		[
			&semaphore,
				failCallback
		]
		(const auto& connect) {
			if (failCallback) {
				failCallback(connect);
			}
			semaphore.release(1);
		}
	);
	semaphore.acquire(1);
	return (semaphore.tryAcquire(1)) ? (sendReply) : (0);
}

qint32 Client::waitForSendVariantMapData(
	const QString& hostName,
	const quint16& port,
	const QString& targetActionFlag,
	const QVariantMap& variantMap,
	const QVariantMap& appendData,
	const ConnectPointerAndPackageSharedPointerFunction& succeedCallback,
	const ConnectPointerFunction& failCallback
) {
	NETWORK_THISNULL_CHECK("Client::waitForSendVariantMapData", 0);
	return this->waitForSendPayloadData(
		hostName,
		port,
		targetActionFlag,
		QJsonDocument(QJsonObject::fromVariantMap(variantMap)).toJson(QJsonDocument::Compact),
		appendData,
		succeedCallback,
		failCallback
	);
}

qint32 Client::waitForSendFileData(
	const QString& hostName,
	const quint16& port,
	const QString& targetActionFlag,
	const QFileInfo& fileInfo,
	const QVariantMap& appendData,
	const ConnectPointerAndPackageSharedPointerFunction& succeedCallback,
	const ConnectPointerFunction& failCallback
) {
	NETWORK_THISNULL_CHECK("Client::waitForSendFileData", 0);
	if (!m_socketThreadPool) {
		qDebug() << "Client::waitForSendFileData: this client need to begin:" << this;
		if (failCallback) {
			failCallback(nullptr);
		}
		return 0;
	}
	QSemaphore semaphore;
	const auto&& sendReply = this->sendFileData(
		hostName,
		port,
		targetActionFlag,
		fileInfo,
		appendData,
		[
			&semaphore,
				succeedCallback
		]
		(const auto& connect, const auto& package) {
			if (succeedCallback) {
				succeedCallback(connect, package);
			}
			semaphore.release(2);
		},
		[
			&semaphore,
				failCallback
		]
		(const auto& connect) {
			if (failCallback) {
				failCallback(connect);
			}
			semaphore.release(1);
		}
	);
	semaphore.acquire(1);
	return (semaphore.tryAcquire(1)) ? (sendReply) : (0);
}

QPointer<Connect> Client::getConnect(const QString& hostName, const quint16& port) {
	NETWORK_THISNULL_CHECK("Client::getConnect", nullptr);
	if (!m_socketThreadPool) {
		qDebug() << "Client::getConnect: this client need to begin:" << this;
		return {};
	}
	for (const auto& connectPool : this->m_connectPools) {
		auto connect = connectPool->getConnectByHostAndPort(hostName, port);
		if (!connect) {
			continue;
		}
		return connect;
	}
	if (!m_clientSettings->autoCreateConnect) {
		return {};
	}
	const auto&& autoConnectSucceed = this->waitForCreateConnect(hostName, port,
		m_clientSettings->maximumAutoConnectToHostWaitTime);
	if (!autoConnectSucceed) {
		return {};
	}
	for (const auto& connectPool : this->m_connectPools) {
		auto connect = connectPool->getConnectByHostAndPort(hostName, port);
		if (!connect) {
			continue;
		}
		return connect;
	}
	return {};
}

bool Client::containsConnect(const QString& hostName, const quint16& port) {
	NETWORK_THISNULL_CHECK("Client::containsConnect", false);
	if (!m_socketThreadPool) {
		qDebug() << "Client::containsConnect: this client need to begin:" << this;
		return {};
	}
	for (const auto& connectPool : this->m_connectPools) {
		auto connect = connectPool->getConnectByHostAndPort(hostName, port);
		if (!connect) {
			continue;
		}
		return true;
	}
	return false;
}

void Client::onConnectToHostError(const QPointer<Connect>& connect,
	const QPointer<ConnectPool>& connectPool) {
	const auto&& reply = connectPool->getHostAndPortByConnect(connect);
	this->releaseWaitConnectSucceedSemaphore(reply.first, reply.second, false);
	if (!m_clientSettings->connectToHostErrorCallback) {
		return;
	}
	if (reply.first.isEmpty() || !reply.second) {
		qDebug() << "Client::onConnectToHostError: error";
		return;
	}
	m_callbackThreadPool->run(
		[
			this,
				connect,
				hostName = reply.first,
				port = reply.second
		]() {
			if (!this->m_clientSettings->connectToHostErrorCallback) {
				return;
			}
			this->m_clientSettings->connectToHostErrorCallback(connect, hostName, port);
		}
			);
}

void Client::onConnectToHostTimeout(const QPointer<Connect>& connect,
	const QPointer<ConnectPool>& connectPool) {
	const auto&& reply = connectPool->getHostAndPortByConnect(connect);
	this->releaseWaitConnectSucceedSemaphore(reply.first, reply.second, false);
	if (!m_clientSettings->connectToHostTimeoutCallback) {
		return;
	}
	if (reply.first.isEmpty() || !reply.second) {
		qDebug() << "Client::onConnectToHostTimeout: error";
		return;
	}
	m_callbackThreadPool->run(
		[
			this,
				connect,
				hostName = reply.first,
				port = reply.second
		]() {
			if (!this->m_clientSettings->connectToHostTimeoutCallback) {
				return;
			}
			this->m_clientSettings->connectToHostTimeoutCallback(connect, hostName, port);
		}
			);
}

void Client::onConnectToHostSucceed(const QPointer<Connect>& connect,
	const QPointer<ConnectPool>& connectPool) {
	const auto&& reply = connectPool->getHostAndPortByConnect(connect);
	if (reply.first.isEmpty() || !reply.second) {
		qDebug() << "Client::onConnectToHostSucceed: connect error";
		return;
	}
	m_callbackThreadPool->run(
		[
			this,
				connect,
				hostName = reply.first,
				port = reply.second
		]() {
			this->releaseWaitConnectSucceedSemaphore(hostName, port, true);
			if (!this->m_clientSettings->connectToHostSucceedCallback) {
				return;
			}
			this->m_clientSettings->connectToHostSucceedCallback(connect, hostName, port);
		}
			);
}

void Client::onRemoteHostClosed(const QPointer<Connect>& connect,
	const QPointer<ConnectPool>& connectPool) {
	if (!m_clientSettings->remoteHostClosedCallback) {
		return;
	}
	const auto&& reply = connectPool->getHostAndPortByConnect(connect);
	if (reply.first.isEmpty() || !reply.second) {
		qDebug() << "Client::onRemoteHostClosed: error";
		return;
	}
	m_callbackThreadPool->run(
		[
			this,
				connect,
				hostName = reply.first,
				port = reply.second
		]() {
			this->m_clientSettings->remoteHostClosedCallback(connect, hostName, port);
		}
			);
}

void Client::onReadyToDelete(const QPointer<Connect>& connect,
	const QPointer<ConnectPool>& connectPool) {
	if (!m_clientSettings->readyToDeleteCallback) {
		return;
	}
	const auto&& reply = connectPool->getHostAndPortByConnect(connect);
	if (reply.first.isEmpty() || !reply.second) {
		qDebug() << "Client::onReadyToDelete: error";
		return;
	}
	m_callbackThreadPool->run(
		[
			this,
				connect,
				hostName = reply.first,
				port = reply.second
		]() {
			this->m_clientSettings->readyToDeleteCallback(connect, hostName, port);
		}
			);
}

void Client::onPackageSending(
	const QPointer<Connect>& connect,
	const QPointer<ConnectPool>& connectPool,
	const qint32& randomFlag,
	const qint64& payloadCurrentIndex,
	const qint64& payloadCurrentSize,
	const qint64& payloadTotalSize
) {
	if (!m_clientSettings->packageSendingCallback) {
		return;
	}
	const auto&& reply = connectPool->getHostAndPortByConnect(connect);
	if (reply.first.isEmpty() || !reply.second) {
		qDebug() << "Client::onPackageSending: error";
		return;
	}
	m_callbackThreadPool->run(
		[
			this,
				connect,
				hostName = reply.first,
				port = reply.second,
				randomFlag,
				payloadCurrentIndex,
				payloadCurrentSize,
				payloadTotalSize
		]() {
			this->m_clientSettings->packageSendingCallback(
				connect,
				hostName,
				port,
				randomFlag,
				payloadCurrentIndex,
				payloadCurrentSize,
				payloadTotalSize
			);
		}
			);
}

void Client::onPackageReceiving(
	const QPointer<Connect>& connect,
	const QPointer<ConnectPool>& connectPool,
	const qint32& randomFlag,
	const qint64& payloadCurrentIndex,
	const qint64& payloadCurrentSize,
	const qint64& payloadTotalSize
) {
	if (!m_clientSettings->packageReceivingCallback) {
		return;
	}
	const auto&& reply = connectPool->getHostAndPortByConnect(connect);
	if (reply.first.isEmpty() || !reply.second) {
		qDebug() << "Client::onPackageReceiving: error";
		return;
	}
	m_callbackThreadPool->run(
		[
			this,
				connect,
				hostName = reply.first,
				port = reply.second,
				randomFlag,
				payloadCurrentIndex,
				payloadCurrentSize,
				payloadTotalSize
		]() {
			this->m_clientSettings->packageReceivingCallback(
				connect,
				hostName,
				port,
				randomFlag,
				payloadCurrentIndex,
				payloadCurrentSize,
				payloadTotalSize
			);
		}
			);
}

void Client::onPackageReceived(
	const QPointer<Connect>& connect,
	const QPointer<ConnectPool>& connectPool,
	const QSharedPointer<Package>& package
) {
	if (m_processorCallbacks.isEmpty()) {
		if (!m_clientSettings->packageReceivedCallback) {
			return;
		}
		const auto&& reply = connectPool->getHostAndPortByConnect(connect);
		if (reply.first.isEmpty() || !reply.second) {
			qDebug() << "Client::onPackageReceived: error";
			return;
		}
		m_callbackThreadPool->run(
			[
				this,
					connect,
					hostName = reply.first,
					port = reply.second,
					package
			]() {
				this->m_clientSettings->packageReceivedCallback(connect, hostName, port, package);
			}
				);
	} else {
		if (package->targetActionFlag().isEmpty()) {
			qDebug() <<
				"Client::onPackageReceived: processor is enable, but package targetActionFlag is empty";
			return;
		}
		const auto&& it = m_processorCallbacks.find(package->targetActionFlag());
		if (it == m_processorCallbacks.end()) {
			qDebug() <<
				"Client::onPackageReceived: processor is enable, but package targetActionFlag not match:" <<
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

void Client::onWaitReplySucceedPackage(
	const QPointer<Connect>& connect,
	const QPointer<ConnectPool>&,
	const QSharedPointer<Package>& package,
	const ConnectPointerAndPackageSharedPointerFunction& succeedCallback
) {
	m_callbackThreadPool->run(
		[
			connect,
				package,
				succeedCallback
		]() {
			succeedCallback(connect, package);
		}
			);
}

void Client::onWaitReplyPackageFail(
	const QPointer<Connect>& connect,
	const QPointer<ConnectPool>&,
	const ConnectPointerFunction& failCallback
) {
	m_callbackThreadPool->run(
		[
			connect,
				failCallback
		]() {
			failCallback(connect);
		}
			);
}

void Client::releaseWaitConnectSucceedSemaphore(const QString& hostName, const quint16& port,
	const bool& succeed) {
	//    qDebug() << "releaseWaitConnectSucceedSemaphore: hostName:" << hostName << ", port:" << port;
	const auto&& hostKey = QString("%1:%2").arg(hostName, QString::number(port));
	this->m_mutex.lock();
	{
		auto it = this->m_waitConnectSucceedSemaphore.find(hostKey);
		if (it != this->m_waitConnectSucceedSemaphore.end()) {
			if (it->toStrongRef()) {
				it->toStrongRef()->release((succeed) ? (2) : (1));
			} else {
				qDebug() << "Client::onConnectToHostSucceed: semaphore error";
			}
		}
	}
	this->m_mutex.unlock();
}
