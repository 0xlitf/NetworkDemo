
#ifndef NETWORK_INCLUDE_NETWORK_CLIENG_H_
#define NETWORK_INCLUDE_NETWORK_CLIENG_H_

#include "foundation.h"

struct ClientSettings {
	QString dutyMark;
	int maximumAutoConnectToHostWaitTime = 10 * 1000;
	bool autoCreateConnect = true;
	std::function<void(const QPointer<Connect>&, const QString& hostName, const quint16& port)> connectToHostErrorCallback = nullptr;
	std::function<void(const QPointer<Connect>&, const QString& hostName, const quint16& port)> connectToHostTimeoutCallback = nullptr;
	std::function<void(const QPointer<Connect>&, const QString& hostName, const quint16& port)> connectToHostSucceedCallback = nullptr;
	std::function<void(const QPointer<Connect>&, const QString& hostName, const quint16& port)> remoteHostClosedCallback = nullptr;
	std::function<void(const QPointer<Connect>&, const QString& hostName, const quint16& port)> readyToDeleteCallback = nullptr;
	std::function<void(const QPointer<Connect>&, const QString& hostName, const quint16& port, const qint32&, const qint64&, const qint64&, const qint64&)> packageSendingCallback = nullptr;
	std::function<void(const QPointer<Connect>&, const QString& hostName, const quint16& port, const qint32&, const qint64&, const qint64&, const qint64&)> packageReceivingCallback = nullptr;
	std::function<void(const QPointer<Connect>&, const QString& hostName, const quint16& port, const QSharedPointer<Package>&)> packageReceivedCallback = nullptr;
	int globalSocketThreadCount = 1;
	int globalCallbackThreadCount = NETWORK_ADVISE_THREADCOUNT;
};

class Client : public QObject {
	Q_OBJECT;
	Q_DISABLE_COPY(Client)

public:
	Client(
		const QSharedPointer<ClientSettings>& clientSettings,
		QSharedPointer<ConnectPoolSettings> connectPoolSettings,
		QSharedPointer<ConnectSettings> connectSettings);

	~Client() override;

	static QSharedPointer<Client> createClient(const bool& fileTransferEnabled = false);

