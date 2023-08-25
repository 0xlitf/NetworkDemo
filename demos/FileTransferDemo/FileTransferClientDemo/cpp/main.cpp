
#include <QCoreApplication>
#include <QStandardPaths>
#include <QFile>
#include <QFileInfo>

#include "network.h"

int main(int argc, char* argv[]) {
	QCoreApplication a(argc, argv);

	Network::printVersionInformation();

	auto client = Client::createClient(true);
	if (!client->begin()) {
		qDebug() << "Client: begin fail";
		return -1;
	}
	qDebug() << "Client: begin succeed";
	// 以阻塞方式创建连接
	qDebug() << "Client: waitForCreateConnect:" << client->waitForCreateConnect("127.0.0.1", 26432);
	const auto&& sourceFilePath = QString("%1/%2").arg(QStandardPaths::writableLocation(QStandardPaths::TempLocation), "filetransferdemo");

	{
		QFile file(sourceFilePath);
		file.open(QIODevice::WriteOnly);
		file.write(QByteArray("FileTransferDemo"));
		file.waitForBytesWritten(30 * 1000);
	}

	// 发送文件
	qDebug() << "Client: sendFileData reply:" << client->sendFileData(
		"127.0.0.1",
		26432,
		"fileTransfer", // 需要调用的服务端方法名称
		QFileInfo(sourceFilePath),
		[](const auto&, const auto&) {
			qDebug() << "Client: send file succeed";
		},
		[](const auto&) {
			qDebug() << "Client: send file fail";
		}
	);
	return a.exec();
}
