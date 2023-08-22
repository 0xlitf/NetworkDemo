
#ifndef NETWORK_INCLUDE_NETWORK_LAN_H_
#define NETWORK_INCLUDE_NETWORK_LAN_H_

#include "foundation.h"

struct LanSettings {
	QString dutyMark;
	QHostAddress multicastGroupAddress;
	quint16 bindPort = 0;
	int checkLoopInterval = 10 * 1000;
	int lanNodeTimeoutInterval = 60 * 1000;
	std::function<void(const LanNode&)> lanNodeOnlineCallback;
	std::function<void(const LanNode&)> lanNodeActiveCallback;
	std::function<void(const LanNode&)> lanNodeOfflineCallback;
	std::function<void()> lanNodeListChangedCallback;
	int globalProcessorThreadCount = 1;
};

struct LanNode {
	QString nodeMarkSummary;
	QString dutyMark;
	int dataPackageIndex = 0;
	qint64 lastActiveTime = 0;
	QList<QHostAddress> ipList;
	QVariant appendData;
	QHostAddress matchAddress;
	bool isSelf = false;
};

struct LanAddressEntries {
	QHostAddress ip;
	QHostAddress netmask;
	QHostAddress ipSegment;
	bool isVmAddress;
};

class Lan : public QObject {
	Q_OBJECT

public:
	Lan(const QSharedPointer<LanSettings>& lanSettings);

	~Lan() override;

	Lan(const Lan&) = delete;

	Lan& operator=(const Lan&) = delete;

	static QSharedPointer<Lan> createLan(
		const QHostAddress& multicastGroupAddress,
		const quint16& bindPort,
		const QString& dutyMark = "");

	static QList<LanAddressEntries> lanAddressEntries();

	inline QSharedPointer<LanSettings> lanSettings() {
		return m_lanSettings;
	}

	inline QString nodeMarkSummary() const {
		return m_nodeMarkSummary;
	}

	inline void setAppendData(const QVariant& appendData) {
		m_appendData = appendData;
	}

	bool begin();

	QHostAddress matchLanAddressEntries(const QList<QHostAddress>& ipList);

	QList<LanNode> availableLanNodes();

	void sendOnline();

	void sendOffline();

private:
	void refreshLanAddressEntries();

	bool refreshUdp();

	void checkLoop();

	QByteArray makeData(const bool& requestOffline, const bool& requestFeedback);

	void onUdpSocketReadyRead();

	inline void onLanNodeStateOnline(const LanNode& lanNode) {
		if (!m_lanSettings->lanNodeOnlineCallback) {
			return;
		}
		m_lanSettings->lanNodeOnlineCallback(lanNode);
	}

	inline void onLanNodeStateActive(const LanNode& lanNode) {
		if (!m_lanSettings->lanNodeActiveCallback) {
			return;
		}
		m_lanSettings->lanNodeActiveCallback(lanNode);
	}

	inline void onLanNodeStateOffline(const LanNode& lanNode) {
		if (!m_lanSettings->lanNodeOfflineCallback) {
			return;
		}
		m_lanSettings->lanNodeOfflineCallback(lanNode);
	}

	inline void onLanNodeListChanged() {
		if (!m_lanSettings->lanNodeListChangedCallback) {
			return;
		}
		m_lanSettings->lanNodeListChangedCallback();
	}

private:
	// Thread pool
	static QWeakPointer<NetworkThreadPool> m_globalProcessorThreadPool;
	QSharedPointer<NetworkThreadPool> m_processorThreadPool;
	// Settings
	QSharedPointer<LanSettings> m_lanSettings;
	// Socket
	QSharedPointer<QUdpSocket> m_udpSocket;
	// Data
	QList<LanAddressEntries> m_lanAddressEntries;
	QMap<QString, LanNode> m_availableLanNodes;
	// Other
	QString m_nodeMarkSummary;
	QMutex m_mutex;
	QVariant m_appendData;
	QSharedPointer<QTimer> m_timerForCheckLoop;
	int m_checkLoopCounting = -1;
	int m_nextDataPackageIndex = 0;
};

#endif // NETWORK_INCLUDE_NETWORK_LAN_H_
