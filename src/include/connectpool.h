
#ifndef NETWORK_INCLUDE_NETWORK_CONNECTPOOL_H_
#define NETWORK_INCLUDE_NETWORK_CONNECTPOOL_H_

#include "foundation.h"

struct ConnectPoolSettings {
	std::function<void(const QPointer<Connect>&, const QPointer<ConnectPool>&)> connectToHostErrorCallback = nullptr;
	std::function<void(const QPointer<Connect>&, const QPointer<ConnectPool>&)> connectToHostTimeoutCallback = nullptr;
	std::function<void(const QPointer<Connect>&, const QPointer<ConnectPool>&)> connectToHostSucceedCallback = nullptr;
	std::function<void(const QPointer<Connect>&, const QPointer<ConnectPool>&)> remoteHostClosedCallback = nullptr;
	std::function<void(const QPointer<Connect>&, const QPointer<ConnectPool>&)> readyToDeleteCallback = nullptr;
	std::function<void(const QPointer<Connect>&, const QPointer<ConnectPool>&, const qint32&, const qint64&, const qint64&, const qint64&)> packageSendingCallback = nullptr;
	std::function<void(const QPointer<Connect>&, const QPointer<ConnectPool>&, const qint32&, const qint64&, const qint64&, const qint64&)> packageReceivingCallback = nullptr;
	std::function<void(const QPointer<Connect>&, const QPointer<ConnectPool>&, const QSharedPointer<Package>&)> packageReceivedCallback = nullptr;
	std::function<void(const QPointer<Connect>&, const QPointer<ConnectPool>&, const QSharedPointer<Package>&, const ConnectPointerAndPackageSharedPointerFunction&)> waitReplyPackageSucceedCallback = nullptr;
	std::function<void(const QPointer<Connect>&, const QPointer<ConnectPool>&, const ConnectPointerFunction&)> waitReplyPackageFailCallback = nullptr;
};

class ConnectPool : public QObject {
	Q_OBJECT;
	Q_DISABLE_COPY(ConnectPool)

public:
	ConnectPool(
		QSharedPointer<ConnectPoolSettings> connectPoolSettings,
		QSharedPointer<ConnectSettings> connectSettings
	);
	~ConnectPool() override;

	void createConnect(
		std::function<void(std::function<void()>)> runOnConnectThreadCallback,
		const QString& hostName,
		const quint16& port
	);

	void createConnect(
		std::function<void(std::function<void()>)> runOnConnectThreadCallback,
		const qintptr& socketDescriptor
	);

	inline bool containsConnect(const QString& hostName, const quint16& port) {
		mutex_.lock();
		auto contains = m_bimapForHostAndPort1.contains(QString("%1:%2").arg(hostName).arg(port));
		mutex_.unlock();
		return contains;
	}

	inline bool containsConnect(const qintptr& socketDescriptor) {
		mutex_.lock();
		auto contains = m_bimapForSocketDescriptor1.contains(socketDescriptor);
		mutex_.unlock();
		return contains;
	}

	QPair<QString, quint16> getHostAndPortByConnect(const QPointer<Connect>& connect);

	qintptr getSocketDescriptorByConnect(const QPointer<Connect>& connect);

	QPointer<Connect> getConnectByHostAndPort(const QString& hostName, const quint16& port);

	QPointer<Connect> getConnectBySocketDescriptor(const qintptr& socketDescriptor);

private:
	inline void onConnectToHostError(const QPointer<Connect>& connect) {
		NETWORK_NULLPTR_CHECK(m_connectPoolSettings->connectToHostErrorCallback);
		m_connectPoolSettings->connectToHostErrorCallback(connect, this);
	}

	inline void onConnectToHostTimeout(const QPointer<Connect>& connect) {
		NETWORK_NULLPTR_CHECK(m_connectPoolSettings->connectToHostTimeoutCallback);
		m_connectPoolSettings->connectToHostTimeoutCallback(connect, this);
	}

	void onConnectToHostSucceed(const QPointer<Connect>& connect);

	inline void onRemoteHostClosed(const QPointer<Connect>& connect) {
		NETWORK_NULLPTR_CHECK(m_connectPoolSettings->remoteHostClosedCallback);
		m_connectPoolSettings->remoteHostClosedCallback(connect, this);
	}

	void onReadyToDelete(const QPointer<Connect>& connect);

	inline void onPackageSending(
		const QPointer<Connect>& connect,
		const qint32& randomFlag,
		const qint64& payloadCurrentIndex,
		const qint64& payloadCurrentSize,
		const qint64& payloadTotalSize) {
		NETWORK_NULLPTR_CHECK(m_connectPoolSettings->packageSendingCallback);
		m_connectPoolSettings->packageSendingCallback(connect, this, randomFlag, payloadCurrentIndex, payloadCurrentSize,
			payloadTotalSize);
	}

	inline void onPackageReceiving(
		const QPointer<Connect>& connect,
		const qint32& randomFlag,
		const qint64& payloadCurrentIndex,
		const qint64& payloadCurrentSize,
		const qint64& payloadTotalSize) {
		NETWORK_NULLPTR_CHECK(m_connectPoolSettings->packageReceivingCallback);
		m_connectPoolSettings->packageReceivingCallback(connect, this, randomFlag, payloadCurrentIndex, payloadCurrentSize,
			payloadTotalSize);
	}

	inline void onPackageReceived(
		const QPointer<Connect>& connect,
		const QSharedPointer<Package>& package) {
		NETWORK_NULLPTR_CHECK(m_connectPoolSettings->packageReceivedCallback);
		m_connectPoolSettings->packageReceivedCallback(connect, this, package);
	}

	inline void onWaitReplyPackageSucceed(
		const QPointer<Connect>& connect,
		const QSharedPointer<Package>& package,
		const ConnectPointerAndPackageSharedPointerFunction& succeedCallback) {
		NETWORK_NULLPTR_CHECK(m_connectPoolSettings->waitReplyPackageSucceedCallback);
		m_connectPoolSettings->waitReplyPackageSucceedCallback(connect, this, package, succeedCallback);
	}

	inline void onWaitReplyPackageFail(
		const QPointer<Connect>& connect,
		const ConnectPointerFunction& failCallback) {
		NETWORK_NULLPTR_CHECK(m_connectPoolSettings->waitReplyPackageFailCallback);
		m_connectPoolSettings->waitReplyPackageFailCallback(connect, this, failCallback);
	}

private:
	// Settings
	QSharedPointer<ConnectPoolSettings> m_connectPoolSettings;
	QSharedPointer<ConnectSettings> m_connectSettings;
	// Connect
	QMap<Connect*, QSharedPointer<Connect>> m_connectForConnecting;
	QMap<Connect*, QSharedPointer<Connect>> m_connectForConnected;
	QMap<QString, QPointer<Connect>> m_bimapForHostAndPort1; // "127.0.0.1:34543" -> Connect
	QMap<Connect*, QString> m_bimapForHostAndPort2; // Connect -> "127.0.0.1:34543"
	QMap<qintptr, QPointer<Connect>> m_bimapForSocketDescriptor1; // socketDescriptor -> Connect
	QMap<Connect*, qintptr> m_bimapForSocketDescriptor2; // Connect -> socketDescriptor
	// Other
	QMutex mutex_;
};

#endif//NETWORK_INCLUDE_NETWORK_CONNECTPOOL_H_
