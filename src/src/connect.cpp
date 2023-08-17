
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
	connectSettings_(connectSettings),
	tcpSocket_(new QTcpSocket),
	connectCreateTime_(QDateTime::currentMSecsSinceEpoch()) {
	connect(tcpSocket_.data(), &QAbstractSocket::stateChanged, this, &Connect::onTcpSocketStateChanged,
		Qt::DirectConnection);
	connect(tcpSocket_.data(), &QAbstractSocket::bytesWritten, this, &Connect::onTcpSocketBytesWritten,
		Qt::DirectConnection);
	connect(tcpSocket_.data(), &QTcpSocket::readyRead, this, &Connect::onTcpSocketReadyRead,
		Qt::DirectConnection);
	if (connectSettings_->fileTransferEnabled && !connectSettings_->filePathProvider) {
		connectSettings_->setFilePathProviderToDefaultDir();
		qDebug() << "Connect: fileTransfer is enabled, but filePathProvider is null, use default dir:"
			<< connectSettings_->filePathProvider(QPointer<Connect>(nullptr),
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
	newConnect->runOnConnectThreadCallback_ = runOnConnectThreadCallback;
	newConnect->sendRandomFlagRotaryIndex_ = connectSettings->randomFlagRangeStart - 1;
	NETWORK_NULLPTR_CHECK(onConnectCreatedCallback);
	onConnectCreatedCallback(newConnect);
	newConnect->startTimerForConnectToHostTimeOut();
	newConnect->tcpSocket_->setProxy(QNetworkProxy::NoProxy);
	newConnect->tcpSocket_->connectToHost(hostName, port);
}
void Connect::createConnect(
	const std::function<void(const QSharedPointer<Connect>&)>& onConnectCreatedCallback,
	const std::function<void(std::function<void()>)>& runOnConnectThreadCallback,
	const QSharedPointer<ConnectSettings>& connectSettings,
	const qintptr& socketDescriptor
) {
	QSharedPointer<Connect> newConnect(new Connect(connectSettings));
	newConnect->runOnConnectThreadCallback_ = runOnConnectThreadCallback;
	newConnect->sendRandomFlagRotaryIndex_ = connectSettings->randomFlagRangeStart - 1;
	NETWORK_NULLPTR_CHECK(onConnectCreatedCallback);
	onConnectCreatedCallback(newConnect);
	newConnect->startTimerForConnectToHostTimeOut();
	newConnect->tcpSocket_->setSocketDescriptor(socketDescriptor);
}
void Connect::close() {
	NETWORK_THISNULL_CHECK("Connect::close");
	if (isAbandonTcpSocket_) {
		return;
	}
	NETWORK_NULLPTR_CHECK(tcpSocket_);
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
	if (isAbandonTcpSocket_) {
		return 0;
	}
	NETWORK_NULLPTR_CHECK(runOnConnectThreadCallback_, 0);
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
	if (isAbandonTcpSocket_) {
		return 0;
	}
	NETWORK_NULLPTR_CHECK(runOnConnectThreadCallback_, 0);
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
	if (isAbandonTcpSocket_) {
		return 0;
	}
	NETWORK_NULLPTR_CHECK(runOnConnectThreadCallback_, 0);
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
	if (isAbandonTcpSocket_) {
		return 0;
	}
	NETWORK_NULLPTR_CHECK(runOnConnectThreadCallback_, 0);
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
	if (isAbandonTcpSocket_) {
		return false;
	}
	NETWORK_NULLPTR_CHECK(runOnConnectThreadCallback_, 0);
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
	if (isAbandonTcpSocket_) {
		return false;
	}
	NETWORK_NULLPTR_CHECK(runOnConnectThreadCallback_, 0);
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
	if (isAbandonTcpSocket_) {
		return;
	}
	NETWORK_NULLPTR_CHECK(tcpSocket_);
	const auto&& state = tcpSocket_->state();
	//    qDebug() << "onTcpSocketStateChanged:" << this << ": state:" << state;
	switch (state) {
	case QAbstractSocket::ConnectedState:
	{
		if (!timerForConnectToHostTimeOut_.isNull()) {
			timerForConnectToHostTimeOut_.clear();
		}
		NETWORK_NULLPTR_CHECK(connectSettings_->connectToHostSucceedCallback);
		connectSettings_->connectToHostSucceedCallback(this);
		onceConnectSucceed_ = true;
		connectSucceedTime_ = QDateTime::currentMSecsSinceEpoch();
		break;
	}
	case QAbstractSocket::UnconnectedState:
	{
		//            qDebug() << "onTcpSocketStateChanged:" << this << ": UnconnectedState: error:" << tcpSocket_->error();
		switch (tcpSocket_->error()) {
		case QAbstractSocket::UnknownSocketError:
		{
			if (onceConnectSucceed_) {
				break;
			}
			NETWORK_NULLPTR_CHECK(connectSettings_->connectToHostErrorCallback);
			connectSettings_->connectToHostErrorCallback(this);
			break;
		}
		case QAbstractSocket::RemoteHostClosedError:
		{
			NETWORK_NULLPTR_CHECK(connectSettings_->remoteHostClosedCallback);
			connectSettings_->remoteHostClosedCallback(this);
			break;
		}
		case QAbstractSocket::HostNotFoundError:
		case QAbstractSocket::ConnectionRefusedError:
		{
			NETWORK_NULLPTR_CHECK(connectSettings_->connectToHostErrorCallback);
			connectSettings_->connectToHostErrorCallback(this);
			break;
		}
		case QAbstractSocket::NetworkError:
		{
			break;
		}
		default:
		{
			qDebug() << "onTcpSocketStateChanged: unknow error:" << tcpSocket_->error();
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
	if (isAbandonTcpSocket_) {
		return;
	}
	NETWORK_NULLPTR_CHECK(tcpSocket_);
	waitForSendBytes_ -= bytes;
	alreadyWrittenBytes_ += bytes;
	//    qDebug() << "onTcpSocketBytesWritten:" << waitForSendBytes_ << alreadyWrittenBytes_ << QThread::currentThread();
}
void Connect::onTcpSocketReadyRead() {
	if (isAbandonTcpSocket_) {
		return;
	}
	NETWORK_NULLPTR_CHECK(tcpSocket_);
	const auto&& data = tcpSocket_->readAll();
	tcpSocketBuffer_.append(data);
	//    qDebug() << tcpSocketBuffer_.size() << data.size();
	forever
	{
		const auto && checkReply = Package::checkDataIsReadyReceive(tcpSocketBuffer_);
		if (checkReply > 0) {
			return;
		}
		if (checkReply < 0) {
			tcpSocketBuffer_.remove(0, checkReply * -1);
		} else {
			auto package = Package::readPackage(tcpSocketBuffer_);
			if (package->isCompletePackage()) {
				switch (package->packageFlag()) {
				case NETWORKPACKAGE_PAYLOADDATATRANSPORTPACKGEFLAG:
				case NETWORKPACKAGE_FILEDATATRANSPORTPACKGEFLAG:
					{
						NETWORK_NULLPTR_CHECK(connectSettings_->packageReceivingCallback);
						connectSettings_->packageReceivingCallback(this, package->randomFlag(), 0,
																   package->payloadDataCurrentSize(),
																   package->payloadDataTotalSize());
						this->onDataTransportPackageReceived(package);
						break;
					}
				case NETWORKPACKAGE_PAYLOADDATAREQUESTPACKGEFLAG:
					{
						if (!sendPayloadPackagePool_.contains(package->randomFlag())) {
							qDebug() << "Connect::onTcpSocketReadyRead: no contains randonFlag:" << package->
								randomFlag();
							break;
						}
						auto& packages = sendPayloadPackagePool_[package->randomFlag()];
						if (packages.isEmpty()) {
							qDebug() << "Connect::onTcpSocketReadyRead: packages is empty:" << package->
								randomFlag();
							break;
						}
						auto nextPackage = packages.first();
						packages.pop_front();
						this->sendPackageToRemote(nextPackage);
						NETWORK_NULLPTR_CHECK(connectSettings_->packageSendingCallback);
						connectSettings_->packageSendingCallback(
							this,
							package->randomFlag(),
							nextPackage->payloadDataOriginalIndex(),
							nextPackage->payloadDataOriginalCurrentSize(),
							nextPackage->payloadDataTotalSize()
						);
						if (packages.isEmpty()) {
							sendPayloadPackagePool_.remove(package->randomFlag());
						}
						break;
					}
				case NETWORKPACKAGE_FILEDATAREQUESTPACKGEFLAG:
					{
						const auto&& itForFile = waitForSendFiles_.find(package->randomFlag());
						const auto&& fileIsContains = itForFile != waitForSendFiles_.end();
						if (!fileIsContains) {
							qDebug() << "Connect::onTcpSocketReadyRead: Not contains file, randomFlag:" <<
								package->randomFlag();
							break;
						}
						const auto&& currentFileData = itForFile.value()->read(connectSettings_->cutPackageSize);
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
						NETWORK_NULLPTR_CHECK(connectSettings_->packageSendingCallback);
						connectSettings_->packageSendingCallback(
							this,
							package->randomFlag(),
							itForFile.value()->pos() - currentFileData.size(),
							currentFileData.size(),
							itForFile.value()->size()
						);
						if (itForFile.value()->atEnd()) {
							itForFile.value()->close();
							waitForSendFiles_.erase(itForFile);
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
						const auto&& itForPackage = receivePayloadPackagePool_.find(package->randomFlag());
						const auto&& packageIsCached = itForPackage != receivePayloadPackagePool_.end();
						if (packageIsCached) {
							auto payloadCurrentIndex = (*itForPackage)->payloadDataCurrentSize();
							NETWORK_NULLPTR_CHECK(connectSettings_->packageReceivingCallback);
							connectSettings_->packageReceivingCallback(this, package->randomFlag(), payloadCurrentIndex,
																	   package->payloadDataCurrentSize(),
																	   package->payloadDataTotalSize());
							if (!(*itForPackage)->mixPackage(package)) {
								receivePayloadPackagePool_.erase(itForPackage);
								return;
							}
							if ((*itForPackage)->isAbandonPackage()) {
								continue;
							}
							if ((*itForPackage)->isCompletePackage()) {
								this->onDataTransportPackageReceived(*itForPackage);
								receivePayloadPackagePool_.erase(itForPackage);
							} else {
								this->sendDataRequestToRemote(package);
							}
						} else {
							NETWORK_NULLPTR_CHECK(connectSettings_->packageReceivingCallback);
							connectSettings_->packageReceivingCallback(this, package->randomFlag(), 0,
																	   package->payloadDataCurrentSize(),
																	   package->payloadDataTotalSize());
							receivePayloadPackagePool_[package->randomFlag()] = package;
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
	if (isAbandonTcpSocket_) {
		return;
	}
	NETWORK_NULLPTR_CHECK(timerForConnectToHostTimeOut_);
	NETWORK_NULLPTR_CHECK(tcpSocket_);
	NETWORK_NULLPTR_CHECK(connectSettings_->connectToHostTimeoutCallback);
	connectSettings_->connectToHostTimeoutCallback(this);
	this->onReadyToDelete();
}
void Connect::onSendPackageCheck() {
	//    qDebug() << "onSendPackageCheck:" << QThread::currentThread() << this->thread();
	if (onReceivedCallbacks_.isEmpty()) {
		timerForSendPackageCheck_.clear();
	} else {
		const auto&& currentTime = QDateTime::currentMSecsSinceEpoch();
		auto it = onReceivedCallbacks_.begin();
		while ((it != onReceivedCallbacks_.end()) &&
			((currentTime - it->sendTime) > connectSettings_->maximumReceivePackageWaitTime)) {
			if (it->failCallback) {
				NETWORK_NULLPTR_CHECK(connectSettings_->waitReplyPackageFailCallback);
				connectSettings_->waitReplyPackageFailCallback(this, it->failCallback);
			}
			onReceivedCallbacks_.erase(it);
			it = onReceivedCallbacks_.begin();
		}
		if (!onReceivedCallbacks_.isEmpty()) {
			timerForSendPackageCheck_->start();
		}
	}
}
void Connect::startTimerForConnectToHostTimeOut() {
	if (timerForConnectToHostTimeOut_) {
		qDebug() << "startTimerForConnectToHostTimeOut: error, timer already started";
		return;
	}
	if (connectSettings_->maximumConnectToHostWaitTime == -1) {
		return;
	}
	timerForConnectToHostTimeOut_.reset(new QTimer);
	connect(timerForConnectToHostTimeOut_.data(), &QTimer::timeout,
		this, &Connect::onTcpSocketConnectToHostTimeOut,
		Qt::DirectConnection);
	timerForConnectToHostTimeOut_->setSingleShot(true);
	timerForConnectToHostTimeOut_->start(connectSettings_->maximumConnectToHostWaitTime);
}
void Connect::startTimerForSendPackageCheck() {
	if (timerForSendPackageCheck_) {
		qDebug() << "startTimerForSendPackageCheck: error, timer already started";
		return;
	}
	if (connectSettings_->maximumSendPackageWaitTime == -1) {
		return;
	}
	timerForSendPackageCheck_.reset(new QTimer);
	connect(timerForSendPackageCheck_.data(), &QTimer::timeout,
		this, &Connect::onSendPackageCheck,
		Qt::DirectConnection);
	timerForSendPackageCheck_->setSingleShot(true);
	timerForSendPackageCheck_->start(1000);
}
void Connect::onDataTransportPackageReceived(const QSharedPointer<Package>& package) {
	if ((package->randomFlag() >= connectSettings_->randomFlagRangeStart) &&
		(package->randomFlag() < connectSettings_->randomFlagRangeEnd)) {
		if (package->packageFlag() == NETWORKPACKAGE_FILEDATATRANSPORTPACKGEFLAG) {
			this->onFileDataTransportPackageReceived(package, false);
		}
		auto it = onReceivedCallbacks_.find(package->randomFlag());
		if (it == onReceivedCallbacks_.end()) {
			return;
		}
		if (it->succeedCallback) {
			NETWORK_NULLPTR_CHECK(connectSettings_->waitReplyPackageSucceedCallback);
			connectSettings_->waitReplyPackageSucceedCallback(this, package, it->succeedCallback);
		}
		onReceivedCallbacks_.erase(it);
	} else {
		switch (package->packageFlag()) {
		case NETWORKPACKAGE_PAYLOADDATATRANSPORTPACKGEFLAG:
		{
			NETWORK_NULLPTR_CHECK(connectSettings_->packageReceivedCallback);
			connectSettings_->packageReceivedCallback(this, package);
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
	const auto&& itForPackage = receivedFilePackagePool_.find(package->randomFlag());
	const auto&& packageIsCached = itForPackage != receivedFilePackagePool_.end();
	auto checkFinish = [this, packageIsCached, callbackOnFinish](const QSharedPointer<Package>& firstPackage,
		QSharedPointer<QFile>& file)-> bool {
			const auto&& fileSize = firstPackage->fileSize();
			if (file->pos() != fileSize) {
				if (!packageIsCached) {
					this->receivedFilePackagePool_[firstPackage->randomFlag()] = { firstPackage, file };
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
				NETWORK_NULLPTR_CHECK(this->connectSettings_->packageReceivedCallback, false);
				this->connectSettings_->packageReceivedCallback(this, firstPackage);
			}
			return true;
	};
	if (packageIsCached) {
		itForPackage.value().second->write(package->payloadData());
		itForPackage.value().second->waitForBytesWritten(connectSettings_->maximumFileWriteWaitTime);
		return checkFinish(itForPackage.value().first, itForPackage.value().second);
	}
	const auto&& fileName = package->fileName();
	const auto&& fileSize = package->fileSize();
	NETWORK_NULLPTR_CHECK(connectSettings_->filePathProvider, false);
	const auto&& localFilePath = connectSettings_->filePathProvider(this, package, fileName);
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
	file->waitForBytesWritten(connectSettings_->maximumFileWriteWaitTime);
	package->clearPayloadData();
	return checkFinish(package, file);
}
void Connect::onReadyToDelete() {
	if (isAbandonTcpSocket_) {
		return;
	}
	isAbandonTcpSocket_ = true;
	if (!timerForConnectToHostTimeOut_) {
		timerForConnectToHostTimeOut_.clear();
	}
	if (!onReceivedCallbacks_.isEmpty()) {
		for (const auto& callback : onReceivedCallbacks_) {
			if (!callback.failCallback) {
				continue;
			}
			callback.failCallback(this);
		}
	}
	NETWORK_NULLPTR_CHECK(tcpSocket_);
	tcpSocket_->close();
	NETWORK_NULLPTR_CHECK(connectSettings_->readyToDeleteCallback);
	connectSettings_->readyToDeleteCallback(this);
}
qint32 Connect::nextRandomFlag() {
	mutexForSend_.lock();
	if (sendRandomFlagRotaryIndex_ >= connectSettings_->randomFlagRangeEnd) {
		sendRandomFlagRotaryIndex_ = connectSettings_->randomFlagRangeStart;
	} else {
		++sendRandomFlagRotaryIndex_;
	}
	const auto currentRandomFlag = sendRandomFlagRotaryIndex_;
	mutexForSend_.unlock();
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
		connectSettings_->cutPackageSize,
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
	if (waitForSendFiles_.contains(randomFlag)) {
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
	const auto&& fileData = file->read(connectSettings_->cutPackageSize);
	if (!file->atEnd()) {
		waitForSendFiles_[randomFlag] = file;
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
		runOnConnectThreadCallback_(
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
		onReceivedCallbacks_[randomFlag] =
		{
			QDateTime::currentMSecsSinceEpoch(),
			succeedCallback,
			failCallback
		};
		if (!timerForSendPackageCheck_) {
			this->startTimerForSendPackageCheck();
		}
	}
	if (packages.size() > 1) {
		sendPayloadPackagePool_[randomFlag].swap(packages);
		sendPayloadPackagePool_[randomFlag].pop_front();
	}
	NETWORK_NULLPTR_CHECK(connectSettings_->packageSendingCallback);
	connectSettings_->packageSendingCallback(
		this,
		randomFlag,
		0,
		firstPackage->payloadDataOriginalCurrentSize(),
		firstPackage->payloadDataTotalSize()
	);
}
void Connect::sendDataRequestToRemote(const QSharedPointer<Package>& package) {
	if (isAbandonTcpSocket_) {
		return;
	}
	NETWORK_NULLPTR_CHECK(tcpSocket_);
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
	waitForSendBytes_ += buffer.size();
	tcpSocket_->write(buffer);
}
