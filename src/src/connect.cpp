
#include "connect.h"
// C lib import
#if ( defined Q_OS_MAC ) || ( defined __MINGW32__ ) || ( defined Q_OS_LINUX )
#   include <utime.h>
#endif

#include <QDebug>
#include <QTcpSocket>
#include <QTimer>
#include <QThread>
#include <QDateTime>
#include <QStandardPaths>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonObject>
#include <QJsonDocument>
#include <QNetworkProxy>

#include "package.h"

// ConnectSettings
void ConnectSettings::setFilePathProviderToDefaultDir() {
	const auto&& defaultDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
	filePathProvider = [defaultDir](const auto&, const auto&, const auto& fileName) {
		return QString("%1/NetworkReceivedFile/%2").arg(defaultDir, fileName);
		};
}

void ConnectSettings::setFilePathProviderToDir(const QDir& dir) {
	filePathProvider = [dir](const auto&, const auto&, const auto& fileName) {
		return QString("%1/%2").arg(dir.path(), fileName);
		};
}

// Connect
Connect::Connect(const QSharedPointer<ConnectSettings>& connectSettings) :
	m_connectSettings(connectSettings),
	m_tcpSocket(new QTcpSocket),
	m_connectCreateTime(QDateTime::currentMSecsSinceEpoch()) {
	connect(m_tcpSocket.data(), &QAbstractSocket::stateChanged, this, &Connect::onTcpSocketStateChanged,
		Qt::DirectConnection);
	connect(m_tcpSocket.data(), &QAbstractSocket::bytesWritten, this, &Connect::onTcpSocketBytesWritten,
		Qt::DirectConnection);
	connect(m_tcpSocket.data(), &QTcpSocket::readyRead, this, &Connect::onTcpSocketReadyRead,
		Qt::DirectConnection);
	if (m_connectSettings->fileTransferEnabled && !m_connectSettings->filePathProvider) {
		m_connectSettings->setFilePathProviderToDefaultDir();
		qDebug() << "Connect: fileTransfer is enabled, but filePathProvider is null, use default dir:"
			<< m_connectSettings->filePathProvider(QPointer<Connect>(nullptr),
			QSharedPointer<Package>(nullptr), QString());
	}

#ifdef Q_OS_IOS
	static bool flag = true;
	if (flag) {
		flag = false;
		QTcpSocket socket;
		socket.setProxy(QNetworkProxy::NoProxy);
		socket.connectToHost("baidu.com", 12345);
		socket.waitForConnected(10);
	}
#endif
}

void Connect::createConnect(
	const std::function<void(const QSharedPointer<Connect>&)>& onConnectCreatedCallback,
	const std::function<void(std::function<void()>)>& runOnConnectThreadCallback,
	const QSharedPointer<ConnectSettings>& connectSettings,
	const QString& hostName,
	const quint16& port
) {
	QSharedPointer<Connect> newConnect(new Connect(connectSettings));
	newConnect->m_runOnConnectThreadCallback = runOnConnectThreadCallback;
	newConnect->m_sendRandomFlagRotaryIndex = connectSettings->randomFlagRangeStart - 1;
	NETWORK_NULLPTR_CHECK(onConnectCreatedCallback);
	onConnectCreatedCallback(newConnect);
	newConnect->startTimerForConnectToHostTimeOut();
	newConnect->m_tcpSocket->setProxy(QNetworkProxy::NoProxy);
	newConnect->m_tcpSocket->connectToHost(hostName, port);
}

void Connect::createConnect(
	const std::function<void(const QSharedPointer<Connect>&)>& onConnectCreatedCallback,
	const std::function<void(std::function<void()>)>& runOnConnectThreadCallback,
	const QSharedPointer<ConnectSettings>& connectSettings,
	const qintptr& socketDescriptor
) {
	QSharedPointer<Connect> newConnect(new Connect(connectSettings));
	newConnect->m_runOnConnectThreadCallback = runOnConnectThreadCallback;
	newConnect->m_sendRandomFlagRotaryIndex = connectSettings->randomFlagRangeStart - 1;
	NETWORK_NULLPTR_CHECK(onConnectCreatedCallback);
	onConnectCreatedCallback(newConnect);
	newConnect->startTimerForConnectToHostTimeOut();
	newConnect->m_tcpSocket->setSocketDescriptor(socketDescriptor);
}

