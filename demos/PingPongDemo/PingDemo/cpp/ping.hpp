﻿#ifndef CPP_PING_HPP_
#define CPP_PING_HPP_

#include <QTimer>
#include <QJsonObject>
#include <QJsonDocument>

#include "network.h"
// Ping 为客户端
class Ping {
public:
	Ping() = default;
	~Ping() = default;
	bool begin() {
		// 创建一个客户端
		client_ = Client::createClient();
		client_->clientSettings()->connectToHostErrorCallback = std::bind(
			&Ping::connectToHostErrorCallback, this, std::placeholders::_1, std::placeholders::_2,
			std::placeholders::_3);
		client_->clientSettings()->connectToHostTimeoutCallback = std::bind(
			&Ping::connectToHostTimeoutCallback, this, std::placeholders::_1, std::placeholders::_2,
			std::placeholders::_3);
		client_->clientSettings()->connectToHostSucceedCallback = std::bind(
			&Ping::onConnectSucceed, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
		if (!client_->begin()) {
			qDebug() << "Ping: begin fail";
			return false;
		}
		// 预创建一个连接，指向某个服务端，并保持长连接，不创建也可以
		client_->createConnect("127.0.0.1", 34543);
		// 设置定时器，定时发送数据
		timer_.setInterval(1000);
		timer_.setSingleShot(true);
		QObject::connect(&timer_, &QTimer::timeout, std::bind(&Ping::ping, this));
		qDebug() << "Ping: begin succeed";
		timer_.start();
		return true;
	}
	// 连接到服务端超时后的回调
	void connectToHostErrorCallback(const QPointer<Connect>& /*connect*/, const QString& hostName,
		const quint16& port) {
		// 回调会发生在一个专用的线程，请注意线程安全
		// 打印谁被连接成功了
		qDebug() << "Client: connect to server error:" << hostName << port;
	}
	// 连接到服务端错误后的回调
	void connectToHostTimeoutCallback(const QPointer<Connect>& /*connect*/, const QString& hostName,
		const quint16& port) {
		// 回调会发生在一个专用的线程，请注意线程安全
		// 打印谁被连接成功了
		qDebug() << "Client: connect to server timeout:" << hostName << port;
	}
	// 连接到服务端后的回调
	void onConnectSucceed(const QPointer<Connect>& /*connect*/, const QString& hostName, const quint16& port) {
		// 回调会发生在一个专用的线程，请注意线程安全
		// 打印谁被连接成功了
		qDebug() << "Client: connect to server succeed:" << hostName << port;
	}
	// Ping 测试
	void ping() {
		++pingCount_;
		// 发送数据前会自动检查连接，如果有则使用已经有的链接，没有则创建一个新的（以阻塞方式创建）
		const auto&& randomFlag = client_->sendPayloadData( // 发送数据，返回0表示失败，其余数表示发送成功
			"127.0.0.1", // 服务端IP
			34543, // 服务端端口
			"pong",
			QJsonDocument(QJsonObject::fromVariantMap(QVariantMap({ {"ping", "ping"} }))).toJson(QJsonDocument::Compact),
			std::bind(&Ping::onPingSucceed, this, std::placeholders::_1, std::placeholders::_2),
			std::bind(&Ping::onPingFail, this, std::placeholders::_1)
		);
		if (!randomFlag) {
			// 发送失败
			qDebug() << "Ping: send fail";
		} else {
			// 发送成功
			qDebug() << "Ping: send succeed, randomFlag:" << randomFlag;
			++sendSucceedCount_;
		}
	}
	// 发送成，并且成功接收回复的数据时的回调
	void onPingSucceed(const QPointer<Connect>& /*connect*/, const QSharedPointer<Package>& package) {
		// 回调会发生在一个专用的线程，请注意线程安全
		// 打印服务端回复的数据
		qDebug() << "Ping: ping reply:" << package->payloadData() << ", randomFlag:" << package->randomFlag();
		++replySucceedCount_;
		qDebug() << "TotalCount: "
			<< "pingCount:" << this->pingCount_
			<< ", sendSucceedCount:" << this->sendSucceedCount_
			<< ", replySucceedCount:" << replySucceedCount_ << "\n";
		if (!(replySucceedCount_ % 100000)) {
			// 到达特定数量后，退出发送循环
			return;
		}
		// 循环发送
		// 因为这里在一个新的线程，所以通过 invokeMethod 去调用 start
		QMetaObject::invokeMethod(&timer_, "start");
	}
	// 发送成功，但是未能成功接收回复的数据时的回调
	void onPingFail(const QPointer<Connect>& /*connect*/) {
		// 回调会发生在一个专用的线程，请注意线程安全
		qDebug() << "Ping: ping fail";
		// 循环发送
		// 因为这里在一个新的线程，所以通过 invokeMethod 去调用 start
		QMetaObject::invokeMethod(&timer_, "start");
	}
private:
	QSharedPointer<Client> client_;
	// 计数器
	int pingCount_ = 0;
	int sendSucceedCount_ = 0;
	int replySucceedCount_ = 0;
	// 定时器，循环发送用
	QTimer timer_;
};
#endif//CPP_PING_HPP_
