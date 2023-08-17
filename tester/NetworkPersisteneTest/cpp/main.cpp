
#include <QApplication>
#include <QCoreApplication>
#include <QtTest>

#include "foundation.h"

#include "persistenetest.h"
int main(int argc, char* argv[]) {
	QApplication a(argc, argv);
	Network::printVersionInformation();
	NetworkPersisteneTest persisteneTest;
	persisteneTest.test();
	return a.exec();
}
