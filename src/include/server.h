
#ifndef NETWORK_INCLUDE_NETWORK_SERVER_H_
#define NETWORK_INCLUDE_NETWORK_SERVER_H_

#include "foundation.h"
struct ServerSettings {
	QString dutyMark;
	QHostAddress listenAddress = QHostAddress::Any;
	quint16 listenPort = 0;
	std::function<void(const QPointer<Connect>&)> connectToHostErrorCallback = nullptr;
	std::function<void(const QPointer<Connect>&)> connectToHostTimeoutCallback = nullptr;
	std::function<void(const QPointer<Connect>&)> connectToHostSucceedCallback = nullptr;
	std::function<void(const QPointer<Connect>&)> remoteHostClosedCallback = nullptr;
	std::function<void(const QPointer<Connect>&)> readyToDeleteCallback = nullptr;
	std::function<void(const QPointer<Connect>&, const qint32&, const qint64&, const qint64&, const qint64&)>
		packageSendingCallback = nullptr;
	std::function<void(const QPointer<Connect>&, const qint32&, const qint64&, const qint64&, const qint64&)>
		packageReceivingCallback = nullptr;
	std::function<void(const QPointer<Connect>&, const QSharedPointer<Package>&)> packageReceivedCallback =
		nullptr;
	int globalServerThreadCount = 1;
	int globalSocketThreadCount = NETWORK_ADVISE_THREADCOUNT;
	int globalCallbackThreadCount = NETWORK_ADVISE_THREADCOUNT;
};
class Server : public QObject {
	Q_OBJECT
		Q_DISABLE_COPY(Server)
public:
	Server(
		QSharedPointer<ServerSettings> serverSettings,
		QSharedPointer<ConnectPoolSettings> connectPoolSettings,
		QSharedPointer<ConnectSettings> connectSettings);
	~Server() override;

	static QSharedPointer<Server> createServer(
		const quint16& listenPort,
		const QHostAddress& listenAddress = QHostAddress::Any,
		const bool& fileTransferEnabled = false);

	inline QSharedPointer<ServerSettings> serverSettings() {
		return m_serverSettings;
	}

	inline QSharedPointer<ConnectPoolSettings> connectPoolSettings() {
		return m_connectPoolSettings;
	}

	inline QSharedPointer<ConnectSettings> connectSettings() {
		return m_connectSettings;
	}

	inline QString nodeMarkSummary() const {
		return m_nodeMarkSummary;
	}

	bool begin();

	void registerProcessor(const QPointer<Processor>& processor);

	inline QSet<QString> availableProcessorMethodNames() const {
		auto l = m_processorCallbacks.keys();
		return QSet<QString>(l.begin(), l.end());
	}

private:
	void incomingConnection(const qintptr& socketDescriptor);

	inline void onConnectToHostError(const QPointer<Connect>& connect,
		const QPointer<ConnectPool>& connectPool) {
		if (!m_serverSettings->connectToHostErrorCallback) {
			return;
		}
		m_callbackThreadPool->run(
			[connect,
			callback = m_serverSettings->connectToHostErrorCallback]() {
				callback(connect);
			});
	}

	inline void onConnectToHostTimeout(const QPointer<Connect>& connect,
		const QPointer<ConnectPool>& connectPool) {
		if (!m_serverSettings->connectToHostTimeoutCallback) {
			return;
		}
		m_callbackThreadPool->run(
			[connect,
			callback = m_serverSettings->connectToHostTimeoutCallback]() {
				callback(connect);
			});
	}

	inline void onConnectToHostSucceed(const QPointer<Connect>& connect,
		const QPointer<ConnectPool>& connectPool) {
		if (!m_serverSettings->connectToHostSucceedCallback) {
			return;
		}
		m_callbackThreadPool->run(
			[connect,
			callback = m_serverSettings->connectToHostSucceedCallback]() {
				callback(connect);
			});
	}

	inline void onRemoteHostClosed(const QPointer<Connect>& connect,
		const QPointer<ConnectPool>& connectPool) {
		if (!m_serverSettings->remoteHostClosedCallback) {
			return;
		}
		m_callbackThreadPool->run(
			[connect,
			callback = m_serverSettings->remoteHostClosedCallback]() {
				callback(connect);
			});
	}

	inline void onReadyToDelete(const QPointer<Connect>& connect, const QPointer<ConnectPool>& connectPool) {
		if (!m_serverSettings->readyToDeleteCallback) {
			return;
		}
		m_callbackThreadPool->run(
			[connect,
			callback = m_serverSettings->readyToDeleteCallback]() {
				callback(connect);
			});
	}

	inline void onPackageSending(
		const QPointer<Connect>& connect,
		const QPointer<ConnectPool>& connectPool,
		const qint32& randomFlag,
		const qint64& payloadCurrentIndex,
		const qint64& payloadCurrentSize,
		const qint64& payloadTotalSize) {
		if (!m_serverSettings->packageSendingCallback) {
			return;
		}
		m_callbackThreadPool->run(
			[connect,
			callback = m_serverSettings->packageSendingCallback,
			randomFlag,
			payloadCurrentIndex,
			payloadCurrentSize,
			payloadTotalSize]() {
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
	) {
		if (!m_serverSettings->packageReceivingCallback) { return; }
		m_callbackThreadPool->run(
			[
				connect,
					callback = m_serverSettings->packageReceivingCallback,
					randomFlag,
					payloadCurrentIndex,
					payloadCurrentSize,
					payloadTotalSize
			]() {
				callback(connect, randomFlag, payloadCurrentIndex, payloadCurrentSize, payloadTotalSize);
			}
				);
	}

	void onPackageReceived(
		const QPointer<Connect>& connect,
		const QPointer<ConnectPool>& connectPool,
		const QSharedPointer<Package>& package);

private:
	// Thread pool
	static QWeakPointer<NetworkThreadPool> m_globalServerThreadPool;
	QSharedPointer<NetworkThreadPool> m_serverThreadPool;
	static QWeakPointer<NetworkThreadPool> m_globalSocketThreadPool;
	QSharedPointer<NetworkThreadPool> m_socketThreadPool;
	static QWeakPointer<NetworkThreadPool> m_globalCallbackThreadPool;
	QSharedPointer<NetworkThreadPool> m_callbackThreadPool;
	// Settings
	QSharedPointer<ServerSettings> m_serverSettings;
	QSharedPointer<ConnectPoolSettings> m_connectPoolSettings;
	QSharedPointer<ConnectSettings> m_connectSettings;
	// Server
	QSharedPointer<QTcpServer> m_tcpServer;
	QMap<QThread*, QSharedPointer<ConnectPool>> m_connectPools;
	// Processor
	QSet<Processor*> m_processors;
	QMap<QString, std::function<void(const QPointer<Connect>&, const QSharedPointer<Package>&)>>
		m_processorCallbacks;
	// Other
	QString m_nodeMarkSummary;
};

#endif // NETWORK_INCLUDE_NETWORK_SERVER_H_
