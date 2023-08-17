#ifndef CPP_FUSIONTEST1_HPP_
#define CPP_FUSIONTEST1_HPP_

#include <QThread>

#include "processor.h"
namespace FusionTest1
{
	class UserProcessor : public Processor
	{
		Q_OBJECT
		Q_DISABLE_COPY(UserProcessor)
	public:
		UserProcessor() = default;
		~UserProcessor() override = default;
	public slots:
		bool accountLogin(const QVariantMap& received, QVariantMap& send)
		{
			//        qDebug() << "accountLogin:" << received;
			NP_CHECKRECEIVEDDATACONTAINSANDNOTEMPTY("handle", "password");
			const auto&& handle = received["handle"].toString();
			const auto&& password = received["password"].toString();
			if ((handle != "test") ||
				(password != "123456"))
			{
				NP_FAIL("handle or password error");
			}
			if (received.contains("userName"))
			{
				send["userName"] = "Jason";
			}
			if (received.contains("userPhoneNumber"))
			{
				send["userPhoneNumber"] = "18600000000";
			}
			NP_SUCCEED();
		}
	};
	class DataProcessor : public Processor
	{
		Q_OBJECT
		Q_DISABLE_COPY(DataProcessor)
	public:
		DataProcessor() = default;
		~DataProcessor() override = default;
	public slots:
		bool someRecords(const QVariantMap& received, QVariantMap& send)
		{
			//        qDebug() << "someRecords:" << received;
			NP_CHECKRECEIVEDDATACONTAINSEXPECTEDCONTENT("action", { "upload", "download" });
			if (received["action"] == "upload")
			{
				NP_CHECKRECEIVEDDATACONTAINS("someRecords");
				someRecords_ = received["someRecords"].toList();
			}
			else if (received["action"] == "download")
			{
				send["someRecords"] = someRecords_;
			}
			NP_SUCCEED();
		}
		bool msleep(const QVariantMap& received, QVariantMap& send)
		{
			//        qDebug() << "msleep:" << received;
			NP_CHECKRECEIVEDDATACONTAINSANDNOT0("msleep");
			QThread::msleep(static_cast<unsigned long>(received["msleep"].toInt()));
			NP_SUCCEED();
		}
	private:
		QList<QVariant> someRecords_;
	};
}
#endif//CPP_FUSIONTEST1_HPP_
