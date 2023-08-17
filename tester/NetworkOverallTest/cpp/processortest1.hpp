#ifndef CPP_PROCESSORTEST1_HPP_
#define CPP_PROCESSORTEST1_HPP_

#include <QThread>

#include "processor.h"
namespace ProcessorTest1
{
	class TestProcessor : public Processor
	{
		Q_OBJECT
		Q_DISABLE_COPY(TestProcessor)
	public:
		TestProcessor() = default;
		~TestProcessor() override = default;
	public slots:
		void actionFlag(const QVariantMap& received)
		{
			testData_ = received;
			testData2_ = QThread::currentThread();
		}
	public:
		QVariantMap testData_;
		QThread* testData2_;
	};
}
#endif//CPP_PROCESSORTEST1_HPP_
