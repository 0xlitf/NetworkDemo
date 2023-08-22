
#ifndef NETWORK_INCLUDE_NETWORK_FOUNDATION_H_
#define NETWORK_INCLUDE_NETWORK_FOUNDATION_H_

#include <functional>
#include <vector>
#include <memory>

#include <QObject>
#include <QSharedPointer>
#include <QWeakPointer>
#include <QPointer>
#include <QMutex>
#include <QVariant>
#include <QHostAddress>

#define NETWORK_VERSIONNUMBER QVersionNumber::fromString( NETWORK_VERSIONSTRING )

#define NETWORKPACKAGE_BOOTFLAG qint8( 0x7d )
#define NETWORKPACKAGE_PAYLOADDATATRANSPORTPACKGEFLAG qint8( 0x1 )
#define NETWORKPACKAGE_PAYLOADDATAREQUESTPACKGEFLAG qint8( 0x2 )
#define NETWORKPACKAGE_FILEDATATRANSPORTPACKGEFLAG qint8( 0x3 )
#define NETWORKPACKAGE_FILEDATAREQUESTPACKGEFLAG qint8( 0x4 )
#define NETWORKPACKAGE_UNCOMPRESSEDFLAG qint8( 0x1 )
#define NETWORKPACKAGE_COMPRESSEDFLAG qint8( 0x2 )

#if ( defined Q_OS_IOS ) || ( defined Q_OS_ANDROID )
#   define NETWORK_ADVISE_THREADCOUNT 1
#   define NETWORKPACKAGE_ADVISE_CUTPACKAGESIZE qint64( 512 * 1024 )
#else
#   define NETWORK_ADVISE_THREADCOUNT 2
#   define NETWORKPACKAGE_ADVISE_CUTPACKAGESIZE qint64( 2 * 1024 * 1024 )
#endif

