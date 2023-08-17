
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
QWeakPointer<NetworkThreadPool> Client::globalSocketThreadPool_;
QWeakPointer<NetworkThreadPool> Client::globalCallbackThreadPool_;
Client::Client(
	const QSharedPointer<ClientSettings>& clientSettings,
	const QSharedPointer<ConnectPoolSettings> connectPoolSettings,
	const QSharedPointer<ConnectSettings> connectSettings
) :
	clientSettings_(clientSettings),
	connectPoolSettings_(connectPoolSettings),
	connectSettings_(connectSettings)
{
}
Client::~Client()
{
	if (!socketThreadPool_)
	{
		return;
	}
	socketThreadPool_->waitRunEach(
		[this]()
		{
			this->connectPools_[QThread::currentThread()].clear();
		}
	);
}
QSharedPointer<Client> Client::createClient(
	const bool& fileTransferEnabled
)
{
	QSharedPointer<ClientSettings> clientSettings(new ClientSettings);
	QSharedPointer<ConnectPoolSettings> connectPoolSettings(new ConnectPoolSettings);
	QSharedPointer<ConnectSettings> connectSettings(new ConnectSettings);
	if (fileTransferEnabled)
	{
		connectSettings->fileTransferEnabled = true;
		connectSettings->setFilePathProviderToDefaultDir();
	}
	return QSharedPointer<Client>(new Client(clientSettings, connectPoolSettings, connectSettings));
}
bool Client::begin()
{
	NETWORK_THISNULL_CHECK("Client::begin", false);
	nodeMarkSummary_ = NetworkNodeMark::calculateNodeMarkSummary(clientSettings_->dutyMark);
	if (globalSocketThreadPool_)
	{
		socketThreadPool_ = globalSocketThreadPool_.toStrongRef();
	}
	else
	{
		socketThreadPool_ = QSharedPointer<NetworkThreadPool>(
			new NetworkThreadPool(clientSettings_->globalSocketThreadCount));
		globalSocketThreadPool_ = socketThreadPool_.toWeakRef();
	}
	if (globalCallbackThreadPool_)
	{
		callbackThreadPool_ = globalCallbackThreadPool_.toStrongRef();
	}
	else
	{
		callbackThreadPool_ = QSharedPointer<NetworkThreadPool>(
			new NetworkThreadPool(clientSettings_->globalCallbackThreadCount));
		globalCallbackThreadPool_ = callbackThreadPool_.toWeakRef();
	}
	if (!processors_.isEmpty())
	{
		QSet<QThread*> receivedPossibleThreads;
		callbackThreadPool_->waitRunEach([&receivedPossibleThreads]()
		{
			receivedPossibleThreads.insert(QThread::currentThread());
		});
		for (const auto& processor : processors_)
		{
			processor->setReceivedPossibleThreads(receivedPossibleThreads);
		}
	}
	socketThreadPool_->waitRunEach(
		[
			this
		]()
		{
			QSharedPointer<ConnectPoolSettings> connectPoolSettings(
				new ConnectPoolSettings(*this->connectPoolSettings_)
			);
			QSharedPointer<ConnectSettings> connectSettings(
				new ConnectSettings(*this->connectSettings_)
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
void Client::registerProcessor(const QPointer<Processor>& processor)
{
	NETWORK_THISNULL_CHECK("Client::registerProcessor");
	if (!connectPools_.isEmpty())
	{
		qDebug() << "Client::registerProcessor: please use registerProcessor befor begin()";
		return;
	}
	const auto&& availableSlots = processor->availableSlots();
	auto counter = 0;
	for (const auto& currentSlot : availableSlots)
	{
		if (processorCallbacks_.contains(currentSlot))
		{
			qDebug() << "Client::registerProcessor: double register:" << currentSlot;
			continue;
		}
		const auto&& callback = [processor](const QPointer<Connect>& connect,
		                                    const QSharedPointer<Package>& package)
		{
			if (!processor)
			{
				qDebug() << "Client::registerProcessor: processor is null";
				return;
			}
			processor->handlePackage(connect, package);
		};
		processorCallbacks_[currentSlot] = callback;
		++counter;
	}
	processors_.insert(processor);
	if (!counter)
	{
		qDebug() << "Client::registerProcessor: no available slots in processor:" << processor->metaObject()->
			className();
	}
}
void Client::createConnect(const QString& hostName, const quint16& port)
{
	NETWORK_THISNULL_CHECK("Client::createConnect");
	if (!socketThreadPool_)
	{
		qDebug() << "Client::createConnect: this client need to begin:" << this;
		return;
	}
	const auto&& rotaryIndex = socketThreadPool_->nextRotaryIndex();
	auto runOnConnectThreadCallback =
		[
			this,
			rotaryIndex
		](const std::function<void()>& runCallback)
	{
		this->socketThreadPool_->run(runCallback, rotaryIndex);
	};
	socketThreadPool_->run(
		[
			this,
			runOnConnectThreadCallback,
			hostName,
			port
		]()
		{
			this->connectPools_[QThread::currentThread()]->createConnect(
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
)
{
	NETWORK_THISNULL_CHECK("Client::waitForCreateConnect", false);
	if (!socketThreadPool_)
	{
		qDebug() << "Client::waitForCreateConnect: this client need to begin:" << this;
		return false;
	}
	if (this->containsConnect(hostName, port))
	{
		return true;
	}
	QSharedPointer<QSemaphore> semaphore(new QSemaphore);
	const auto&& hostKey = QString("%1:%2").arg(hostName, QString::number(port));
	mutex_.lock();
	waitConnectSucceedSemaphore_[hostKey] = semaphore.toWeakRef();
	this->createConnect(hostName, port);
	mutex_.unlock();
	auto acquireSucceed = semaphore->tryAcquire(
		1,
		(maximumConnectToHostWaitTime == -1)
			? (connectSettings_->maximumConnectToHostWaitTime)
			: (maximumConnectToHostWaitTime)
	);
	mutex_.lock();
	waitConnectSucceedSemaphore_.remove(hostKey);
	mutex_.unlock();
	if (!acquireSucceed)
	{
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
)
{
	NETWORK_THISNULL_CHECK("Client::sendPayloadData", 0);
	if (!socketThreadPool_)
	{
		qDebug() << "Client::sendPayloadData: this client need to begin:" << this;
		if (failCallback)
		{
			failCallback(nullptr);
		}
		return 0;
	}
	auto connect = this->getConnect(hostName, port);
	if (!connect)
	{
		if (failCallback)
		{
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
)
{
	NETWORK_THISNULL_CHECK("Client::sendVariantMapData", 0);
	if (!socketThreadPool_)
	{
		qDebug() << "Client::sendVariantMapData: this client need to begin:" << this;
		if (failCallback)
		{
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
)
{
	NETWORK_THISNULL_CHECK("Client::sendFileData", 0);
	if (!socketThreadPool_)
	{
		qDebug() << "Client::sendFileData: this client need to begin:" << this;
		if (failCallback)
		{
			failCallback(nullptr);
		}
		return 0;
	}
	auto connect = this->getConnect(hostName, port);
	if (!connect)
	{
		if (failCallback)
		{
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
)
{
	NETWORK_THISNULL_CHECK("Client::waitForSendPayloadData", 0);
	if (!socketThreadPool_)
	{
		qDebug() << "Client::waitForSendPayloadData: this client need to begin:" << this;
		if (failCallback)
		{
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
	(const auto& connect, const auto& package)
		{
			if (succeedCallback)
			{
				succeedCallback(connect, package);
			}
			semaphore.release(2);
		},
		[
			&semaphore,
			failCallback
		]
	(const auto& connect)
		{
			if (failCallback)
			{
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
)
{
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
)
{
	NETWORK_THISNULL_CHECK("Client::waitForSendFileData", 0);
	if (!socketThreadPool_)
	{
		qDebug() << "Client::waitForSendFileData: this client need to begin:" << this;
		if (failCallback)
		{
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
	(const auto& connect, const auto& package)
		{
			if (succeedCallback)
			{
				succeedCallback(connect, package);
			}
			semaphore.release(2);
		},
		[
			&semaphore,
			failCallback
		]
	(const auto& connect)
		{
			if (failCallback)
			{
				failCallback(connect);
			}
			semaphore.release(1);
		}
	);
	semaphore.acquire(1);
	return (semaphore.tryAcquire(1)) ? (sendReply) : (0);
}
QPointer<Connect> Client::getConnect(const QString& hostName, const quint16& port)
{
	NETWORK_THISNULL_CHECK("Client::getConnect", nullptr);
	if (!socketThreadPool_)
	{
		qDebug() << "Client::getConnect: this client need to begin:" << this;
		return {};
	}
	for (const auto& connectPool : this->connectPools_)
	{
		auto connect = connectPool->getConnectByHostAndPort(hostName, port);
		if (!connect)
		{
			continue;
		}
		return connect;
	}
	if (!clientSettings_->autoCreateConnect)
	{
		return {};
	}
	const auto&& autoConnectSucceed = this->waitForCreateConnect(hostName, port,
	                                                             clientSettings_->maximumAutoConnectToHostWaitTime);
	if (!autoConnectSucceed)
	{
		return {};
	}
	for (const auto& connectPool : this->connectPools_)
	{
		auto connect = connectPool->getConnectByHostAndPort(hostName, port);
		if (!connect)
		{
			continue;
		}
		return connect;
	}
	return {};
}
bool Client::containsConnect(const QString& hostName, const quint16& port)
{
	NETWORK_THISNULL_CHECK("Client::containsConnect", false);
	if (!socketThreadPool_)
	{
		qDebug() << "Client::containsConnect: this client need to begin:" << this;
		return {};
	}
	for (const auto& connectPool : this->connectPools_)
	{
		auto connect = connectPool->getConnectByHostAndPort(hostName, port);
		if (!connect)
		{
			continue;
		}
		return true;
	}
	return false;
}
void Client::onConnectToHostError(const QPointer<Connect>& connect,
                                           const QPointer<ConnectPool>& connectPool)
{
	const auto&& reply = connectPool->getHostAndPortByConnect(connect);
	this->releaseWaitConnectSucceedSemaphore(reply.first, reply.second, false);
	if (!clientSettings_->connectToHostErrorCallback)
	{
		return;
	}
	if (reply.first.isEmpty() || !reply.second)
	{
		qDebug() << "Client::onConnectToHostError: error";
		return;
	}
	callbackThreadPool_->run(
		[
			this,
			connect,
			hostName = reply.first,
			port = reply.second
		]()
		{
			if (!this->clientSettings_->connectToHostErrorCallback)
			{
				return;
			}
			this->clientSettings_->connectToHostErrorCallback(connect, hostName, port);
		}
	);
}
void Client::onConnectToHostTimeout(const QPointer<Connect>& connect,
                                             const QPointer<ConnectPool>& connectPool)
{
	const auto&& reply = connectPool->getHostAndPortByConnect(connect);
	this->releaseWaitConnectSucceedSemaphore(reply.first, reply.second, false);
	if (!clientSettings_->connectToHostTimeoutCallback)
	{
		return;
	}
	if (reply.first.isEmpty() || !reply.second)
	{
		qDebug() << "Client::onConnectToHostTimeout: error";
		return;
	}
	callbackThreadPool_->run(
		[
			this,
			connect,
			hostName = reply.first,
			port = reply.second
		]()
		{
			if (!this->clientSettings_->connectToHostTimeoutCallback)
			{
				return;
			}
			this->clientSettings_->connectToHostTimeoutCallback(connect, hostName, port);
		}
	);
}
void Client::onConnectToHostSucceed(const QPointer<Connect>& connect,
                                             const QPointer<ConnectPool>& connectPool)
{
	const auto&& reply = connectPool->getHostAndPortByConnect(connect);
	if (reply.first.isEmpty() || !reply.second)
	{
		qDebug() << "Client::onConnectToHostSucceed: connect error";
		return;
	}
	callbackThreadPool_->run(
		[
			this,
			connect,
			hostName = reply.first,
			port = reply.second
		]()
		{
			this->releaseWaitConnectSucceedSemaphore(hostName, port, true);
			if (!this->clientSettings_->connectToHostSucceedCallback)
			{
				return;
			}
			this->clientSettings_->connectToHostSucceedCallback(connect, hostName, port);
		}
	);
}
void Client::onRemoteHostClosed(const QPointer<Connect>& connect,
                                         const QPointer<ConnectPool>& connectPool)
{
	if (!clientSettings_->remoteHostClosedCallback)
	{
		return;
	}
	const auto&& reply = connectPool->getHostAndPortByConnect(connect);
	if (reply.first.isEmpty() || !reply.second)
	{
		qDebug() << "Client::onRemoteHostClosed: error";
		return;
	}
	callbackThreadPool_->run(
		[
			this,
			connect,
			hostName = reply.first,
			port = reply.second
		]()
		{
			this->clientSettings_->remoteHostClosedCallback(connect, hostName, port);
		}
	);
}
void Client::onReadyToDelete(const QPointer<Connect>& connect,
                                      const QPointer<ConnectPool>& connectPool)
{
	if (!clientSettings_->readyToDeleteCallback)
	{
		return;
	}
	const auto&& reply = connectPool->getHostAndPortByConnect(connect);
	if (reply.first.isEmpty() || !reply.second)
	{
		qDebug() << "Client::onReadyToDelete: error";
		return;
	}
	callbackThreadPool_->run(
		[
			this,
			connect,
			hostName = reply.first,
			port = reply.second
		]()
		{
			this->clientSettings_->readyToDeleteCallback(connect, hostName, port);
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
)
{
	if (!clientSettings_->packageSendingCallback)
	{
		return;
	}
	const auto&& reply = connectPool->getHostAndPortByConnect(connect);
	if (reply.first.isEmpty() || !reply.second)
	{
		qDebug() << "Client::onPackageSending: error";
		return;
	}
	callbackThreadPool_->run(
		[
			this,
			connect,
			hostName = reply.first,
			port = reply.second,
			randomFlag,
			payloadCurrentIndex,
			payloadCurrentSize,
			payloadTotalSize
		]()
		{
			this->clientSettings_->packageSendingCallback(
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
)
{
	if (!clientSettings_->packageReceivingCallback)
	{
		return;
	}
	const auto&& reply = connectPool->getHostAndPortByConnect(connect);
	if (reply.first.isEmpty() || !reply.second)
	{
		qDebug() << "Client::onPackageReceiving: error";
		return;
	}
	callbackThreadPool_->run(
		[
			this,
			connect,
			hostName = reply.first,
			port = reply.second,
			randomFlag,
			payloadCurrentIndex,
			payloadCurrentSize,
			payloadTotalSize
		]()
		{
			this->clientSettings_->packageReceivingCallback(
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
)
{
	if (processorCallbacks_.isEmpty())
	{
		if (!clientSettings_->packageReceivedCallback)
		{
			return;
		}
		const auto&& reply = connectPool->getHostAndPortByConnect(connect);
		if (reply.first.isEmpty() || !reply.second)
		{
			qDebug() << "Client::onPackageReceived: error";
			return;
		}
		callbackThreadPool_->run(
			[
				this,
				connect,
				hostName = reply.first,
				port = reply.second,
				package
			]()
			{
				this->clientSettings_->packageReceivedCallback(connect, hostName, port, package);
			}
		);
	}
	else
	{
		if (package->targetActionFlag().isEmpty())
		{
			qDebug() <<
				"Client::onPackageReceived: processor is enable, but package targetActionFlag is empty";
			return;
		}
		const auto&& it = processorCallbacks_.find(package->targetActionFlag());
		if (it == processorCallbacks_.end())
		{
			qDebug() <<
				"Client::onPackageReceived: processor is enable, but package targetActionFlag not match:" <<
				package->targetActionFlag();
			return;
		}
		callbackThreadPool_->run(
			[
				connect,
				package,
				callback = *it
			]()
			{
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
)
{
	callbackThreadPool_->run(
		[
			connect,
			package,
			succeedCallback
		]()
		{
			succeedCallback(connect, package);
		}
	);
}
void Client::onWaitReplyPackageFail(
	const QPointer<Connect>& connect,
	const QPointer<ConnectPool>&,
	const ConnectPointerFunction& failCallback
)
{
	callbackThreadPool_->run(
		[
			connect,
			failCallback
		]()
		{
			failCallback(connect);
		}
	);
}
void Client::releaseWaitConnectSucceedSemaphore(const QString& hostName, const quint16& port,
                                                         const bool& succeed)
{
	//    qDebug() << "releaseWaitConnectSucceedSemaphore: hostName:" << hostName << ", port:" << port;
	const auto&& hostKey = QString("%1:%2").arg(hostName, QString::number(port));
	this->mutex_.lock();
	{
		auto it = this->waitConnectSucceedSemaphore_.find(hostKey);
		if (it != this->waitConnectSucceedSemaphore_.end())
		{
			if (it->toStrongRef())
			{
				it->toStrongRef()->release((succeed) ? (2) : (1));
			}
			else
			{
				qDebug() << "Client::onConnectToHostSucceed: semaphore error";
			}
		}
	}
	this->mutex_.unlock();
}
