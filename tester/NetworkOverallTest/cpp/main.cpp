
#include <QCoreApplication>
#include <QtTest>

#include "foundation.h"

#include "overalltest.h"
int main(int argc, char* argv[]) {
	QCoreApplication app(argc, argv);
	Network::printVersionInformation();
	//    for ( auto count = 100; count; --count )
	//    {
	//        if ( QTest::qExec( &NetworkTest, argc, argv ) )
	//        {
	//            return -1;
	//        }
	//    }
	//    return 0;
	NetworkOverallTest NetworkTest;
	return QTest::qExec(&NetworkTest, argc, argv);
}
