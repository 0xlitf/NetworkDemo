
#include <QCoreApplication>

#include "pong.hpp"
int main(int argc, char* argv[])
{
	QCoreApplication a(argc, argv);
	Network::printVersionInformation();
	Pong pong;
	if (!pong.begin()) { return -1; }
	return a.exec();
}
