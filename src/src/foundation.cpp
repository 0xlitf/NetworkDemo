
#include "foundation.h"

#include <QDebug>
#include <QThreadPool>
#include <QSemaphore>
#include <QVector>
#include <QCryptographicHash>
#include <QHostInfo>
#include <QtConcurrent>
#include <QLocale>
#include <QTime>

// NetworkThreadPoolHelper
NetworkThreadPoolHelper::NetworkThreadPoolHelper() :
	m_waitForRunCallbacks(new std::vector<std::function<void()>>) {
}

void NetworkThreadPoolHelper::run(const std::function<void()>& callback) {
	m_mutex.lock();
	m_waitForRunCallbacks->push_back(callback);
	if (!m_alreadyCall) {
		m_alreadyCall = true;
		QMetaObject::invokeMethod(
			this,
			"onRun",
			Qt::QueuedConnection
		);
	}
	m_mutex.unlock();
}

void NetworkThreadPoolHelper::onRun() {
	auto currentTime = QDateTime::currentMSecsSinceEpoch();
	if (((currentTime - m_lastRunTime) < 5) && (m_lastRunCallbackCount > 10)) {
		QThread::msleep(5);
	}
	std::vector<std::function<void()>> callbacks;
	m_mutex.lock();
	callbacks = *m_waitForRunCallbacks;
	m_waitForRunCallbacks->clear();
	m_alreadyCall = false;
	m_lastRunTime = currentTime;
	m_lastRunCallbackCount = static_cast<int>(callbacks.size());
	m_mutex.unlock();
	for (const auto& callback : callbacks) {
		callback();
	}
}

// NetworkThreadPool
NetworkThreadPool::NetworkThreadPool(const int& threadCount) :
	m_threadPool(new QThreadPool),
	m_eventLoops(new QVector<QPointer<QEventLoop>>),
	m_helpers(new QVector<QPointer<NetworkThreadPoolHelper>>) {
	m_threadPool->setMaxThreadCount(threadCount);
	m_eventLoops->resize(threadCount);
	m_helpers->resize(threadCount);
	QSemaphore semaphoreForThreadStart;
	for (auto index = 0; index < threadCount; ++index) {
		QtConcurrent::run(
			m_threadPool.data(),
			[
				this,
				index,
				&semaphoreForThreadStart
			]() {
				QEventLoop eventLoop;
				NetworkThreadPoolHelper helper;
				(*this->m_eventLoops)[index] = &eventLoop;
				(*this->m_helpers)[index] = &helper;
				semaphoreForThreadStart.release(1);
				eventLoop.exec();
			}
		);
	}
	semaphoreForThreadStart.acquire(threadCount);
}

NetworkThreadPool::~NetworkThreadPool() {
	for (const auto& eventLoop : *m_eventLoops) {
		QMetaObject::invokeMethod(eventLoop.data(), "quit");
	}
	m_threadPool->waitForDone();
}

int NetworkThreadPool::run(const std::function<void()>& callback, const int& threadIndex) {
	if (threadIndex == -1) {
		m_rotaryIndex = (m_rotaryIndex + 1) % m_helpers->size();
	}
	const auto index = (threadIndex == -1) ? (m_rotaryIndex) : (threadIndex);
	(*m_helpers)[index]->run(callback);
	return index;
}

int NetworkThreadPool::waitRun(const std::function<void()>& callback, const int& threadIndex) {
	QSemaphore semaphore;
	auto index = this->run(
		[&semaphore, &callback]() {
			callback();
			semaphore.release(1);
		},
		threadIndex
	);
	semaphore.acquire(1);
	return index;
}

// NetworkNodeMark
qint64 NetworkNodeMark::m_applicationStartTime = QDateTime::currentMSecsSinceEpoch();
QString NetworkNodeMark::m_applicationFilePath;
QString NetworkNodeMark::m_localHostName;

NetworkNodeMark::NetworkNodeMark(const QString& dutyMark) :
	m_nodeMarkCreatedTime(QDateTime::currentMSecsSinceEpoch()),
	m_nodeMarkClassAddress(QString::number(reinterpret_cast<qint64>(this), 16)),
	m_dutyMark(dutyMark) {
	if (m_applicationFilePath.isEmpty()) {
		m_applicationFilePath = qApp->applicationFilePath();
		m_localHostName = QHostInfo::localHostName();
	}
	m_nodeMarkSummary = QCryptographicHash::hash(
		QString("%1:%2:%3:%4:%5:%6").arg(
			QString::number(m_applicationStartTime),
			m_applicationFilePath,
			m_localHostName,
			QString::number(m_nodeMarkCreatedTime),
			m_nodeMarkClassAddress,
			m_dutyMark
		).toUtf8(), QCryptographicHash::Md5).toHex();
}

QString NetworkNodeMark::calculateNodeMarkSummary(const QString& dutyMark) {
	NetworkNodeMark nodeMark(dutyMark);
	return nodeMark.nodeMarkSummary();
}

// Network
void Network::printVersionInformation(const char* NetworkCompileModeString) {
	qDebug() << "Network library version:" << NETWORK_VERSIONNUMBER.toString().toLatin1().data()
#ifdef __STDC__
		<< ", build in:" << QDateTime(
			QLocale(QLocale::English).toDate(QString(__DATE__).replace("  ", " 0"), "MMM dd yyyy"),
			QTime::fromString(__TIME__, "hh:mm:ss")
		).toString("yyyy-MM-dd hh:mm:ss").toLatin1().data()
#endif
		<< ", compile mode:" << NetworkCompileModeString;
}
