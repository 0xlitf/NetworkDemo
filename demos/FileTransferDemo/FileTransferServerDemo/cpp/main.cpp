
#include <QCoreApplication>

#include "myserver.hpp"

int main(int argc, char* argv[]) {
	QCoreApplication a(argc, argv);

	Network::printVersionInformation();

	MyServer myServer;
	if (!myServer.begin()) {
		return -1;
	}

	return a.exec();
}
