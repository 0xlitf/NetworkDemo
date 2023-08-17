
#include "lan.h"

#include <QDebug>
#include <QUdpSocket>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QThread>
#include <QNetworkInterface>
QWeakPointer<NetworkThreadPool> Lan::globalProcessorThreadPool_;
Lan::Lan(const QSharedPointer<LanSettings>& lanSettings) :
	lanSettings_(lanSettings) {
}
Lan::~Lan() {
	processorThreadPool_->waitRun(
		[
			this
		]() {
			mutex_.lock();
			this->sendOffline();
			QThread::msleep(100);
			timerForCheckLoop_.clear();
			udpSocket_.clear();
			for (const auto& lanNode : availableLanNodes_) {
				if (lanSettings_->lanNodeOfflineCallback) {
					lanSettings_->lanNodeOfflineCallback(lanNode);
				}
			}
			availableLanNodes_.clear();
			mutex_.unlock();
		}
	);
}
QSharedPointer<Lan> Lan::createLan(
	const QHostAddress& multicastGroupAddress,
	const quint16& bindPort,
	const QString& dutyMark
) {
	QSharedPointer<LanSettings> lanSettings(new LanSettings);
	lanSettings->dutyMark = dutyMark;
	lanSettings->multicastGroupAddress = multicastGroupAddress;
	lanSettings->bindPort = bindPort;
	return QSharedPointer<Lan>(new Lan(lanSettings));
}
QList<LanAddressEntries> Lan::lanAddressEntries() {
	QList<LanAddressEntries> result;
	for (const auto& interface : QNetworkInterface::allInterfaces()) {
		if (interface.flags() != (
			QNetworkInterface::IsUp |
			QNetworkInterface::IsRunning |
			QNetworkInterface::CanBroadcast |
			QNetworkInterface::CanMulticast
			)
			) {
			continue;
		}
		bool isVmAddress = interface.humanReadableName().toLower().startsWith("vm");
		for (const auto& addressEntry : interface.addressEntries()) {
			if (!addressEntry.ip().toIPv4Address()) {
				continue;
			}
			result.push_back({
				addressEntry.ip(),
				addressEntry.netmask(),
				QHostAddress(addressEntry.ip().toIPv4Address() & addressEntry.netmask().toIPv4Address()),
				isVmAddress
				});
		}
	}
	if (result.isEmpty()) {
		result.push_back({
			QHostAddress::LocalHost,
			QHostAddress::Broadcast,
			QHostAddress::Broadcast,
			false
			});
	}
	return result;
}
bool Lan::begin() {
	nodeMarkSummary_ = NetworkNodeMark::calculateNodeMarkSummary(lanSettings_->dutyMark);
	if (globalProcessorThreadPool_) {
		processorThreadPool_ = globalProcessorThreadPool_.toStrongRef();
	} else {
		processorThreadPool_ = QSharedPointer<NetworkThreadPool>(
			new NetworkThreadPool(lanSettings_->globalProcessorThreadCount));
		globalProcessorThreadPool_ = processorThreadPool_.toWeakRef();
	}
	bool bindSucceed = false;
	processorThreadPool_->waitRun(
		[
			this,
			&bindSucceed
		]() {
			timerForCheckLoop_.reset(new QTimer);
			timerForCheckLoop_->setSingleShot(true);
			timerForCheckLoop_->setInterval(lanSettings_->checkLoopInterval);
			connect(timerForCheckLoop_.data(), &QTimer::timeout, this, &Lan::checkLoop, Qt::DirectConnection);
			bindSucceed = this->refreshUdp();
			if (!bindSucceed) {
				return;
			}
			this->checkLoop();
		}
	);
	return bindSucceed;
}
QHostAddress Lan::matchLanAddressEntries(const QList<QHostAddress>& ipList) {
	for (const auto& currentAddress : ipList) {
		for (const auto& lanAddressEntries : lanAddressEntries_) {
			if (((currentAddress.toIPv4Address() & lanAddressEntries.netmask.toIPv4Address()) == lanAddressEntries.
				ipSegment.toIPv4Address()) ||
				(currentAddress == QHostAddress::LocalHost)) {
				return currentAddress;
			}
		}
	}
	return {};
}
QList<LanNode> Lan::availableLanNodes() {
	QList<LanNode> result;
	mutex_.lock();
	for (const auto& lanAddressEntries : availableLanNodes_) {
		result.push_back(lanAddressEntries);
	}
	mutex_.unlock();
	return result;
}
void Lan::sendOnline() {
	//    qDebug() << "Lan::sendOnline";
	if ((lanAddressEntries_.size() == 1) && (lanAddressEntries_.first().ip == QHostAddress::LocalHost)) {
		qDebug() << "Lan::sendOnline: current lanAddressEntries only contains local host, skip sendOnline";
		return;
	}
	const auto&& data = this->makeData(false, true);
	udpSocket_->writeDatagram(data, lanSettings_->multicastGroupAddress, lanSettings_->bindPort);
	udpSocket_->writeDatagram(data, QHostAddress(QHostAddress::Broadcast), lanSettings_->bindPort);
}
void Lan::sendOffline() {
	//    qDebug() << "Lan::sendOffline";
	const auto&& data = this->makeData(true, false);
	udpSocket_->writeDatagram(data, lanSettings_->multicastGroupAddress, lanSettings_->bindPort);
	udpSocket_->writeDatagram(data, QHostAddress(QHostAddress::Broadcast), lanSettings_->bindPort);
}
void Lan::refreshLanAddressEntries() {
	lanAddressEntries_ = this->lanAddressEntries();
}
bool Lan::refreshUdp() {
	udpSocket_.reset(new QUdpSocket);
	connect(udpSocket_.data(), &QUdpSocket::readyRead, this, &Lan::onUdpSocketReadyRead, Qt::DirectConnection);
	const auto&& bindSucceed = udpSocket_->bind(
		QHostAddress::AnyIPv4,
		lanSettings_->bindPort,
#ifdef Q_OS_WIN
		QUdpSocket::ReuseAddressHint
#else
		QUdpSocket::ShareAddress
#endif
	);
	if (!bindSucceed) {
		return false;
	}
	if (!udpSocket_->joinMulticastGroup(lanSettings_->multicastGroupAddress)) {
		return false;
	}
	return true;
}
void Lan::checkLoop() {
	++checkLoopCounting_;
	if (!(checkLoopCounting_ % 12)) {
		this->refreshLanAddressEntries();
	}
	if (checkLoopCounting_ && !(checkLoopCounting_ % 3)) {
		this->refreshUdp();
	}
	mutex_.lock();
	bool lanListModified = false;
	const auto&& currentTime = QDateTime::currentMSecsSinceEpoch();
	for (auto it = availableLanNodes_.begin(); it != availableLanNodes_.end();) {
		if ((currentTime - it->lastActiveTime) >= lanSettings_->lanNodeTimeoutInterval) {
			const auto lanNode = *it;
			availableLanNodes_.erase(it);
			lanListModified = true;
			mutex_.unlock();
			this->onLanNodeStateOffline(lanNode);
			mutex_.lock();
			it = availableLanNodes_.begin();
		} else {
			++it;
		}
	}
	mutex_.unlock();
	if (lanListModified) {
		this->onLanNodeListChanged();
	}
	this->sendOnline();
	timerForCheckLoop_->start();
}
QByteArray Lan::makeData(const bool& requestOffline, const bool& requestFeedback) {
	QVariantMap data;
	QVariantList ipList;
	for (const auto& lanAddressEntries : lanAddressEntries_) {
		ipList.push_back(lanAddressEntries.ip.toString());
	}
	data["nodeMarkSummary"] = nodeMarkSummary_;
	data["dutyMark"] = lanSettings_->dutyMark;
	data["dataPackageIndex"] = ++nextDataPackageIndex_;
	data["ipList"] = ipList;
	data["requestOffline"] = requestOffline;
	data["requestFeedback"] = requestFeedback;
	data["appendData"] = appendData_;
	return QJsonDocument(QJsonObject::fromVariantMap(data)).toJson(QJsonDocument::Compact);
}
void Lan::onUdpSocketReadyRead() {
	while (udpSocket_->hasPendingDatagrams()) {
		QByteArray datagram;
		datagram.resize(udpSocket_->pendingDatagramSize());
		udpSocket_->readDatagram(datagram.data(), datagram.size());
		//        qDebug() << "Lan::onUdpSocketReadyRead:" << datagram;
		const auto&& data = QJsonDocument::fromJson(datagram).object().toVariantMap();
		const auto&& nodeMarkSummary = data["nodeMarkSummary"].toString();
		const auto&& dutyMark = data["dutyMark"].toString();
		const auto&& dataPackageIndex = data["dataPackageIndex"].toInt();
		const auto&& requestOffline = data["requestOffline"].toBool();
		const auto&& requestFeedback = data["requestFeedback"].toBool();
		const auto&& appendData = data["appendData"];
		if (nodeMarkSummary.isEmpty()) {
			qDebug() << "Lan::onUdpSocketReadyRead: error data1:" << datagram;
			continue;
		}
		if (!dataPackageIndex) {
			qDebug() << "Lan::onUdpSocketReadyRead: error data2:" << datagram;
			continue;
		}
		QList<QHostAddress> ipList;
		for (const auto& ip : data["ipList"].toList()) {
			ipList.push_back(QHostAddress(ip.toString()));
		}
		if (ipList.isEmpty()) {
			qDebug() << "Lan::onUdpSocketReadyRead: error data3:" << datagram;
			continue;
		}
		if (!requestOffline) {
			mutex_.lock();
			LanNode lanNode;
			bool firstOnline = false;
			if (!availableLanNodes_.contains(nodeMarkSummary)) {
				lanNode.nodeMarkSummary = nodeMarkSummary;
				lanNode.dutyMark = dutyMark;
				lanNode.lastActiveTime = QDateTime::currentMSecsSinceEpoch();
				lanNode.dataPackageIndex = dataPackageIndex;
				lanNode.ipList = ipList;
				lanNode.appendData = appendData;
				lanNode.matchAddress = this->matchLanAddressEntries(ipList);
				lanNode.isSelf = nodeMarkSummary == nodeMarkSummary_;
				availableLanNodes_[nodeMarkSummary] = lanNode;
				mutex_.unlock();
				this->onLanNodeStateOnline(lanNode);
				this->onLanNodeListChanged();
				firstOnline = true;
			} else {
				lanNode = availableLanNodes_[nodeMarkSummary];
				if (lanNode.dataPackageIndex < dataPackageIndex) {
					lanNode.lastActiveTime = QDateTime::currentMSecsSinceEpoch();
					lanNode.dataPackageIndex = dataPackageIndex;
					lanNode.ipList = ipList;
					lanNode.appendData = appendData;
					lanNode.matchAddress = this->matchLanAddressEntries(ipList);
					availableLanNodes_[nodeMarkSummary] = lanNode;
					mutex_.unlock();
					this->onLanNodeStateActive(lanNode);
				} else {
					mutex_.unlock();
				}
			}
			if (requestFeedback && firstOnline && (lanNode.nodeMarkSummary != nodeMarkSummary_)) {
				const auto&& data = this->makeData(false, false);
				udpSocket_->writeDatagram(data, lanSettings_->multicastGroupAddress, lanSettings_->bindPort);
				udpSocket_->writeDatagram(data, lanNode.matchAddress, lanSettings_->bindPort);
			}
		} else {
			mutex_.lock();
			if (availableLanNodes_.contains(nodeMarkSummary)) {
				const auto lanNode = availableLanNodes_[nodeMarkSummary];
				availableLanNodes_.remove(nodeMarkSummary);
				mutex_.unlock();
				this->onLanNodeStateOffline(lanNode);
				this->onLanNodeListChanged();
			} else {
				mutex_.unlock();
			}
		}
	}
}