void Connect::close() {
	NETWORK_THISNULL_CHECK("Connect::close");
	if (m_isAbandonTcpSocket) {
		return;
	}
	NETWORK_NULLPTR_CHECK(m_tcpSocket);
	this->onReadyToDelete();
}

qint32 Connect::sendPayloadData(
	const QString& targetActionFlag,
	const QByteArray& payloadData,
	const QVariantMap& appendData,
	const ConnectPointerAndPackageSharedPointerFunction& succeedCallback,
	const ConnectPointerFunction& failCallback
) {
	NETWORK_THISNULL_CHECK("Connect::sendPayloadData", 0);
	if (m_isAbandonTcpSocket) {
		return 0;
	}
	NETWORK_NULLPTR_CHECK(m_runOnConnectThreadCallback, 0);
	const auto currentRandomFlag = this->nextRandomFlag();
	const auto&& readySendPayloadDataSucceed = this->readySendPayloadData(
		currentRandomFlag,
		targetActionFlag,
		payloadData,
		appendData,
		succeedCallback,
		failCallback
	);
	if (!readySendPayloadDataSucceed) {
		return 0;
	}
	return currentRandomFlag;
}

qint32 Connect::sendVariantMapData(
	const QString& targetActionFlag,
	const QVariantMap& variantMap,
	const QVariantMap& appendData,
	const ConnectPointerAndPackageSharedPointerFunction& succeedCallback,
	const ConnectPointerFunction& failCallback
) {
	NETWORK_THISNULL_CHECK("Connect::sendVariantMapData", 0);
	return this->sendPayloadData(
		targetActionFlag,
		QJsonDocument(QJsonObject::fromVariantMap(variantMap)).toJson(QJsonDocument::Compact),
		appendData,
		succeedCallback,
		failCallback
	);
}

qint32 Connect::sendFileData(
	const QString& targetActionFlag,
	const QFileInfo& fileInfo,
	const QVariantMap& appendData,
	const ConnectPointerAndPackageSharedPointerFunction& succeedCallback,
	const ConnectPointerFunction& failCallback
) {
	NETWORK_THISNULL_CHECK("Connect::sendFileData", 0);
	if (m_isAbandonTcpSocket) {
		return 0;
	}
	NETWORK_NULLPTR_CHECK(m_runOnConnectThreadCallback, 0);
	const auto currentRandomFlag = this->nextRandomFlag();
	const auto&& readySendFileDataSucceed = this->readySendFileData(
		currentRandomFlag,
		targetActionFlag,
		fileInfo,
		appendData,
		succeedCallback,
		failCallback
	);
	if (!readySendFileDataSucceed) {
		return 0;
	}
	return currentRandomFlag;
}

qint32 Connect::replyPayloadData(
	const qint32& receivedPackageRandomFlag,
	const QByteArray& payloadData,
	const QVariantMap& appendData
) {
	NETWORK_THISNULL_CHECK("Connect::replyPayloadData", 0);
	if (m_isAbandonTcpSocket) {
		return 0;
	}
	NETWORK_NULLPTR_CHECK(m_runOnConnectThreadCallback, 0);
	const auto&& readySendPayloadDataSucceed = this->readySendPayloadData(
		receivedPackageRandomFlag,
		{}, // empty targetActionFlag
		payloadData,
		appendData,
		nullptr,
		nullptr
	);
	if (!readySendPayloadDataSucceed) {
		return 0;
	}
	return receivedPackageRandomFlag;
}

qint32 Connect::replyVariantMapData(
	const qint32& receivedPackageRandomFlag,
	const QVariantMap& variantMap,
	const QVariantMap& appendData
) {
	NETWORK_THISNULL_CHECK("Connect::replyVariantMapData", 0);
	return this->replyPayloadData(
		receivedPackageRandomFlag,
		QJsonDocument(QJsonObject::fromVariantMap(variantMap)).toJson(QJsonDocument::Compact),
		appendData
	);
}

