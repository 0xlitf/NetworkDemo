#ifndef __CPP_MYSERVER_HPP__
#define __CPP_MYSERVER_HPP__

#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>

#include "network.h"

class MyServer : public Processor {
public:
	Q_OBJECT;
	Q_DISABLE_COPY(MyServer)

public:
	MyServer() = default;

	~MyServer() override = default;

	bool begin() {
		// 创建一个服务端
		m_server = Server::createServer(26432, QHostAddress::Any, true);
		// 注册当前类
		m_server->registerProcessor(this);
		// 设置文件保存到桌面
		m_server->connectSettings()->setFilePathProviderToDir(
			QStandardPaths::writableLocation(QStandardPaths::DesktopLocation));
		// 初始化服务端
		if (!m_server->begin()) {
			qDebug() << "MyServer: begin fail";
			return false;
		}
		qDebug() << "MyServer: begin succeed";
		return true;
	}

	// 用于处理的函数必须是槽函数
public slots:
	void fileTransfer(const QFileInfo& received, QVariantMap& send) {
		// 回调会发生在一个专用的线程，请注意线程安全
		// 打印接收到的文件
		qDebug() << "fileTransfer received:" << received.filePath();
		// 返回数据
		send["succeed"] = true;
		send["message"] = "";
	}

private:
	QSharedPointer<Server> m_server;
};
#endif//__CPP_MYSERVER_HPP__
