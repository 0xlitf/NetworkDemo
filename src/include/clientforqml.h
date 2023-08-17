
#ifndef NETWORK_INCLUDE_NETWORK_CLIENGFORQML_H_
#define NETWORK_INCLUDE_NETWORK_CLIENGFORQML_H_

#include <QJSValue>
#include <QJSValueList>
#include <QDateTime>

#include "foundation.h"
#ifndef QT_CORE_LIB
#   error("Please add qml in pro file")
#endif
#define NETWORKCLIENTFORQML_REGISTERTYPE( engine ) \
    qmlRegisterType< ClientForQml >( "ClientForQml", 1, 0, "ClientForQml" ); \
    engine.addImportPath( ":/Network/" );
class ClientForQml : public QObject {
	Q_OBJECT
public:
	ClientForQml();
	~ClientForQml() override = default;
public slots:
	bool beginClient();
	QVariantMap test() {
		return { {"key", QDateTime::currentDateTime()}, {"key2", QByteArray::fromHex("00112233")} };
	}
	void print(const QVariant& d) {
		qDebug() << d;
	}
	void createConnect(const QString& hostName, const quint16& port);
	void sendVariantMapData(
		const QString& hostName,
		const quint16& port,
		const QString& targetActionFlag,
		const QVariantMap& payloadData,
		QJSValue succeedCallback,
		QJSValue failCallback
	);
private Q_SLOTS:
	inline void runOnClientThread(const std::function<void()> &callback)
	{
		callback();
	}
signals:
	void connectToHostError(const QString& hostName, const quint16& port);
	void connectToHostTimeout(const QString& hostName, const quint16& port);
	void connectToHostSucceed(const QString& hostName, const quint16& port);
	void remoteHostClosed(const QString& hostName, const quint16& port);
	void readyToDelete(const QString& hostName, const quint16& port);
private:
	QSharedPointer<Client> NetworkClient_;
};

#endif//NETWORK_INCLUDE_NETWORK_CLIENGFORQML_H_
