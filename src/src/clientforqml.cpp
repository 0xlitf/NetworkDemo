
#include "clientforqml.h"

#include <QDebug>
#include <QJsonObject>
#include <QJsonDocument>
#include <QMetaObject>

#include "client.h"
#include "connect.h"
#include "package.h"
using namespace std;
using namespace std::placeholders;
ClientForQml::ClientForQml()
{
	static bool flag = true;
	if (flag)
	{
		flag = false;
		qRegisterMetaType<std::function<void()>>("std::function<void()>");
		qRegisterMetaType<QVariantMap>("QVariantMap");
	}
}
bool ClientForQml::beginClient()
{
	NetworkClient_ = Client::createClient();
	NetworkClient_->clientSettings()->connectToHostErrorCallback = std::bind(
		&ClientForQml::connectToHostError, this, _2, _3);
	NetworkClient_->clientSettings()->connectToHostTimeoutCallback = std::bind(
		&ClientForQml::connectToHostTimeout, this, _2, _3);
	NetworkClient_->clientSettings()->connectToHostSucceedCallback = std::bind(
		&ClientForQml::connectToHostSucceed, this, _2, _3);
	NetworkClient_->clientSettings()->remoteHostClosedCallback = std::bind(
		&ClientForQml::remoteHostClosed, this, _2, _3);
	NetworkClient_->clientSettings()->readyToDeleteCallback = std::bind(
		&ClientForQml::readyToDelete, this, _2, _3);
	return NetworkClient_->begin();
}
void ClientForQml::createConnect(const QString& hostName, const quint16& port)
{
	NetworkClient_->createConnect(hostName, port);
}
void ClientForQml::sendVariantMapData(
	const QString& hostName,
	const quint16& port,
	const QString& targetActionFlag,
	const QVariantMap& payloadData,
	QJSValue succeedCallback,
	QJSValue failCallback
)
{
	if (!succeedCallback.isCallable())
	{
		qDebug() << "ClientForQml::sendPayloadData: error, succeedCallback not callable";
		return;
	}
	if (!failCallback.isCallable())
	{
		qDebug() << "ClientForQml::sendPayloadData: error, failCallback not callable";
		return;
	}
	if (!NetworkClient_)
	{
		qDebug() << "ClientForQml::sendPayloadData: error, client need beginClient";
		return;
	}
	NetworkClient_->sendVariantMapData(
		hostName,
		port,
		targetActionFlag,
		payloadData,
		{}, // empty appendData
		[ this, succeedCallback ](const auto&, const auto& package)
		{
			const auto&& received = QJsonDocument::fromJson(package->payloadData()).object().toVariantMap();
			QMetaObject::invokeMethod(
				this,
				"onSendSucceed",
				Qt::QueuedConnection,
				Q_ARG(QVariant, QVariant::fromValue( succeedCallback )),
				Q_ARG(QVariant, received)
			);
		},
		[ this, failCallback ](const auto&)
		{
			QMetaObject::invokeMethod(
				this,
				"received",
				Qt::QueuedConnection,
				Q_ARG(QVariant, QVariant::fromValue( failCallback ))
			);
		}
	);
}