qint32 Connect::replyFile(
	const qint32& receivedPackageRandomFlag,
	const QFileInfo& fileInfo,
	const QVariantMap& appendData
) {
	NETWORK_THISNULL_CHECK("Connect::replyFile", 0);
	if (m_isAbandonTcpSocket) {
		return 0;
	}
	NETWORK_NULLPTR_CHECK(m_runOnConnectThreadCallback, 0);
	const auto&& readySendFileData = this->readySendFileData(
		receivedPackageRandomFlag,
		{}, // empty targetActionFlag
		fileInfo,
		appendData,
		nullptr,
		nullptr
	);
	if (!readySendFileData) {
		return 0;
	}
	return receivedPackageRandomFlag;
}

bool Connect::putPayloadData(
	const QString& targetActionFlag,
	const QByteArray& payloadData,
	const QVariantMap& appendData
) {
	NETWORK_THISNULL_CHECK("Connect::putPayloadData", 0);
	if (m_isAbandonTcpSocket) {
		return false;
	}
	NETWORK_NULLPTR_CHECK(m_runOnConnectThreadCallback, 0);
	const auto&& readySendPayloadDataSucceed = this->readySendPayloadData(
		2000000001,
		targetActionFlag,
		payloadData,
		appendData,
		nullptr,
		nullptr
	);
	if (!readySendPayloadDataSucceed) {
		return false;
	}
	return true;
}

bool Connect::putVariantMapData(
	const QString& targetActionFlag,
	const QVariantMap& variantMap,
	const QVariantMap& appendData
) {
	NETWORK_THISNULL_CHECK("Connect::putVariantMapData", 0);
	return this->putPayloadData(
		targetActionFlag,
		QJsonDocument(QJsonObject::fromVariantMap(variantMap)).toJson(QJsonDocument::Compact),
		appendData
	);
}

bool Connect::putFile(
	const QString& targetActionFlag,
	const QFileInfo& fileInfo,
	const QVariantMap& appendData
) {
	NETWORK_THISNULL_CHECK("Connect::putFile", 0);
	if (m_isAbandonTcpSocket) {
		return false;
	}
	NETWORK_NULLPTR_CHECK(m_runOnConnectThreadCallback, 0);
	const auto&& readySendFileData = this->readySendFileData(
		2000000001,
		targetActionFlag,
		fileInfo,
		appendData,
		nullptr,
		nullptr
	);
	if (!readySendFileData) {
		return false;
	}
	return true;
}

void Connect::onTcpSocketStateChanged() {
	if (m_isAbandonTcpSocket) {
		return;
	}
	NETWORK_NULLPTR_CHECK(m_tcpSocket);
	const auto&& state = m_tcpSocket->state();
	//    qDebug() << "onTcpSocketStateChanged:" << this << ": state:" << state;
	switch (state) {
		case QAbstractSocket::ConnectedState:
		{
			if (!m_timerForConnectToHostTimeOut.isNull()) {
				m_timerForConnectToHostTimeOut.clear();
			}
			NETWORK_NULLPTR_CHECK(m_connectSettings->connectToHostSucceedCallback);
			m_connectSettings->connectToHostSucceedCallback(this);
			m_onceConnectSucceed = true;
			m_connectSucceedTime = QDateTime::currentMSecsSinceEpoch();
			break;
		}
		case QAbstractSocket::UnconnectedState:
		{
			//            qDebug() << "onTcpSocketStateChanged:" << this << ": UnconnectedState: error:" << tcpSocket_->error();
			switch (m_tcpSocket->error()) {
				case QAbstractSocket::UnknownSocketError:
				{
					if (m_onceConnectSucceed) {
						break;
					}
					NETWORK_NULLPTR_CHECK(m_connectSettings->connectToHostErrorCallback);
					m_connectSettings->connectToHostErrorCallback(this);
					break;
				}
				case QAbstractSocket::RemoteHostClosedError:
				{
					NETWORK_NULLPTR_CHECK(m_connectSettings->remoteHostClosedCallback);
					m_connectSettings->remoteHostClosedCallback(this);
					break;
				}
				case QAbstractSocket::HostNotFoundError:
				case QAbstractSocket::ConnectionRefusedError:
				{
					NETWORK_NULLPTR_CHECK(m_connectSettings->connectToHostErrorCallback);
					m_connectSettings->connectToHostErrorCallback(this);
					break;
				}
				case QAbstractSocket::NetworkError:
				{
					break;
				}
				default:
				{
					qDebug() << "onTcpSocketStateChanged: unknow error:" << m_tcpSocket->error();
					break;
				}
			}
			this->onReadyToDelete();
			break;
		}
		default:
		{
			break;
		}
	}
}

