
#ifndef NETWORK_INCLUDE_NETWORK_LAN_H_
#define NETWORK_INCLUDE_NETWORK_LAN_H_

#include "foundation.h"
struct LanSettings
{
	QString dutyMark;
	QHostAddress multicastGroupAddress;
	quint16 bindPort = 0;
	int checkLoopInterval = 10 * 1000;
	int lanNodeTimeoutInterval = 60 * 1000;
	std::function<void(const LanNode &)> lanNodeOnlineCallback;
	std::function<void(const LanNode &)> lanNodeActiveCallback;
	std::function<void(const LanNode &)> lanNodeOfflineCallback;
	std::function<void()> lanNodeListChangedCallback;
	int globalProcessorThreadCount = 1;
};
struct LanNode
{
	QString nodeMarkSummary;
	QString dutyMark;
	int dataPackageIndex = 0;
	qint64 lastActiveTime = 0;
	QList<QHostAddress> ipList;
	QVariant appendData;
	QHostAddress matchAddress;
	bool isSelf = false;
};
struct LanAddressEntries
{
	QHostAddress ip;
	QHostAddress netmask;
	QHostAddress ipSegment;
	bool isVmAddress;
};
class Lan : public QObject
{
	Q_OBJECT
public:
	Lan(const QSharedPointer<LanSettings> &lanSettings);
	~Lan() override;
	Lan(const Lan &) = delete;
	Lan &operator=(const Lan &) = delete;
	static QSharedPointer<Lan> createLan(
		const QHostAddress &multicastGroupAddress,
		const quint16 &bindPort,
		const QString &dutyMark = "");
	static QList<LanAddressEntries> lanAddressEntries();
	inline QSharedPointer<LanSettings> lanSettings()
	{
		return lanSettings_;
	}
	inline QString nodeMarkSummary() const
	{
		return nodeMarkSummary_;
	}
	inline void setAppendData(const QVariant &appendData)
	{
		appendData_ = appendData;
	}
	bool begin();
	QHostAddress matchLanAddressEntries(const QList<QHostAddress> &ipList);
	QList<LanNode> availableLanNodes();
	void sendOnline();
	void sendOffline();

private:
	void refreshLanAddressEntries();
	bool refreshUdp();
	void checkLoop();
	QByteArray makeData(const bool &requestOffline, const bool &requestFeedback);
	void onUdpSocketReadyRead();

	inline void onLanNodeStateOnline(const LanNode &lanNode)
	{
		if (!lanSettings_->lanNodeOnlineCallback)
		{
			return;
		}
		lanSettings_->lanNodeOnlineCallback(lanNode);
	}
	inline void onLanNodeStateActive(const LanNode &lanNode)
	{
		if (!lanSettings_->lanNodeActiveCallback)
		{
			return;
		}
		lanSettings_->lanNodeActiveCallback(lanNode);
	}
	inline void onLanNodeStateOffline(const LanNode &lanNode)
	{
		if (!lanSettings_->lanNodeOfflineCallback)
		{
			return;
		}
		lanSettings_->lanNodeOfflineCallback(lanNode);
	}
	inline void onLanNodeListChanged()
	{
		if (!lanSettings_->lanNodeListChangedCallback)
		{
			return;
		}
		lanSettings_->lanNodeListChangedCallback();
	}

private:
	// Thread pool
	static QWeakPointer<NetworkThreadPool> globalProcessorThreadPool_;
	QSharedPointer<NetworkThreadPool> processorThreadPool_;
	// Settings
	QSharedPointer<LanSettings> lanSettings_;
	// Socket
	QSharedPointer<QUdpSocket> udpSocket_;
	// Data
	QList<LanAddressEntries> lanAddressEntries_;
	QMap<QString, LanNode> availableLanNodes_;
	// Other
	QString nodeMarkSummary_;
	QMutex mutex_;
	QVariant appendData_;
	QSharedPointer<QTimer> timerForCheckLoop_;
	int checkLoopCounting_ = -1;
	int nextDataPackageIndex_ = 0;
};

#endif // NETWORK_INCLUDE_NETWORK_LAN_H_
