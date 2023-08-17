#ifndef CPP_FUSIONTEST2_HPP_
#define CPP_FUSIONTEST2_HPP_

#include <QThread>
#include <QMutex>

#include "processor.h"
#include "connect.h"
namespace FusionTest2
{
	class ServerProcessor : public Processor
	{
		Q_OBJECT
	public:
		ServerProcessor() = default;
		~ServerProcessor() override = default;
	public slots:
		bool registerClient(const QVariantMap& received, QVariantMap& send)
		{
			NP_CHECKRECEIVEDDATACONTAINSANDNOT0("clientId");
			mutex_.lock();
			clients_[received["clientId"].toInt()] = this->currentThreadConnect();
			mutex_.unlock();
			NP_SUCCEED();
		}
		bool broadcastToAll(const QVariantMap& received, QVariantMap& send)
		{
			mutex_.lock();
			for (const auto& connect : clients_)
			{
				connect->putVariantMapData("receivedMessage", received);
			}
			mutex_.unlock();
			NP_SUCCEED();
		}
		bool broadcastToOne(const QVariantMap& received, QVariantMap& send)
		{
			NP_CHECKRECEIVEDDATACONTAINSANDNOT0("clientId");
			mutex_.lock();
			clients_[received["clientId"].toInt()]->putVariantMapData("receivedMessage", received);
			mutex_.unlock();
			NP_SUCCEED();
		}
	private:
		QMutex mutex_;
		QMap<int, QPointer<Connect>> clients_;
	};
	class ClientProcessor : public Processor
	{
		Q_OBJECT
	public:
		ClientProcessor() = default;
		~ClientProcessor() override = default;
		int receivedMessageCount() const { return receivedMessageCount_; }
		QVariantMap lastReceived() const { return lastReceived_; }
	public slots:
		void receivedMessage(const QVariantMap& received)
		{
			mutex_.lock();
			++receivedMessageCount_;
			lastReceived_ = received;
			mutex_.unlock();
		}
	private:
		QMutex mutex_;
		int receivedMessageCount_ = 0;
		QVariantMap lastReceived_;
	};
}
#endif//CPP_FUSIONTEST2_HPP_