void Connect::onTcpSocketBytesWritten(const qint64& bytes) {
	if (m_isAbandonTcpSocket) {
		return;
	}
	NETWORK_NULLPTR_CHECK(m_tcpSocket);
	m_waitForSendBytes -= bytes;
	m_alreadyWrittenBytes += bytes;
	//    qDebug() << "onTcpSocketBytesWritten:" << waitForSendBytes_ << alreadyWrittenBytes_ << QThread::currentThread();
}

void Connect::onTcpSocketReadyRead() {
	if (m_isAbandonTcpSocket) {
		return;
	}
	NETWORK_NULLPTR_CHECK(m_tcpSocket);
	const auto&& data = m_tcpSocket->readAll();
	m_tcpSocketBuffer.append(data);
	//    qDebug() << tcpSocketBuffer_.size() << data.size();
	forever
	{
		const auto && checkReply = Package::checkDataIsReadyReceive(m_tcpSocketBuffer);
		if (checkReply > 0) {
			return;
		}
		if (checkReply < 0) {
			m_tcpSocketBuffer.remove(0, checkReply * -1);
		} else {
			auto package = Package::readPackage(m_tcpSocketBuffer);
			if (package->isCompletePackage()) {
				switch (package->packageFlag()) {
				case NETWORKPACKAGE_PAYLOADDATATRANSPORTPACKGEFLAG:
				case NETWORKPACKAGE_FILEDATATRANSPORTPACKGEFLAG:
					{
						NETWORK_NULLPTR_CHECK(m_connectSettings->packageReceivingCallback);
						m_connectSettings->packageReceivingCallback(this, package->randomFlag(), 0,
																   package->payloadDataCurrentSize(),
																   package->payloadDataTotalSize());
						this->onDataTransportPackageReceived(package);
						break;
					}
				case NETWORKPACKAGE_PAYLOADDATAREQUESTPACKGEFLAG:
					{
						if (!m_sendPayloadPackagePool.contains(package->randomFlag())) {
							qDebug() << "Connect::onTcpSocketReadyRead: no contains randonFlag:" << package->
								randomFlag();
							break;
						}
						auto& packages = m_sendPayloadPackagePool[package->randomFlag()];
						if (packages.isEmpty()) {
							qDebug() << "Connect::onTcpSocketReadyRead: packages is empty:" << package->
								randomFlag();
							break;
						}
						auto nextPackage = packages.first();
						packages.pop_front();
						this->sendPackageToRemote(nextPackage);
						NETWORK_NULLPTR_CHECK(m_connectSettings->packageSendingCallback);
						m_connectSettings->packageSendingCallback(
							this,
							package->randomFlag(),
							nextPackage->payloadDataOriginalIndex(),
							nextPackage->payloadDataOriginalCurrentSize(),
							nextPackage->payloadDataTotalSize()
						);
						if (packages.isEmpty()) {
							m_sendPayloadPackagePool.remove(package->randomFlag());
						}
						break;
					}
				case NETWORKPACKAGE_FILEDATAREQUESTPACKGEFLAG:
					{
						const auto&& itForFile = m_waitForSendFiles.find(package->randomFlag());
						const auto&& fileIsContains = itForFile != m_waitForSendFiles.end();
						if (!fileIsContains) {
							qDebug() << "Connect::onTcpSocketReadyRead: Not contains file, randomFlag:" <<
								package->randomFlag();
							break;
						}
						const auto&& currentFileData = itForFile.value()->read(m_connectSettings->cutPackageSize);
						this->sendPackageToRemote(
							Package::createFileTransportPackage(
								{}, // empty targetActionFlag,
								{}, // empty fileInfo
								currentFileData,
								{}, // empty appendData
								package->randomFlag(),
								this->needCompressionPayloadData(currentFileData.size())
							)
						);
						NETWORK_NULLPTR_CHECK(m_connectSettings->packageSendingCallback);
						m_connectSettings->packageSendingCallback(
							this,
							package->randomFlag(),
							itForFile.value()->pos() - currentFileData.size(),
							currentFileData.size(),
							itForFile.value()->size()
						);
						if (itForFile.value()->atEnd()) {
							itForFile.value()->close();
							m_waitForSendFiles.erase(itForFile);
						}
						break;
					}
				default:
					{
						qDebug() << "Connect::onTcpSocketReadyRead: unknow packageFlag (isCompletePackage):" <<
							package->packageFlag();
						break;
					}
				}
			} else {
				switch (package->packageFlag()) {
				case NETWORKPACKAGE_PAYLOADDATATRANSPORTPACKGEFLAG:
					{
						const auto&& itForPackage = m_receivePayloadPackagePool.find(package->randomFlag());
						const auto&& packageIsCached = itForPackage != m_receivePayloadPackagePool.end();
						if (packageIsCached) {
							auto payloadCurrentIndex = (*itForPackage)->payloadDataCurrentSize();
							NETWORK_NULLPTR_CHECK(m_connectSettings->packageReceivingCallback);
							m_connectSettings->packageReceivingCallback(this, package->randomFlag(), payloadCurrentIndex,
																	   package->payloadDataCurrentSize(),
																	   package->payloadDataTotalSize());
							if (!(*itForPackage)->mixPackage(package)) {
								m_receivePayloadPackagePool.erase(itForPackage);
								return;
							}
							if ((*itForPackage)->isAbandonPackage()) {
								continue;
							}
							if ((*itForPackage)->isCompletePackage()) {
								this->onDataTransportPackageReceived(*itForPackage);
								m_receivePayloadPackagePool.erase(itForPackage);
							} else {
								this->sendDataRequestToRemote(package);
							}
						} else {
							NETWORK_NULLPTR_CHECK(m_connectSettings->packageReceivingCallback);
							m_connectSettings->packageReceivingCallback(this, package->randomFlag(), 0,
																	   package->payloadDataCurrentSize(),
																	   package->payloadDataTotalSize());
							m_receivePayloadPackagePool[package->randomFlag()] = package;
							this->sendDataRequestToRemote(package);
						}
						break;
					}
				default:
					{
						qDebug() << "Connect::onTcpSocketReadyRead: unknow packageFlag:" << package->
							packageFlag();
						break;
					}
				}
			}
		}
	}
}

