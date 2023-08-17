
#include <QCoreApplication>

#include "ping.hpp"
int main(int argc, char* argv[]) {
	QCoreApplication a(argc, argv);
	Network::printVersionInformation();
	Ping ping;
	if (!ping.begin()) {
		return -1;
	}
	return a.exec();
}