	inline QSharedPointer<ClientSettings> clientSettings() {
		return m_clientSettings;
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

	void createConnect(const QString& hostName, const quint16& port);

	bool waitForCreateConnect(
		const QString& hostName,
		const quint16& port,
		const int& maximumConnectToHostWaitTime = -1);

	qint32 sendPayloadData(
		const QString& hostName,
		const quint16& port,
		const QString& targetActionFlag,
		const QByteArray& payloadData,
		const QVariantMap& appendData,
		const ConnectPointerAndPackageSharedPointerFunction& succeedCallback = nullptr,
		const ConnectPointerFunction& failCallback = nullptr);


	inline qint32 sendPayloadData(
		const QString& hostName,
		const quint16& port,
		const QString& targetActionFlag,
		const QByteArray& payloadData,
		const ConnectPointerAndPackageSharedPointerFunction& succeedCallback = nullptr,
		const ConnectPointerFunction& failCallback = nullptr
	) {
		return this->sendPayloadData(
			hostName,
			port,
			targetActionFlag,
			payloadData,
			{ }, // empty appendData
			succeedCallback,
			failCallback
		);
	}

	inline qint32 sendPayloadData(
		const QString& hostName,
		const quint16& port,
		const QByteArray& payloadData,
		const ConnectPointerAndPackageSharedPointerFunction& succeedCallback = nullptr,
		const ConnectPointerFunction& failCallback = nullptr
	) {
		return this->sendPayloadData(
			hostName,
			port,
			{ }, // empty targetActionFlag
			payloadData,
			{ }, // empty appendData
			succeedCallback,
			failCallback
		);
	}

	qint32 sendVariantMapData(
		const QString& hostName,
		const quint16& port,
		const QString& targetActionFlag,
		const QVariantMap& variantMap,
		const QVariantMap& appendData,
		const ConnectPointerAndPackageSharedPointerFunction& succeedCallback = nullptr,
		const ConnectPointerFunction& failCallback = nullptr);

	inline qint32 sendVariantMapData(
		const QString& hostName,
		const quint16& port,
		const QString& targetActionFlag,
		const QVariantMap& variantMap,
		const ConnectPointerAndPackageSharedPointerFunction& succeedCallback = nullptr,
		const ConnectPointerFunction& failCallback = nullptr) {
		return this->sendVariantMapData(
			hostName,
			port,
			targetActionFlag,
			variantMap,
			{}, // empty appendData
			succeedCallback,
			failCallback);
	}
	inline qint32 sendVariantMapData(
		const QString& hostName,
		const quint16& port,
		const QVariantMap& variantMap,
		const ConnectPointerAndPackageSharedPointerFunction& succeedCallback = nullptr,
		const ConnectPointerFunction& failCallback = nullptr) {
		return this->sendVariantMapData(
			hostName,
			port,
			{}, // empty targetActionFlag
			variantMap,
			{}, // empty appendData
			succeedCallback,
			failCallback);
	}

	qint32 sendFileData(
		const QString& hostName,
		const quint16& port,
		const QString& targetActionFlag,
		const QFileInfo& fileInfo,
		const QVariantMap& appendData,
		const ConnectPointerAndPackageSharedPointerFunction& succeedCallback = nullptr,
		const ConnectPointerFunction& failCallback = nullptr);

	inline qint32 sendFileData(
		const QString& hostName,
		const quint16& port,
		const QString& targetActionFlag,
		const QFileInfo& fileInfo,
		const ConnectPointerAndPackageSharedPointerFunction& succeedCallback = nullptr,
		const ConnectPointerFunction& failCallback = nullptr) {
		return this->sendFileData(
			hostName,
			port,
			targetActionFlag,
			fileInfo,
			{}, // empty appendData
			succeedCallback,
			failCallback);
	}

	inline qint32 sendFileData(
		const QString& hostName,
		const quint16& port,
		const QFileInfo& fileInfo,
		const ConnectPointerAndPackageSharedPointerFunction& succeedCallback = nullptr,
		const ConnectPointerFunction& failCallback = nullptr) {
		return this->sendFileData(
			hostName,
			port,
			{}, // empty targetActionFlag
			fileInfo,
			{}, // empty appendData
			succeedCallback,
			failCallback);
	}

	qint32 waitForSendPayloadData(
		const QString& hostName,
		const quint16& port,
		const QString& targetActionFlag,
		const QByteArray& payloadData,
		const QVariantMap& appendData,
		const ConnectPointerAndPackageSharedPointerFunction& succeedCallback = nullptr,
		const ConnectPointerFunction& failCallback = nullptr);

	inline qint32 waitForSendPayloadData(
		const QString& hostName,
		const quint16& port,
		const QString& targetActionFlag,
		const QByteArray& payloadData,
		const ConnectPointerAndPackageSharedPointerFunction& succeedCallback = nullptr,
		const ConnectPointerFunction& failCallback = nullptr) {
		return this->waitForSendPayloadData(
			hostName,
			port,
			targetActionFlag,
			payloadData,
			{}, // empty appendData
			succeedCallback,
			failCallback);
	}

	inline qint32 waitForSendPayloadData(
		const QString& hostName,
		const quint16& port,
		const QByteArray& payloadData,
		const ConnectPointerAndPackageSharedPointerFunction& succeedCallback = nullptr,
		const ConnectPointerFunction& failCallback = nullptr) {
		return this->waitForSendPayloadData(
			hostName,
			port,
			{}, // empty targetActionFlag
			payloadData,
			{}, // empty appendData
			succeedCallback,
			failCallback);
	}

	qint32 waitForSendVariantMapData(
		const QString& hostName,
		const quint16& port,
		const QString& targetActionFlag,
		const QVariantMap& variantMap,
		const QVariantMap& appendData,
		const ConnectPointerAndPackageSharedPointerFunction& succeedCallback = nullptr,
		const ConnectPointerFunction& failCallback = nullptr);

	inline qint32 waitForSendVariantMapData(
		const QString& hostName,
		const quint16& port,
		const QString& targetActionFlag,
		const QVariantMap& variantMap,
		const ConnectPointerAndPackageSharedPointerFunction& succeedCallback = nullptr,
		const ConnectPointerFunction& failCallback = nullptr) {
		return this->waitForSendVariantMapData(
			hostName,
			port,
			targetActionFlag,
			variantMap,
			{}, // empty appendData
			succeedCallback,
			failCallback);
	}

	inline qint32 waitForSendVariantMapData(
		const QString& hostName,
		const quint16& port,
		const QVariantMap& variantMap,
		const ConnectPointerAndPackageSharedPointerFunction& succeedCallback = nullptr,
		const ConnectPointerFunction& failCallback = nullptr) {
		return this->waitForSendVariantMapData(
			hostName,
			port,
			{}, // empty targetActionFlag
			variantMap,
			{}, // empty appendData
			succeedCallback,
			failCallback);
	}

	qint32 waitForSendFileData(
		const QString& hostName,
		const quint16& port,
		const QString& targetActionFlag,
		const QFileInfo& fileInfo,
		const QVariantMap& appendData,
		const ConnectPointerAndPackageSharedPointerFunction& succeedCallback = nullptr,
		const ConnectPointerFunction& failCallback = nullptr);

	inline qint32 waitForSendFileData(
		const QString& hostName,
		const quint16& port,
		const QString& targetActionFlag,
		const QFileInfo& fileInfo,
		const ConnectPointerAndPackageSharedPointerFunction& succeedCallback = nullptr,
		const ConnectPointerFunction& failCallback = nullptr) {
		return this->waitForSendFileData(
			hostName,
			port,
			targetActionFlag,
			fileInfo,
			{}, // empty appendData
			succeedCallback,
			failCallback);
	}

	inline qint32 waitForSendFileData(
		const QString& hostName,
		const quint16& port,
		const QFileInfo& fileInfo,
		const ConnectPointerAndPackageSharedPointerFunction& succeedCallback = nullptr,
		const ConnectPointerFunction& failCallback = nullptr) {
		return this->waitForSendFileData(
			hostName,
			port,
			{}, // empty targetActionFlag
			fileInfo,
			{}, // empty appendData
			succeedCallback,
			failCallback);
	}

	QPointer<Connect> getConnect(const QString& hostName, const quint16& port);

	bool containsConnect(const QString& hostName, const quint16& port);

private:
	void onConnectToHostError(const QPointer<Connect>& connect, const QPointer<ConnectPool>& connectPool);

	void onConnectToHostTimeout(const QPointer<Connect>& connect, const QPointer<ConnectPool>& connectPool);

	void onConnectToHostSucceed(const QPointer<Connect>& connect, const QPointer<ConnectPool>& connectPool);

	void onRemoteHostClosed(const QPointer<Connect>& connect, const QPointer<ConnectPool>& connectPool);

	void onReadyToDelete(const QPointer<Connect>& connect, const QPointer<ConnectPool>& connectPool);

	void onPackageSending(
		const QPointer<Connect>& connect,
		const QPointer<ConnectPool>& connectPool,
		const qint32& randomFlag,
		const qint64& payloadCurrentIndex,
		const qint64& payloadCurrentSize,
		const qint64& payloadTotalSize);

	void onPackageReceiving(
		const QPointer<Connect>& connect,
		const QPointer<ConnectPool>& connectPool,
		const qint32& randomFlag,
		const qint64& payloadCurrentIndex,
		const qint64& payloadCurrentSize,
		const qint64& payloadTotalSize);

	void onPackageReceived(
		const QPointer<Connect>& connect,
		const QPointer<ConnectPool>& connectPool,
		const QSharedPointer<Package>& package);

	void onWaitReplySucceedPackage(
		const QPointer<Connect>& connect,
		const QPointer<ConnectPool>& connectPool,
		const QSharedPointer<Package>& package,
		const ConnectPointerAndPackageSharedPointerFunction& succeedCallback);

	void onWaitReplyPackageFail(
		const QPointer<Connect>& connect,
		const QPointer<ConnectPool>& connectPool,
		const ConnectPointerFunction& failCallback);

	void releaseWaitConnectSucceedSemaphore(const QString& hostName, const quint16& port, const bool& succeed);

private:
	// Thread pool
	static QWeakPointer<NetworkThreadPool> m_globalSocketThreadPool;
	QSharedPointer<NetworkThreadPool> m_socketThreadPool;
	static QWeakPointer<NetworkThreadPool> m_globalCallbackThreadPool;
	QSharedPointer<NetworkThreadPool> m_callbackThreadPool;
	// Settings
	QSharedPointer<ClientSettings> m_clientSettings;
	QSharedPointer<ConnectPoolSettings> m_connectPoolSettings;
	QSharedPointer<ConnectSettings> m_connectSettings;
	// Client
	QMap<QThread*, QSharedPointer<ConnectPool>> m_connectPools;
	// Processor
	QSet<Processor*> m_processors;
	QMap<QString, std::function<void(const QPointer<Connect>&, const QSharedPointer<Package>&)>>
		m_processorCallbacks;
	// Other
	QString m_nodeMarkSummary;
	QMutex m_mutex;
	QMap<QString, QWeakPointer<QSemaphore>> m_waitConnectSucceedSemaphore; // "127.0.0.1:34543" -> SemaphoreForConnect
};

#endif // NETWORK_INCLUDE_NETWORK_CLIENG_H_