void Connect::onTcpSocketConnectToHostTimeOut() {
	if (m_isAbandonTcpSocket) {
		return;
	}
	NETWORK_NULLPTR_CHECK(m_timerForConnectToHostTimeOut);
	NETWORK_NULLPTR_CHECK(m_tcpSocket);
	NETWORK_NULLPTR_CHECK(m_connectSettings->connectToHostTimeoutCallback);
	m_connectSettings->connectToHostTimeoutCallback(this);
	this->onReadyToDelete();
}

void Connect::onSendPackageCheck() {
	//    qDebug() << "onSendPackageCheck:" << QThread::currentThread() << this->thread();
	if (m_onReceivedCallbacks.isEmpty()) {
		m_timerForSendPackageCheck.clear();
	} else {
		const auto&& currentTime = QDateTime::currentMSecsSinceEpoch();
		auto it = m_onReceivedCallbacks.begin();
		while ((it != m_onReceivedCallbacks.end()) &&
			((currentTime - it->sendTime) > m_connectSettings->maximumReceivePackageWaitTime)) {
			if (it->failCallback) {
				NETWORK_NULLPTR_CHECK(m_connectSettings->waitReplyPackageFailCallback);
				m_connectSettings->waitReplyPackageFailCallback(this, it->failCallback);
			}
			m_onReceivedCallbacks.erase(it);
			it = m_onReceivedCallbacks.begin();
		}
		if (!m_onReceivedCallbacks.isEmpty()) {
			m_timerForSendPackageCheck->start();
		}
	}
}

