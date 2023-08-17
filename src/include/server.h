
#ifndef NETWORK_INCLUDE_NETWORK_SERVER_H_
#define NETWORK_INCLUDE_NETWORK_SERVER_H_

#include "foundation.h"
struct ServerSettings
{
	QString dutyMark;
	QHostAddress listenAddress = QHostAddress::Any;
	quint16 listenPort = 0;
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
	int globalServerThreadCount = 1;
	int globalSocketThreadCount = NETWORK_ADVISE_THREADCOUNT;
	int globalCallbackThreadCount = NETWORK_ADVISE_THREADCOUNT;
};
class Server : public QObject
{
	Q_OBJECT
	Q_DISABLE_COPY(Server)
public:
	Server(
		QSharedPointer<ServerSettings> serverSettings,
		QSharedPointer<ConnectPoolSettings> connectPoolSettings,
		QSharedPointer<ConnectSettings> connectSettings);
	~Server() override;
	static QSharedPointer<Server> createServer(
		const quint16 &listenPort,
		const QHostAddress &listenAddress = QHostAddress::Any,
		const bool &fileTransferEnabled = false);
	inline QSharedPointer<ServerSettings> serverSettings()
	{
		return serverSettings_;
	}
	inline QSharedPointer<ConnectPoolSettings> connectPoolSettings()
	{
		return connectPoolSettings_;
	}
	inline QSharedPointer<ConnectSettings> connectSettings()
	{
		return connectSettings_;
	}
	inline QString nodeMarkSummary() const
	{
		return nodeMarkSummary_;
	}
	bool begin();
	void registerProcessor(const QPointer<Processor> &processor);
	inline QSet<QString> availableProcessorMethodNames() const
	{
		auto l = processorCallbacks_.keys();
		return QSet<QString>(l.begin(), l.end());
	}

private:
	void incomingConnection(const qintptr &socketDescriptor);
	inline void onConnectToHostError(const QPointer<Connect> &connect,
									 const QPointer<ConnectPool> &connectPool)
	{
		if (!serverSettings_->connectToHostErrorCallback)
		{
			return;
		}
		callbackThreadPool_->run(
			[connect,
			 callback = serverSettings_->connectToHostErrorCallback]()
			{
				callback(connect);
			});
	}
	inline void onConnectToHostTimeout(const QPointer<Connect> &connect,
									   const QPointer<ConnectPool> &connectPool)
	{
		if (!serverSettings_->connectToHostTimeoutCallback)
		{
			return;
		}
		callbackThreadPool_->run(
			[connect,
			 callback = serverSettings_->connectToHostTimeoutCallback]()
			{
				callback(connect);
			});
	}
	inline void onConnectToHostSucceed(const QPointer<Connect> &connect,
									   const QPointer<ConnectPool> &connectPool)
	{
		if (!serverSettings_->connectToHostSucceedCallback)
		{
			return;
		}
		callbackThreadPool_->run(
			[connect,
			 callback = serverSettings_->connectToHostSucceedCallback]()
			{
				callback(connect);
			});
	}
	inline void onRemoteHostClosed(const QPointer<Connect> &connect,
								   const QPointer<ConnectPool> &connectPool)
	{
		if (!serverSettings_->remoteHostClosedCallback)
		{
			return;
		}
		callbackThreadPool_->run(
			[connect,
			 callback = serverSettings_->remoteHostClosedCallback]()
			{
				callback(connect);
			});
	}
	inline void onReadyToDelete(const QPointer<Connect> &connect, const QPointer<ConnectPool> &connectPool)
	{
		if (!serverSettings_->readyToDeleteCallback)
		{
			return;
		}
		callbackThreadPool_->run(
			[connect,
			 callback = serverSettings_->readyToDeleteCallback]()
			{
				callback(connect);
			});
	}
	inline void onPackageSending(
		const QPointer<Connect> &connect,
		const QPointer<ConnectPool> &connectPool,
		const qint32 &randomFlag,
		const qint64 &payloadCurrentIndex,
		const qint64 &payloadCurrentSize,
		const qint64 &payloadTotalSize)
	{
		if (!serverSettings_->packageSendingCallback)
		{
			return;
		}
		callbackThreadPool_->run(
			[connect,
			 callback = serverSettings_->packageSendingCallback,
			 randomFlag,
			 payloadCurrentIndex,
			 payloadCurrentSize,
			 payloadTotalSize]()
			{
				callback(connect, randomFlag, payloadCurrentIndex, payloadCurrentSize, payloadTotalSize);
			});
    }
    inline void onPackageReceiving(
        const QPointer<Connect>& connect,
        const QPointer<ConnectPool>&,
        const qint32& randomFlag,
        const qint64& payloadCurrentIndex,
        const qint64& payloadCurrentSize,
        const qint64& payloadTotalSize
        )
    {
        if (!serverSettings_->packageReceivingCallback) { return; }
        callbackThreadPool_->run(
            [
                connect,
                callback = serverSettings_->packageReceivingCallback,
                randomFlag,
                payloadCurrentIndex,
                payloadCurrentSize,
                payloadTotalSize
        ]()
            {
                callback(connect, randomFlag, payloadCurrentIndex, payloadCurrentSize, payloadTotalSize);
            }
            );
    }
	void onPackageReceived(
		const QPointer<Connect> &connect,
        const QPointer<ConnectPool> &connectPool,
        const QSharedPointer<Package> &package);

private:
	// Thread pool
	static QWeakPointer<NetworkThreadPool> globalServerThreadPool_;
	QSharedPointer<NetworkThreadPool> serverThreadPool_;
	static QWeakPointer<NetworkThreadPool> globalSocketThreadPool_;
	QSharedPointer<NetworkThreadPool> socketThreadPool_;
	static QWeakPointer<NetworkThreadPool> globalCallbackThreadPool_;
	QSharedPointer<NetworkThreadPool> callbackThreadPool_;
	// Settings
	QSharedPointer<ServerSettings> serverSettings_;
	QSharedPointer<ConnectPoolSettings> connectPoolSettings_;
	QSharedPointer<ConnectSettings> connectSettings_;
	// Server
	QSharedPointer<QTcpServer> tcpServer_;
	QMap<QThread *, QSharedPointer<ConnectPool>> connectPools_;
	// Processor
	QSet<Processor *> processors_;
	QMap<QString, std::function<void(const QPointer<Connect> &, const QSharedPointer<Package> &)>>
		processorCallbacks_;
	// Other
	QString nodeMarkSummary_;
};

#endif // NETWORK_INCLUDE_NETWORK_SERVER_H_
