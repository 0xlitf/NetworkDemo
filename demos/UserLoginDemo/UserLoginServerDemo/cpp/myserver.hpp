﻿#ifndef CPP_MYSERVER_HPP_
#define CPP_MYSERVER_HPP_

#include <QTimer>

#include "network.h"
class MyServer : public Processor {
	Q_OBJECT
		Q_DISABLE_COPY(MyServer)
public:
	MyServer() = default;
	~MyServer() override = default;
	bool begin() {
		// 创建一个服务端
		const quint16&& listenPort = 23456; // 监听端口
		server_ = Server::createServer(listenPort);
		// 注册当前类
		server_->registerProcessor(this);
		// 初始化服务端
		if (!server_->begin()) {
			qDebug() << "MyServer: begin fail";
			return false;
		}
		qDebug() << "MyServer: begin succeed";
		return true;
	}
	// 用于处理的函数必须是槽函数
public slots:
	void userLogin(const QVariantMap& received, QVariantMap& send) {
		// 回调会发生在一个专用的线程，请注意线程安全
		// 打印客户端发送的数据
		qDebug() << "MyServer::userLogin: received data:" << received;
		if (!received.contains("username") || received["username"].toString().isEmpty()) {
			send["succeed"] = false;
			send["message"] = "username is empty";
			return;
		}
		if (!received.contains("password") || received["password"].toString().isEmpty()) {
			send["succeed"] = false;
			send["message"] = "password is empty";
			return;
		}
		// 返回数据
		send["succeed"] = true;
		send["message"] = "";
	}
private:
	QSharedPointer<Server> server_;
};
#endif//CPP_MYSERVER_HPP_
