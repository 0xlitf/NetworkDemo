/*
	This file is part of Network

	Library introduce: https://github.com/188080501/Network

	Copyright: Jason

	Contact email: Jason@JasonServer.com

	GitHub: https://github.com/188080501/
*/

#ifndef NETWORK_INCLUDE_NETWORK_FOUNDATION_H_
#define NETWORK_INCLUDE_NETWORK_FOUNDATION_H_

// C++ lib import
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

#define NETWORK_THISNULL_CHECK( message, ... )                \
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
using ConnectPointerAndPackageSharedPointerFunction = std::function<void(
	const ConnectPointer& connect, const PackageSharedPointer& package)>;

struct NetworkOnReceivedCallbackPackage {
	std::function<void(const ConnectPointer& connect, const PackageSharedPointer&)> succeedCallback =
		nullptr;
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
	QMutex mutex_;
	QSharedPointer<std::vector<std::function<void()>>> waitForRunCallbacks_;
	bool alreadyCall_ = false;
	qint64 lastRunTime_ = 0;
	int lastRunCallbackCount_ = 0;
};

class NetworkThreadPool : public QObject {
	Q_OBJECT
		Q_DISABLE_COPY(NetworkThreadPool)

public:
	NetworkThreadPool(const int& threadCount);

	~NetworkThreadPool() override;

    inline int nextRotaryIndex(){
        rotaryIndex_ = (rotaryIndex_ + 1) % helpers_->size();
        return rotaryIndex_;
    }

	int run(const std::function<void()>& callback, const int& threadIndex = -1);

    inline void runEach(const std::function<void()>& callback){
        for (auto index = 0; index < helpers_->size(); ++index)
        {
            (*helpers_)[index]->run(callback);
        }
    }

	int waitRun(const std::function<void()>& callback, const int& threadIndex = -1);

    inline void waitRunEach(const std::function<void()>& callback){
        for (auto index = 0; index < helpers_->size(); ++index)
        {
            this->waitRun(callback, index);
        }
    }

private:
	QSharedPointer<QThreadPool> threadPool_;
	QSharedPointer<QVector<QPointer<QEventLoop>>> eventLoops_;
	QSharedPointer<QVector<QPointer<NetworkThreadPoolHelper>>> helpers_;
	int rotaryIndex_ = -1;
};

class NetworkNodeMark {
public:
	NetworkNodeMark(const QString& dutyMark);

	~NetworkNodeMark() = default;

	static QString calculateNodeMarkSummary(const QString& dutyMark);

    inline qint64 applicationStartTime() const
    {
        return applicationStartTime_;
    }

    inline QString applicationFilePath() const
    {
        return applicationFilePath_;
    }

    inline QString localHostName() const
    {
        return localHostName_;
    }

    inline qint64 nodeMarkCreatedTime() const
    {
        return nodeMarkCreatedTime_;
    }

    inline QString nodeMarkClassAddress() const
    {
        return nodeMarkClassAddress_;
    }

    inline QString dutyMark() const
    {
        return dutyMark_;
    }

    inline QString nodeMarkSummary() const
    {
        return nodeMarkSummary_;
    }

private:
	static qint64 applicationStartTime_;
	static QString applicationFilePath_;
	static QString localHostName_;
	qint64 nodeMarkCreatedTime_;
	QString nodeMarkClassAddress_;
	QString dutyMark_;
	QString nodeMarkSummary_;
};

namespace Network {
	void printVersionInformation(const char* NetworkCompileModeString = NETWORK_COMPILE_MODE_STRING);
}

#endif//NETWORK_INCLUDE_NETWORK_FOUNDATION_H_
