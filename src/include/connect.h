
#ifndef NETWORK_INCLUDE_NETWORK_CONNECT_H_
#define NETWORK_INCLUDE_NETWORK_CONNECT_H_

#include "foundation.h"
struct ConnectSettings
{
	bool longConnection = true;
	bool autoMaintainLongConnection = false;
	int streamFormat = -1;
	qint64 cutPackageSize = NETWORKPACKAGE_ADVISE_CUTPACKAGESIZE;
	qint64 packageCompressionMinimumBytes = 1024;
	int packageCompressionThresholdForConnectSucceedElapsed = 500;
	qint64 maximumSendForTotalByteCount = -1;	 // reserve
	qint64 maximumSendPackageByteCount = -1;	 // reserve
	int maximumSendSpeed = -1;					 // Byte/s reserve
	qint64 maximumReceiveForTotalByteCount = -1; // reserve
	qint64 maximumReceivePackageByteCount = -1;	 // reserve
	int maximumReceiveSpeed = -1;				 // Byte/s reserve
	bool fileTransferEnabled = false;
	qint32 randomFlagRangeStart = -1;
	qint32 randomFlagRangeEnd = -1;
	int maximumConnectToHostWaitTime = 15 * 1000;
	int maximumSendPackageWaitTime = 30 * 1000;
	int maximumReceivePackageWaitTime = 30 * 1000;
	int maximumFileWriteWaitTime = 30 * 1000;
	int maximumConnectionTime = -1;
	std::function<void(const QPointer<Connect> &)> connectToHostErrorCallback = nullptr;
	std::function<void(const QPointer<Connect> &)> connectToHostTimeoutCallback = nullptr;
	std::function<void(const QPointer<Connect> &)> connectToHostSucceedCallback = nullptr;
	std::function<void(const QPointer<Connect> &)> remoteHostClosedCallback = nullptr;
	std::function<void(const QPointer<Connect> &)> readyToDeleteCallback = nullptr;
	std::function<void(const QPointer<Connect> &, const qint32 &, const qint64 &, const qint64 &, const qint64 &)>
		packageSendingCallback = nullptr;
	std::function<void(const QPointer<Connect> &, const qint32 &, const qint64 &, const qint64 &, const qint64 &)>
		packageReceivingCallback = nullptr;
	std::function<void(const QPointer<Connect> &, const QSharedPointer<Package> &)> packageReceivedCallback =
		nullptr;
	std::function<void(const QPointer<Connect> &, const QSharedPointer<Package> &,
					   const ConnectPointerAndPackageSharedPointerFunction &)>
		waitReplyPackageSucceedCallback =
			nullptr;
	std::function<void(const QPointer<Connect> &,
					   const std::function<void(const QPointer<Connect> &connect)> &)>
		waitReplyPackageFailCallback = nullptr;
	std::function<QString(const QPointer<Connect> &, const QSharedPointer<Package> &, const QString &)>
		filePathProvider = nullptr;
	void setFilePathProviderToDefaultDir();
	void setFilePathProviderToDir(const QDir &dir);
};
class Connect : public QObject
{
	Q_OBJECT
	Q_DISABLE_COPY(Connect)
private:
	struct ReceivedCallbackPackage
	{
		qint64 sendTime;
		ConnectPointerAndPackageSharedPointerFunction succeedCallback;
		ConnectPointerFunction failCallback;
	};

private:
	Connect(const QSharedPointer<ConnectSettings> &connectSettings);

public:
	~Connect() override = default;
	static void createConnect(
		const std::function<void(const QSharedPointer<Connect> &)> &onConnectCreatedCallback,
		const std::function<void(std::function<void()>)> &runOnConnectThreadCallback,
		const QSharedPointer<ConnectSettings> &connectSettings,
		const QString &hostName,
		const quint16 &port);
	static void createConnect(
		const std::function<void(const QSharedPointer<Connect> &)> &onConnectCreatedCallback,
		const std::function<void(std::function<void()>)> &runOnConnectThreadCallback,
		const QSharedPointer<ConnectSettings> &connectSettings,
		const qintptr &socketDescriptor);
	inline QWeakPointer<QTcpSocket> tcpSocket()
	{
		return tcpSocket_.toWeakRef();
	}
	inline bool onceConnectSucceed() const
	{
		return onceConnectSucceed_;
	}
	inline bool isAbandonTcpSocket() const
	{
		return isAbandonTcpSocket_;
	}
	inline qint64 connectCreateTime() const
	{
		return connectCreateTime_;
	}
	inline qint64 connectSucceedTime() const
	{
		return connectSucceedTime_;
	}
	inline qint64 waitForSendBytes() const
	{
		return waitForSendBytes_;
	}
	inline qint64 alreadyWrittenBytes() const
	{
		return alreadyWrittenBytes_;
	}
	inline qint64 connectSucceedElapsed() const
	{
		if (!connectSucceedTime_)
		{
			return -1;
		}
		return connectSucceedTime_ - connectCreateTime_;
	}
	void close();
	qint32 sendPayloadData(
		const QString &targetActionFlag,
		const QByteArray &payloadData,
		const QVariantMap &appendData,
		const ConnectPointerAndPackageSharedPointerFunction &succeedCallback = nullptr,
		const ConnectPointerFunction &failCallback = nullptr);
	inline qint32 sendPayloadData(
		const QByteArray &payloadData,
        const ConnectPointerAndPackageSharedPointerFunction &succeedCallback = nullptr,
        const ConnectPointerFunction &failCallback = nullptr)
	{
		return this->sendPayloadData(
			{}, // empty targetActionFlag
			payloadData,
			{}, // empty appendData
			succeedCallback,
			failCallback);
	}
	qint32 sendVariantMapData(
		const QString &targetActionFlag,
		const QVariantMap &variantMap,
		const QVariantMap &appendData,
		const ConnectPointerAndPackageSharedPointerFunction &succeedCallback = nullptr,
		const ConnectPointerFunction &failCallback = nullptr);
	inline qint32 sendVariantMapData(
		const QVariantMap &variantMap,
		const ConnectPointerAndPackageSharedPointerFunction &succeedCallback,
		const ConnectPointerFunction &failCallback)
	{
		return this->sendVariantMapData(
			{}, // empty targetActionFlag
			variantMap,
			{}, // empty appendData
			succeedCallback,
			failCallback);
	}
	qint32 sendFileData(
		const QString &targetActionFlag,
		const QFileInfo &fileInfo,
		const QVariantMap &appendData,
		const ConnectPointerAndPackageSharedPointerFunction &succeedCallback = nullptr,
		const ConnectPointerFunction &failCallback = nullptr);
	inline qint32 sendFileData(
		const QFileInfo &fileInfo,
		const ConnectPointerAndPackageSharedPointerFunction &succeedCallback,
		const ConnectPointerFunction &failCallback)
	{
		return this->sendFileData(
			{}, // empty targetActionFlag
			fileInfo,
			{}, // empty appendData
			succeedCallback,
			failCallback);
	}
	qint32 replyPayloadData(
		const qint32 &receivedPackageRandomFlag,
		const QByteArray &payloadData,
		const QVariantMap &appendData = QVariantMap());
	qint32 replyVariantMapData(
		const qint32 &receivedPackageRandomFlag,
		const QVariantMap &variantMap,
		const QVariantMap &appendData = QVariantMap());
	qint32 replyFile(
		const qint32 &receivedPackageRandomFlag,
		const QFileInfo &fileInfo,
		const QVariantMap &appendData = QVariantMap());
	bool putPayloadData(
		const QString &targetActionFlag,
		const QByteArray &payloadData,
		const QVariantMap &appendData = QVariantMap());
	inline bool putPayloadData(
		const QByteArray &payloadData,
		const QVariantMap &appendData)
	{
		return this->putPayloadData(
			{}, // empty targetActionFlag,
			payloadData,
			appendData);
	}

