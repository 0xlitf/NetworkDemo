
#ifndef NETWORK_INCLUDE_NETWORK_PROCESSOR_H_
#define NETWORK_INCLUDE_NETWORK_PROCESSOR_H_

#include "foundation.h"
#define NP_PRINTFUNCTION()                                                            \
    {                                                                                   \
        const auto &&buffer = QString( Q_FUNC_INFO );                                   \
        const auto &&indexForEnd = buffer.indexOf( '(' );                               \
        const auto functionName = buffer.mid( 0, indexForEnd ).remove( QStringLiteral( "bool " ) ); \
        qDebug() << functionName.toLocal8Bit().data();                                  \
    }
#define NP_PRINTRECEIVED()                                                            \
    {                                                                                   \
        const auto &&buffer = QString( Q_FUNC_INFO );                                   \
        const auto &&indexForEnd = buffer.indexOf( '(' );                               \
        const auto functionName = buffer.mid( 0, indexForEnd ).remove( QStringLiteral( "bool " ) ); \
        qDebug() << ( functionName + ": received:" ).toLocal8Bit().data() << received;  \
    }
#define NP_SUCCEED()                                                                  \
    send[ QStringLiteral( "succeed" ) ] = true;                                         \
    send[ QStringLiteral( "message" ) ] = "";                                           \
    return true;
#define NP_FAIL( errorMessage )                                                       \
    send[ QStringLiteral( "succeed" ) ] = false;                                        \
    send[ QStringLiteral( "message" ) ] = errorMessage;                                 \
    return false;
#define NP_SERVERFAIL( errorMessage )                                                 \
    const auto &&message = QStringLiteral( ": Server error: " ) + errorMessage;         \
    qDebug() << QString( Q_FUNC_INFO ).remove( "bool " ).toLocal8Bit().data()           \
             << message.toLocal8Bit().data();                                           \
    send[ QStringLiteral( "succeed" ) ] = false;                                        \
    send[ QStringLiteral( "message" ) ] = errorMessage;                                 \
    return false;
#define NP_CHECKRECEIVEDDATACONTAINS( ... )                                           \
    if (                                                                                \
        !Processor::checkMapContains(                                          \
            { __VA_ARGS__ },                                                            \
            received,                                                                   \
            send                                                                        \
        )                                                                               \
    )                                                                                   \
    { return false; }
#define NP_CHECKRECEIVEDDATACONTAINSANDNOT0( ... )                                    \
    if (                                                                                \
        !Processor::checkMapContainsAndNot0(                                   \
            { __VA_ARGS__ },                                                            \
            received,                                                                   \
            send                                                                        \
        )                                                                               \
    )                                                                                   \
    { return false; }
#define NP_CHECKRECEIVEDDATACONTAINSANDNOTEMPTY( ... )                                \
    if (                                                                                \
        !Processor::checkMapContainsAndNotEmpty(                               \
            { __VA_ARGS__ },                                                            \
            received,                                                                   \
            send                                                                        \
        )                                                                               \
    )                                                                                   \
    { return false; }
#define NP_CHECKRECEIVEDDATACONTAINSEXPECTEDCONTENT( key, ... )                       \
    if (                                                                                \
        !Processor::checkDataContasinsExpectedContent(                         \
            key,                                                                        \
            __VA_ARGS__,                                                                \
            received,                                                                   \
            send                                                                        \
        )                                                                               \
    )                                                                                   \
    { return false; }
#define NP_CHECKRECEIVEDAPPENDDATACONTAINS( ... )                                     \
    if (                                                                                \
        !Processor::checkMapContains(                                          \
            { __VA_ARGS__ },                                                            \
            receivedAppend,                                                             \
            send                                                                        \
        )                                                                               \
    )                                                                                   \
    { return false; }
#define NP_CHECKRECEIVEDAPPENDDATACONTAINSANDNOT0( ... )                              \
    if (                                                                                \
        !Processor::checkMapContainsAndNot0(                                   \
            { __VA_ARGS__ },                                                            \
            receivedAppend,                                                             \
            send                                                                        \
        )                                                                               \
    )                                                                                   \
    { return false; }
#define NP_CHECKRECEIVEDAPPENDDATACONTAINSANDNOTEMPTY( ... )                          \
    if (                                                                                \
        !Processor::checkMapContainsAndNotEmpty(                               \
            { __VA_ARGS__ },                                                            \
            receivedAppend,                                                             \
            send                                                                        \
        )                                                                               \
    )                                                                                   \
    { return false; }
#define NP_CHECKRECEIVEDAPPENDDATACONTAINSEXPECTEDCONTENT( key, ... )                 \
    if (                                                                                \
        !Processor::checkDataContasinsExpectedContent(                         \
            key,                                                                        \
            __VA_ARGS__,                                                                \
            receivedAppend,                                                             \
            send                                                                        \
        )                                                                               \
    )                                                                                   \
    { return false; }
class Processor : public QObject {
	Q_OBJECT
		Q_DISABLE_COPY(Processor)
public:
	Processor(const bool& invokeMethodByProcessorThread = false);
	~Processor() override = default;
	QSet<QString> availableSlots();
	bool handlePackage(const QPointer<Connect>& connect, const QSharedPointer<Package>& package);
	void setReceivedPossibleThreads(const QSet<QThread*>& threads);
	static bool checkMapContains(const QStringList& keys, const QVariantMap& received, QVariantMap& send);
	static bool checkMapContainsAndNot0(const QStringList& keys, const QVariantMap& received, QVariantMap& send);
	static bool checkMapContainsAndNotEmpty(const QStringList& keys, const QVariantMap& received, QVariantMap& send);
	static bool checkDataContasinsExpectedContent(const QString& key, const QVariantList& expectedContentList,
		const QVariantMap& received, QVariantMap& send);
protected:
	QPointer<Connect> currentThreadConnect();
private:
    inline static void deleteByteArray(QByteArray *ptr)
    {
        delete ptr;
    }
    inline static void deleteVariantMap(QVariantMap *ptr)
    {
        delete ptr;
    }

    static void deleteFileInfo(QFileInfo* ptr);
private:
	static QSet<QString> exceptionSlots_;
	bool invokeMethodByProcessorThread_;
	QSet<QString> availableSlots_;
	QMap<QThread*, QPointer<Connect>> connectMapByThread_;
	QMap<QString, std::function<void(const QPointer<Connect>& connect,
		const QSharedPointer<Package>& package)>> onpackageReceivedCallbacks_;
};

#endif//NETWORK_INCLUDE_NETWORK_PROCESSOR_H_