void Connect::startTimerForConnectToHostTimeOut() {
	if (m_timerForConnectToHostTimeOut) {
		qDebug() << "startTimerForConnectToHostTimeOut: error, timer already started";
		return;
	}
	if (m_connectSettings->maximumConnectToHostWaitTime == -1) {
		return;
	}
	m_timerForConnectToHostTimeOut.reset(new QTimer);
	connect(m_timerForConnectToHostTimeOut.data(), &QTimer::timeout,
		this, &Connect::onTcpSocketConnectToHostTimeOut,
		Qt::DirectConnection);
	m_timerForConnectToHostTimeOut->setSingleShot(true);
	m_timerForConnectToHostTimeOut->start(m_connectSettings->maximumConnectToHostWaitTime);
}

void Connect::startTimerForSendPackageCheck() {
	if (m_timerForSendPackageCheck) {
		qDebug() << "startTimerForSendPackageCheck: error, timer already started";
		return;
	}
	if (m_connectSettings->maximumSendPackageWaitTime == -1) {
		return;
	}
	m_timerForSendPackageCheck.reset(new QTimer);
	connect(m_timerForSendPackageCheck.data(), &QTimer::timeout,
		this, &Connect::onSendPackageCheck,
		Qt::DirectConnection);
	m_timerForSendPackageCheck->setSingleShot(true);
	m_timerForSendPackageCheck->start(1000);
}

void Connect::onDataTransportPackageReceived(const QSharedPointer<Package>& package) {
	if ((package->randomFlag() >= m_connectSettings->randomFlagRangeStart) &&
		(package->randomFlag() < m_connectSettings->randomFlagRangeEnd)) {
		if (package->packageFlag() == NETWORKPACKAGE_FILEDATATRANSPORTPACKGEFLAG) {
			this->onFileDataTransportPackageReceived(package, false);
		}
		auto it = m_onReceivedCallbacks.find(package->randomFlag());
		if (it == m_onReceivedCallbacks.end()) {
			return;
		}
		if (it->succeedCallback) {
			NETWORK_NULLPTR_CHECK(m_connectSettings->waitReplyPackageSucceedCallback);
			m_connectSettings->waitReplyPackageSucceedCallback(this, package, it->succeedCallback);
		}
		m_onReceivedCallbacks.erase(it);
	} else {
		switch (package->packageFlag()) {
			case NETWORKPACKAGE_PAYLOADDATATRANSPORTPACKGEFLAG:
			{
				NETWORK_NULLPTR_CHECK(m_connectSettings->packageReceivedCallback);
				m_connectSettings->packageReceivedCallback(this, package);
				break;
			}
			case NETWORKPACKAGE_FILEDATATRANSPORTPACKGEFLAG:
			{
				this->onFileDataTransportPackageReceived(package, true);
				break;
			}
			default:
			{
				qDebug() << "Connect::onDataTransportPackageReceived: Unknow packageFlag:" << package->
					packageFlag();
				break;
			}
		}
	}
}