	bool putVariantMapData(
		const QString &targetActionFlag,
		const QVariantMap &variantMap,
		const QVariantMap &appendData = QVariantMap());
	inline bool putVariantMapData(
		const QVariantMap &variantMap,
		const QVariantMap &appendData)
	{
		return this->putVariantMapData(
			{}, // empty targetActionFlag,
			variantMap,
			appendData);
	}
	bool putFile(
		const QString &targetActionFlag,
		const QFileInfo &fileInfo,
		const QVariantMap &appendData = QVariantMap());
	inline bool putFile(
		const QFileInfo &fileInfo,
		const QVariantMap &appendData)
	{
		return this->putFile(
			{}, // empty targetActionFlag,
			fileInfo,
			appendData);
	}

private Q_SLOTS:
	void onTcpSocketStateChanged();
	void onTcpSocketBytesWritten(const qint64 &bytes);
	void onTcpSocketReadyRead();
	void onTcpSocketConnectToHostTimeOut();
	void onSendPackageCheck();

private:
	void startTimerForConnectToHostTimeOut();
	void startTimerForSendPackageCheck();
	void onDataTransportPackageReceived(const QSharedPointer<Package> &package);
	bool onFileDataTransportPackageReceived(
		const QSharedPointer<Package> &package,
		const bool &callbackOnFinish);
	void onReadyToDelete();
	qint32 nextRandomFlag();
	inline bool needCompressionPayloadData(const int &dataSize)
	{
		bool compressionPayloadData = false;
		if (connectSettings_->packageCompressionThresholdForConnectSucceedElapsed != -1)
		{
			if (this->connectSucceedElapsed() >= connectSettings_->packageCompressionThresholdForConnectSucceedElapsed)
			{
				compressionPayloadData = true;
			}
			if ((connectSettings_->packageCompressionMinimumBytes != -1) &&
				(dataSize < connectSettings_->packageCompressionMinimumBytes))
			{
				compressionPayloadData = false;
			}
		}
		return compressionPayloadData;
	}
	bool readySendPayloadData(
		const qint32 &randomFlag,
		const QString &targetActionFlag,
		const QByteArray &payloadData,
		const QVariantMap &appendData,
		const ConnectPointerAndPackageSharedPointerFunction &succeedCallback,
		const ConnectPointerFunction &failCallback);
	bool readySendFileData(
		const qint32 &randomFlag,
		const QString &targetActionFlag,
		const QFileInfo &fileInfo,
		const QVariantMap &appendData,
		const ConnectPointerAndPackageSharedPointerFunction &succeedCallback,
		const ConnectPointerFunction &failCallback);
	void readySendPackages(
		const qint32 &randomFlag,
		QList<QSharedPointer<Package>> &packages,
		const ConnectPointerAndPackageSharedPointerFunction &succeedCallback,
		const ConnectPointerFunction &failCallback);
	void sendDataRequestToRemote(const QSharedPointer<Package> &package);
	void sendPackageToRemote(const QSharedPointer<Package> &package);

private:
	// Settings
	QSharedPointer<ConnectSettings> connectSettings_;
	std::function<void(std::function<void()>)> runOnConnectThreadCallback_;
	// Socket
	QSharedPointer<QTcpSocket> tcpSocket_;
	bool onceConnectSucceed_ = false;
	bool isAbandonTcpSocket_ = false;
	QByteArray tcpSocketBuffer_;
	// Timer
	QSharedPointer<QTimer> timerForConnectToHostTimeOut_;
	QSharedPointer<QTimer> timerForSendPackageCheck_;
	// Package
	QMutex mutexForSend_;
	qint32 sendRandomFlagRotaryIndex_ = 0;
	QMap<qint32, ReceivedCallbackPackage> onReceivedCallbacks_; // randomFlag -> package
	// Payload
	QMap<qint32, QList<QSharedPointer<Package>>> sendPayloadPackagePool_; // randomFlag -> package
	QMap<qint32, QSharedPointer<Package>> receivePayloadPackagePool_;	  // randomFlag -> package
	// File
	QMap<qint32, QSharedPointer<QFile>> waitForSendFiles_; // randomFlag -> file
	QMap<qint32, QPair<QSharedPointer<Package>, QSharedPointer<QFile>>> receivedFilePackagePool_;
	// randomFlag -> { package, file }
	// Statistics
	qint64 connectCreateTime_ = 0;
	qint64 connectSucceedTime_ = 0;
	qint64 waitForSendBytes_ = 0;
	qint64 alreadyWrittenBytes_ = 0;
};

#endif // NETWORK_INCLUDE_NETWORK_CONNECT_H_
