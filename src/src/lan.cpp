
#include "lan.h"

#include <QDebug>
#include <QUdpSocket>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QThread>
#include <QNetworkInterface>

QWeakPointer<NetworkThreadPool> Lan::m_globalProcessorThreadPool;

Lan::Lan(const QSharedPointer<LanSettings>& lanSettings) :
	m_lanSettings(lanSettings) {
}

Lan::~Lan() {
	m_processorThreadPool->waitRun(
		[
			this
		]() {
			m_mutex.lock();
			this->sendOffline();
			QThread::msleep(100);
			m_timerForCheckLoop.clear();
			m_udpSocket.clear();
			for (const auto& lanNode : m_availableLanNodes) {
				if (m_lanSettings->lanNodeOfflineCallback) {
					m_lanSettings->lanNodeOfflineCallback(lanNode);
				}
			}
			m_availableLanNodes.clear();
			m_mutex.unlock();
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
	m_nodeMarkSummary = NetworkNodeMark::calculateNodeMarkSummary(m_lanSettings->dutyMark);
	if (m_globalProcessorThreadPool) {
		m_processorThreadPool = m_globalProcessorThreadPool.toStrongRef();
	} else {
		m_processorThreadPool = QSharedPointer<NetworkThreadPool>(
			new NetworkThreadPool(m_lanSettings->globalProcessorThreadCount));
		m_globalProcessorThreadPool = m_processorThreadPool.toWeakRef();
	}
	bool bindSucceed = false;
	m_processorThreadPool->waitRun(
		[
			this,
				&bindSucceed
		]() {
			m_timerForCheckLoop.reset(new QTimer);
			m_timerForCheckLoop->setSingleShot(true);
			m_timerForCheckLoop->setInterval(m_lanSettings->checkLoopInterval);
			connect(m_timerForCheckLoop.data(), &QTimer::timeout, this, &Lan::checkLoop, Qt::DirectConnection);
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
		for (const auto& lanAddressEntries : m_lanAddressEntries) {
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
	m_mutex.lock();
	for (const auto& lanAddressEntries : m_availableLanNodes) {
		result.push_back(lanAddressEntries);
	}
	m_mutex.unlock();
	return result;
}

void Lan::sendOnline() {
	//    qDebug() << "Lan::sendOnline";
	if ((m_lanAddressEntries.size() == 1) && (m_lanAddressEntries.first().ip == QHostAddress::LocalHost)) {
		qDebug() << "Lan::sendOnline: current lanAddressEntries only contains local host, skip sendOnline";
		return;
	}
	const auto&& data = this->makeData(false, true);
	m_udpSocket->writeDatagram(data, m_lanSettings->multicastGroupAddress, m_lanSettings->bindPort);
	m_udpSocket->writeDatagram(data, QHostAddress(QHostAddress::Broadcast), m_lanSettings->bindPort);
}

void Lan::sendOffline() {
	//    qDebug() << "Lan::sendOffline";
	const auto&& data = this->makeData(true, false);
	m_udpSocket->writeDatagram(data, m_lanSettings->multicastGroupAddress, m_lanSettings->bindPort);
	m_udpSocket->writeDatagram(data, QHostAddress(QHostAddress::Broadcast), m_lanSettings->bindPort);
}

void Lan::refreshLanAddressEntries() {
	m_lanAddressEntries = this->lanAddressEntries();
}

bool Lan::refreshUdp() {
	m_udpSocket.reset(new QUdpSocket);
	connect(m_udpSocket.data(), &QUdpSocket::readyRead, this, &Lan::onUdpSocketReadyRead, Qt::DirectConnection);
	const auto&& bindSucceed = m_udpSocket->bind(
		QHostAddress::AnyIPv4,
		m_lanSettings->bindPort,
#ifdef Q_OS_WIN
		QUdpSocket::ReuseAddressHint
#else
		QUdpSocket::ShareAddress
#endif
	);
	if (!bindSucceed) {
		return false;
	}
	if (!m_udpSocket->joinMulticastGroup(m_lanSettings->multicastGroupAddress)) {
		return false;
	}
	return true;
}

void Lan::checkLoop() {
	++m_checkLoopCounting;
	if (!(m_checkLoopCounting % 12)) {
		this->refreshLanAddressEntries();
	}
	if (m_checkLoopCounting && !(m_checkLoopCounting % 3)) {
		this->refreshUdp();
	}
	m_mutex.lock();
	bool lanListModified = false;
	const auto&& currentTime = QDateTime::currentMSecsSinceEpoch();
	for (auto it = m_availableLanNodes.begin(); it != m_availableLanNodes.end();) {
		if ((currentTime - it->lastActiveTime) >= m_lanSettings->lanNodeTimeoutInterval) {
			const auto lanNode = *it;
			m_availableLanNodes.erase(it);
			lanListModified = true;
			m_mutex.unlock();
			this->onLanNodeStateOffline(lanNode);
			m_mutex.lock();
			it = m_availableLanNodes.begin();
		} else {
			++it;
		}
	}
	m_mutex.unlock();
	if (lanListModified) {
		this->onLanNodeListChanged();
	}
	this->sendOnline();
	m_timerForCheckLoop->start();
}

QByteArray Lan::makeData(const bool& requestOffline, const bool& requestFeedback) {
	QVariantMap data;
	QVariantList ipList;
	for (const auto& lanAddressEntries : m_lanAddressEntries) {
		ipList.push_back(lanAddressEntries.ip.toString());
	}
	data["nodeMarkSummary"] = m_nodeMarkSummary;
	data["dutyMark"] = m_lanSettings->dutyMark;
	data["dataPackageIndex"] = ++m_nextDataPackageIndex;
	data["ipList"] = ipList;
	data["requestOffline"] = requestOffline;
	data["requestFeedback"] = requestFeedback;
	data["appendData"] = m_appendData;
	return QJsonDocument(QJsonObject::fromVariantMap(data)).toJson(QJsonDocument::Compact);
}

void Lan::onUdpSocketReadyRead() {
	while (m_udpSocket->hasPendingDatagrams()) {
		QByteArray datagram;
		datagram.resize(m_udpSocket->pendingDatagramSize());
		m_udpSocket->readDatagram(datagram.data(), datagram.size());
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
			m_mutex.lock();
			LanNode lanNode;
			bool firstOnline = false;
			if (!m_availableLanNodes.contains(nodeMarkSummary)) {
				lanNode.nodeMarkSummary = nodeMarkSummary;
				lanNode.dutyMark = dutyMark;
				lanNode.lastActiveTime = QDateTime::currentMSecsSinceEpoch();
				lanNode.dataPackageIndex = dataPackageIndex;
				lanNode.ipList = ipList;
				lanNode.appendData = appendData;
				lanNode.matchAddress = this->matchLanAddressEntries(ipList);
				lanNode.isSelf = nodeMarkSummary == m_nodeMarkSummary;
				m_availableLanNodes[nodeMarkSummary] = lanNode;
				m_mutex.unlock();
				this->onLanNodeStateOnline(lanNode);
				this->onLanNodeListChanged();
				firstOnline = true;
			} else {
				lanNode = m_availableLanNodes[nodeMarkSummary];
				if (lanNode.dataPackageIndex < dataPackageIndex) {
					lanNode.lastActiveTime = QDateTime::currentMSecsSinceEpoch();
					lanNode.dataPackageIndex = dataPackageIndex;
					lanNode.ipList = ipList;
					lanNode.appendData = appendData;
					lanNode.matchAddress = this->matchLanAddressEntries(ipList);
					m_availableLanNodes[nodeMarkSummary] = lanNode;
					m_mutex.unlock();
					this->onLanNodeStateActive(lanNode);
				} else {
					m_mutex.unlock();
				}
			}
			if (requestFeedback && firstOnline && (lanNode.nodeMarkSummary != m_nodeMarkSummary)) {
				const auto&& data = this->makeData(false, false);
				m_udpSocket->writeDatagram(data, m_lanSettings->multicastGroupAddress, m_lanSettings->bindPort);
				m_udpSocket->writeDatagram(data, lanNode.matchAddress, m_lanSettings->bindPort);
			}
		} else {
			m_mutex.lock();
			if (m_availableLanNodes.contains(nodeMarkSummary)) {
				const auto lanNode = m_availableLanNodes[nodeMarkSummary];
				m_availableLanNodes.remove(nodeMarkSummary);
				m_mutex.unlock();
				this->onLanNodeStateOffline(lanNode);
				this->onLanNodeListChanged();
			} else {
				m_mutex.unlock();
			}
		}
	}
}
