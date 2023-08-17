
#include <QGuiApplication>
#include <QQmlApplicationEngine>

#include "network.h"
int main(int argc, char* argv[]) {
	QGuiApplication app(argc, argv);
	Network::printVersionInformation();
	QQmlApplicationEngine engine;
	NETWORKCLIENTFORQML_REGISTERTYPE(engine);
	engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
	return app.exec();
}