bool Connect::onFileDataTransportPackageReceived(
	const QSharedPointer<Package>& package,
	const bool& callbackOnFinish
) {
	const auto&& itForPackage = m_receivedFilePackagePool.find(package->randomFlag());
	const auto&& packageIsCached = itForPackage != m_receivedFilePackagePool.end();
	auto checkFinish = [this, packageIsCached, callbackOnFinish](const QSharedPointer<Package>& firstPackage,
		QSharedPointer<QFile>& file)-> bool {
			const auto&& fileSize = firstPackage->fileSize();
			if (file->pos() != fileSize) {
				if (!packageIsCached) {
					this->m_receivedFilePackagePool[firstPackage->randomFlag()] = { firstPackage, file };
				}
				this->sendDataRequestToRemote(firstPackage);
				return false;
			}
			const auto&& filePermissions = firstPackage->filePermissions();
			file->setPermissions(QFile::Permissions(filePermissions));
			file->close();
			file.clear();
#if ( defined Q_OS_MAC ) || ( defined __MINGW32__ ) || ( defined Q_OS_LINUX )
			utimbuf timeBuf = { static_cast<time_t>(firstPackage->fileLastReadTime().toTime_t()), static_cast<time_t>(firstPackage->fileLastModifiedTime().toTime_t()) };
			utime(firstPackage->localFilePath().toLatin1().data(), &timeBuf);
#endif
			if (callbackOnFinish) {
				NETWORK_NULLPTR_CHECK(this->m_connectSettings->packageReceivedCallback, false);
				this->m_connectSettings->packageReceivedCallback(this, firstPackage);
			}
			return true;
		};
	if (packageIsCached) {
		itForPackage.value().second->write(package->payloadData());
		itForPackage.value().second->waitForBytesWritten(m_connectSettings->maximumFileWriteWaitTime);
		return checkFinish(itForPackage.value().first, itForPackage.value().second);
	}
	const auto&& fileName = package->fileName();
	const auto&& fileSize = package->fileSize();
	NETWORK_NULLPTR_CHECK(m_connectSettings->filePathProvider, false);
	const auto&& localFilePath = m_connectSettings->filePathProvider(this, package, fileName);
	if (localFilePath.isEmpty()) {
		qDebug() << "Connect::onFileDataTransportPackageReceived: File path is empty, fileName:" << fileName;
		return false;
	}
	const auto&& localFileInfo = QFileInfo(localFilePath);
	if (!localFileInfo.dir().exists() && !localFileInfo.dir().mkpath(localFileInfo.dir().absolutePath())) {
		qDebug() << "Connect::onFileDataTransportPackageReceived: mkpath error, filePath:" << localFilePath;
		return false;
	}
	QSharedPointer<QFile> file(new QFile(localFilePath));
	if (!file->open(QIODevice::WriteOnly)) {
		qDebug() << "Connect::onFileDataTransportPackageReceived: Open file error, filePath:" << localFilePath;
		return false;
	}
	if (!file->resize(fileSize)) {
		qDebug() << "Connect::onFileDataTransportPackageReceived: File resize error, filePath:" <<
			localFilePath;
		return false;
	}
	package->setLocalFilePath(localFilePath);
	file->write(package->payloadData());
	file->waitForBytesWritten(m_connectSettings->maximumFileWriteWaitTime);
	package->clearPayloadData();
	return checkFinish(package, file);
}

void Connect::onReadyToDelete() {
	if (m_isAbandonTcpSocket) {
		return;
	}
	m_isAbandonTcpSocket = true;
	if (!m_timerForConnectToHostTimeOut) {
		m_timerForConnectToHostTimeOut.clear();
	}
	if (!m_onReceivedCallbacks.isEmpty()) {
		for (const auto& callback : m_onReceivedCallbacks) {
			if (!callback.failCallback) {
				continue;
			}
			callback.failCallback(this);
		}
	}
	NETWORK_NULLPTR_CHECK(m_tcpSocket);
	m_tcpSocket->close();
	NETWORK_NULLPTR_CHECK(m_connectSettings->readyToDeleteCallback);
	m_connectSettings->readyToDeleteCallback(this);
}

qint32 Connect::nextRandomFlag() {
	m_mutexForSend.lock();
	if (m_sendRandomFlagRotaryIndex >= m_connectSettings->randomFlagRangeEnd) {
		m_sendRandomFlagRotaryIndex = m_connectSettings->randomFlagRangeStart;
	} else {
		++m_sendRandomFlagRotaryIndex;
	}
	const auto currentRandomFlag = m_sendRandomFlagRotaryIndex;
	m_mutexForSend.unlock();
	return currentRandomFlag;
}

bool Connect::readySendPayloadData(
	const qint32& randomFlag,
	const QString& targetActionFlag,
	const QByteArray& payloadData,
	const QVariantMap& appendData,
	const ConnectPointerAndPackageSharedPointerFunction& succeedCallback,
	const ConnectPointerFunction& failCallback
) {
	auto packages = Package::createPayloadTransportPackages(
		targetActionFlag,
		payloadData,
		appendData,
		randomFlag,
		m_connectSettings->cutPackageSize,
		this->needCompressionPayloadData(payloadData.size())
	);
	if (packages.isEmpty()) {
		qDebug() << "Connect::readySendPayloadData: createPackagesFromPayloadData error";
		return false;
	}
	this->readySendPackages(randomFlag, packages, succeedCallback, failCallback);
	return true;
}