#define NETWORK_NULLPTR_CHECK( ptr, ... ) \
    if ( !ptr ) { qDebug( "%s: %s is null", __func__, # ptr ); return __VA_ARGS__; }

#define NETWORK_THISNULL_CHECK( message, ... )                  \
    {                                                           \
        auto this_ = this;                                      \
        if ( !this_ )                                           \
        {                                                       \
            qDebug( "%s: this is null", message );              \
            return __VA_ARGS__;                                 \
        }                                                       \
    }

class QSemaphore;
class QMutex;
class QTimer;
class QThreadPool;
class QEventLoop;
class QJsonObject;
class QJsonArray;
class QJsonValue;
class QJsonDocument;
class QFile;
class QDir;
class QFileInfo;
class QTcpSocket;
class QTcpServer;
class QUdpSocket;

class Package;
class Connect;
class ConnectPool;
class Server;
class Processor;
class Client;
class Lan;

struct ConnectSettings;
struct ConnectPoolSettings;
struct ServerSettings;
struct ClientSettings;
struct LanSettings;
struct LanNode;

using PackagePointer = QPointer<Package>;
using ConnectPointer = QPointer<Connect>;
using ConnectPoolPointer = QPointer<ConnectPool>;
using ServerPointer = QPointer<Server>;
using ProcessorPointer = QPointer<Processor>;
using ClientPointer = QPointer<Client>;
using LanPointer = QPointer<Lan>;

using NetworkVoidSharedPointer = std::shared_ptr<void>;

using PackageSharedPointer = QSharedPointer<Package>;
using ConnectSharedPointer = QSharedPointer<Connect>;
using ConnectPoolSharedPointer = QSharedPointer<ConnectPool>;
using ServerSharedPointer = QSharedPointer<Server>;
using ProcessorSharedPointer = QSharedPointer<Processor>;
using ClientSharedPointer = QSharedPointer<Client>;
using LanSharedPointer = QSharedPointer<Lan>;

using ConnectSettingsSharedPointer = QSharedPointer<ConnectSettings>;
using ConnectPoolSettingsSharedPointer = QSharedPointer<ConnectPoolSettings>;
using ServerSettingsSharedPointer = QSharedPointer<ServerSettings>;
using ClientSettingsSharedPointer = QSharedPointer<ClientSettings>;
using LanSettingsSharedPointer = QSharedPointer<LanSettings>;

using ConnectPointerFunction = std::function<void(const ConnectPointer& connect)>;
using ConnectPointerAndPackageSharedPointerFunction = std::function<void(const ConnectPointer& connect, const PackageSharedPointer& package)>;

struct NetworkOnReceivedCallbackPackage {
	std::function<void(const ConnectPointer& connect, const PackageSharedPointer&)> succeedCallback = nullptr;
	std::function<void(const ConnectPointer& connect)> failCallback = nullptr;
};

class NetworkThreadPoolHelper : public QObject {
	Q_OBJECT
		Q_DISABLE_COPY(NetworkThreadPoolHelper)

public:
	NetworkThreadPoolHelper();

	~NetworkThreadPoolHelper() override = default;

	void run(const std::function<void()>& callback);

public Q_SLOTS:
	void onRun();

private:
	QMutex m_mutex;
	QSharedPointer<std::vector<std::function<void()>>> m_waitForRunCallbacks;
	bool m_alreadyCall = false;
	qint64 m_lastRunTime = 0;
	int m_lastRunCallbackCount = 0;
};

class NetworkThreadPool : public QObject {
	Q_OBJECT
		Q_DISABLE_COPY(NetworkThreadPool)

public:
	NetworkThreadPool(const int& threadCount);

	~NetworkThreadPool() override;

	inline int nextRotaryIndex() {
		m_rotaryIndex = (m_rotaryIndex + 1) % m_helpers->size();
		return m_rotaryIndex;
	}

	int run(const std::function<void()>& callback, const int& threadIndex = -1);

	inline void runEach(const std::function<void()>& callback) {
		for (auto index = 0; index < m_helpers->size(); ++index) {
			(*m_helpers)[index]->run(callback);
		}
	}

	int waitRun(const std::function<void()>& callback, const int& threadIndex = -1);

	inline void waitRunEach(const std::function<void()>& callback) {
		for (auto index = 0; index < m_helpers->size(); ++index) {
			this->waitRun(callback, index);
		}
	}

private:
	QSharedPointer<QThreadPool> m_threadPool;
	QSharedPointer<QVector<QPointer<QEventLoop>>> m_eventLoops;
	QSharedPointer<QVector<QPointer<NetworkThreadPoolHelper>>> m_helpers;
	int m_rotaryIndex = -1;
};

class NetworkNodeMark {
public:
	NetworkNodeMark(const QString& dutyMark);

	~NetworkNodeMark() = default;

	static QString calculateNodeMarkSummary(const QString& dutyMark);

	inline qint64 applicationStartTime() const {
		return m_applicationStartTime;
	}

	inline QString applicationFilePath() const {
		return m_applicationFilePath;
	}

	inline QString localHostName() const {
		return m_localHostName;
	}

	inline qint64 nodeMarkCreatedTime() const {
		return m_nodeMarkCreatedTime;
	}

	inline QString nodeMarkClassAddress() const {
		return m_nodeMarkClassAddress;
	}

	inline QString dutyMark() const {
		return m_dutyMark;
	}

	inline QString nodeMarkSummary() const {
		return m_nodeMarkSummary;
	}

private:
	static qint64 m_applicationStartTime;
	static QString m_applicationFilePath;
	static QString m_localHostName;
	qint64 m_nodeMarkCreatedTime;
	QString m_nodeMarkClassAddress;
	QString m_dutyMark;
	QString m_nodeMarkSummary;
};

namespace Network {
	void printVersionInformation(const char* NetworkCompileModeString = NETWORK_COMPILE_MODE_STRING);
}

#endif//NETWORK_INCLUDE_NETWORK_FOUNDATION_H_