bool Connect::readySendFileData(
	const qint32& randomFlag,
	const QString& targetActionFlag,
	const QFileInfo& fileInfo,
	const QVariantMap& appendData,
	const ConnectPointerAndPackageSharedPointerFunction& succeedCallback,
	const ConnectPointerFunction& failCallback
) {
	if (m_waitForSendFiles.contains(randomFlag)) {
		qDebug() << "Connect::readySendFileData: file is sending, filePath:" << fileInfo.filePath();
		return false;
	}
	if (!fileInfo.exists()) {
		qDebug() << "Connect::readySendFileData: file not exists, filePath:" << fileInfo.filePath();
		return false;
	}
	QSharedPointer<QFile> file(new QFile(fileInfo.filePath()));
	if (!file->open(QIODevice::ReadOnly)) {
		qDebug() << "Connect::readySendFileData: file open error, filePath:" << fileInfo.filePath();
		return false;
	}
	const auto&& fileData = file->read(m_connectSettings->cutPackageSize);
	if (!file->atEnd()) {
		m_waitForSendFiles[randomFlag] = file;
	}
	auto packages = QList<QSharedPointer<Package>>(
		{
			Package::createFileTransportPackage(
				targetActionFlag,
				fileInfo,
				fileData,
				appendData,
				randomFlag,
				this->needCompressionPayloadData(fileData.size())
			)
		}
	);
	this->readySendPackages(randomFlag, packages, succeedCallback, failCallback);
	return true;
}

void Connect::readySendPackages(
	const qint32& randomFlag,
	QList<QSharedPointer<Package>>& packages,
	const ConnectPointerAndPackageSharedPointerFunction& succeedCallback,
	const ConnectPointerFunction& failCallback
) {
	if (this->thread() != QThread::currentThread()) {
		m_runOnConnectThreadCallback(
			[
				this,
					randomFlag,
					packages,
					succeedCallback,
					failCallback
			]() {
				auto buf = packages;
				this->readySendPackages(randomFlag, buf, succeedCallback, failCallback);
			}
				);
		return;
	}
	auto firstPackage = packages.first();
	this->sendPackageToRemote(firstPackage);
	if (succeedCallback || failCallback) {
		m_onReceivedCallbacks[randomFlag] =
		{
			QDateTime::currentMSecsSinceEpoch(),
			succeedCallback,
			failCallback
		};
		if (!m_timerForSendPackageCheck) {
			this->startTimerForSendPackageCheck();
		}
	}
	if (packages.size() > 1) {
		m_sendPayloadPackagePool[randomFlag].swap(packages);
		m_sendPayloadPackagePool[randomFlag].pop_front();
	}
	NETWORK_NULLPTR_CHECK(m_connectSettings->packageSendingCallback);
	m_connectSettings->packageSendingCallback(
		this,
		randomFlag,
		0,
		firstPackage->payloadDataOriginalCurrentSize(),
		firstPackage->payloadDataTotalSize()
	);
}

void Connect::sendDataRequestToRemote(const QSharedPointer<Package>& package) {
	if (m_isAbandonTcpSocket) {
		return;
	}
	NETWORK_NULLPTR_CHECK(m_tcpSocket);
	switch (package->packageFlag()) {
		case NETWORKPACKAGE_PAYLOADDATATRANSPORTPACKGEFLAG:
		{
			this->sendPackageToRemote(Package::createPayloadDataRequestPackage(package->randomFlag()));
			break;
		}
		case NETWORKPACKAGE_FILEDATATRANSPORTPACKGEFLAG:
		{
			this->sendPackageToRemote(Package::createFileDataRequestPackage(package->randomFlag()));
			break;
		}
		default:
		{
			qDebug() << "Connect::realSendDataRequest: Unknow packageFlag:" << package->packageFlag();
			break;
		}
	}
}

void Connect::sendPackageToRemote(const QSharedPointer<Package>& package) {
	const auto&& buffer = package->toByteArray();
	m_waitForSendBytes += buffer.size();
	m_tcpSocket->write(buffer);
}
