
#include <typeinfo>
#include "processor.h"

#include <QDebug>
#include <QThread>
#include <QMetaObject>
#include <QMetaMethod>
#include <QVariantMap>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>

#include "server.h"
#include "connect.h"
#include "package.h"
#include <QSharedPointer>
#include <functional>
#include "qobjectdefs.h"

using Test = std::function<QGenericArgument(const std::shared_ptr<void>/*NetworkVoidSharedPointer*/& sendArg)>;
// Processor
QSet<QString> Processor::exceptionSlots_({ "deleteLater", "_q_reregisterTimers" });

Processor::Processor(const bool& invokeMethodByProcessorThread) :
	invokeMethodByProcessorThread_(invokeMethodByProcessorThread) {
	static bool flag = true;
	if (flag) {
		flag = false;
		qRegisterMetaType<QVariantMap>("QVariantMap");
	}
}

QSet<QString> Processor::availableSlots() {
	if (!availableSlots_.isEmpty()) {
		return availableSlots_;
	}
	for (auto index = 0; index < this->metaObject()->methodCount(); ++index) {
		const auto&& method = this->metaObject()->method(index);
		if (method.methodType() != QMetaMethod::Slot) {
			continue;
		}
		const auto&& methodName = method.name();
		if (exceptionSlots_.contains(methodName)) {
			continue;
		}
		if (onpackageReceivedCallbacks_.contains(methodName)) {
			qDebug() << "Processor::availableSlots: same name slot:" << methodName;
			continue;
		}
		QSharedPointer<std::function<std::shared_ptr<void>/*NetworkVoidSharedPointer*/()>> receiveArgumentPreparer;
		QSharedPointer<std::function<QGenericArgument(const std::shared_ptr<void>/*NetworkVoidSharedPointer*/& receivedArg,
			const QSharedPointer<Package>& package)>>
			receiveArgumentMaker;
		QSharedPointer<std::function<std::shared_ptr<void>/*NetworkVoidSharedPointer*/()>> sendArgumentPreparer;
		QSharedPointer<std::function<QGenericArgument(const std::shared_ptr<void>/*NetworkVoidSharedPointer*/& sendArg)>> sendArgumentMaker;
		QSharedPointer<std::function<void(const QPointer<Connect>& connect,
			const QSharedPointer<Package>& package,
			const std::shared_ptr<void>/*NetworkVoidSharedPointer*/& sendArg, const QVariantMap& sendAppend)>>
			sendArgumentAnswer;
		QSharedPointer<std::function<std::shared_ptr<void>/*NetworkVoidSharedPointer*/()>> receiveAppendArgumentPreparer;
		QSharedPointer<std::function<QGenericArgument(const std::shared_ptr<void>/*NetworkVoidSharedPointer*/& receivedAppendArg,
			const QSharedPointer<Package>& package)>>
			receiveAppendArgumentMaker;
		QSharedPointer<std::function<std::shared_ptr<void>/*NetworkVoidSharedPointer*/()>> sendAppendArgumentPreparer;
		QSharedPointer<std::function<QGenericArgument(const std::shared_ptr<void>/*NetworkVoidSharedPointer*/& sendAppendArg)>>
			sendAppendArgumentMaker;
		if (method.parameterTypes().size() >= 1) {
			const auto&& currentSum = QString("%1:%2").arg(QString(method.parameterTypes()[0]),
				QString(method.parameterNames()[0]));
			qDebug() << "currentSum: " << currentSum;
			if (currentSum == "QByteArray:received") {
				receiveArgumentPreparer.reset(new std::function<std::shared_ptr<void>/*NetworkVoidSharedPointer*/()>([]() {
					return std::shared_ptr<void>/*NetworkVoidSharedPointer*/(new QByteArray, &Processor::deleteByteArray);
					}));
				receiveArgumentMaker.reset(
					new std::function<QGenericArgument(const std::shared_ptr<void>&receivedArg,
					const QSharedPointer<Package> &package)>(
					[](const auto& receivedArg, const auto& package) {
						(*static_cast<QByteArray*>(receivedArg.get())) = package->payloadData();
						qDebug() << "receivedArg.get(): " << *static_cast<const QByteArray*>(receivedArg.get());
						return QArgument<const QByteArray&>("const QByteArray&",
							*static_cast<const QByteArray*>(receivedArg.get()));
					}));
			} else if (currentSum == "QVariantMap:received") {
				receiveArgumentPreparer.reset(new std::function<std::shared_ptr<void>/*NetworkVoidSharedPointer*/()>([]() {
					return std::shared_ptr<void>/*NetworkVoidSharedPointer*/(new QVariantMap, &Processor::deleteVariantMap);
					}));
				receiveArgumentMaker.reset(
					new std::function<QGenericArgument(const std::shared_ptr<void>&receivedArg,
					const QSharedPointer<Package> &package)>(
					[](const auto& receivedArg, const auto& package) {
						(*static_cast<QVariantMap*>(receivedArg.get())) = QJsonDocument::fromJson(
							package->payloadData()).object().toVariantMap();
						return QArgument<const QVariantMap&>("const QVariantMap&",
							*static_cast<const QVariantMap*>(receivedArg.get()));
					}));
			} else if (currentSum == "QFileInfo:received") {
				receiveArgumentPreparer.reset(new std::function<std::shared_ptr<void>/*NetworkVoidSharedPointer*/()>([]() {
					return std::shared_ptr<void>/*NetworkVoidSharedPointer*/(new QFileInfo, &Processor::deleteFileInfo);
					}));
				receiveArgumentMaker.reset(
					new std::function<QGenericArgument(const std::shared_ptr<void>&receivedArg,
					const QSharedPointer<Package> &package)>(
					[](const auto& receivedArg, const auto& package) {
						(*static_cast<QFileInfo*>(receivedArg.get())) = QFileInfo(package->localFilePath());
						return QArgument<const QFileInfo&>("const QFileInfo&",
							*static_cast<const QFileInfo*>(receivedArg.get()));
					}));
			} else if (!method.parameterNames()[0].isEmpty()) {
				qDebug() << "Processor::availableSlots: Unknow argument:" << currentSum;
				continue;
			}
		}
		if (method.parameterTypes().size() >= 2) {
			const auto&& currentSum = QString("%1:%2").arg(QString(method.parameterTypes()[1]),
				QString(method.parameterNames()[1]));
			if (currentSum == "QByteArray&:send") {
				sendArgumentPreparer.reset(new std::function<std::shared_ptr<void>/*NetworkVoidSharedPointer*/()>([]() {
					return std::shared_ptr<void>/*NetworkVoidSharedPointer*/(new QByteArray, &Processor::deleteByteArray);
					}));
				sendArgumentMaker.reset(new std::function<QGenericArgument(const std::shared_ptr<void>&sendArg)>(
					[](const auto& sendArg) {
						return QArgument<QByteArray&>("QByteArray&", *static_cast<QByteArray*>(sendArg.get()));
					}));
				sendArgumentAnswer.reset(new std::function<void(
					const QPointer<Connect> &connect,
					const QSharedPointer<Package> &package,
					const std::shared_ptr<void>/*NetworkVoidSharedPointer*/ &sendArg,
					const QVariantMap & sendAppend
				)>([](
					const auto& connect,
					const auto& package,
					const auto& sendArg,
					const auto& sendAppend
				) {
					if (!connect) {
						qDebug() << "Processor::availableSlots: connect is null";
						return;
					}
					if (!package->randomFlag()) {
						qDebug() <<
							"Processor::availableSlots: when the randomFlag is 0, the reply is not allowed";
						return;
					}
					const auto&& replyReply = connect->replyPayloadData(
						package->randomFlag(),
						*static_cast<QByteArray*>(sendArg.get()),
						sendAppend
					);
					if (!replyReply) {
						qDebug() << "Processor::availableSlots: replyPayloadData error";
					}
					}));
			} else if (currentSum == "QVariantMap&:send") {
				sendArgumentPreparer.reset(new std::function<std::shared_ptr<void>/*NetworkVoidSharedPointer*/()>([]() {
					return std::shared_ptr<void>/*NetworkVoidSharedPointer*/(new QVariantMap, &Processor::deleteVariantMap);
					}));
				sendArgumentMaker.reset(new std::function<QGenericArgument(const std::shared_ptr<void>/*NetworkVoidSharedPointer*/ &sendArg)>(
					[](const auto& sendArg) {
						return QArgument<QVariantMap&>("QVariantMap&", *static_cast<QVariantMap*>(sendArg.get()));
					}));
				sendArgumentAnswer.reset(new std::function<void(
					const QPointer<Connect> &connect,
					const QSharedPointer<Package> &package,
					const std::shared_ptr<void>/*NetworkVoidSharedPointer*/ &sendArg,
					const QVariantMap & sendAppend
				)>([](
					const auto& connect,
					const auto& package,
					const auto& sendArg,
					const auto& sendAppend
				) {
					if (!connect) {
						qDebug() << "Processor::availableSlots: connect is null";
						return;
					}
					if (!package->randomFlag()) {
						qDebug() <<
							"Processor::availableSlots: when the randomFlag is 0, the reply is not allowed";
						return;
					}
					const auto&& replyReply = connect->replyPayloadData(
						package->randomFlag(),
						QJsonDocument(QJsonObject::fromVariantMap(*static_cast<QVariantMap*>(sendArg.get()))).
						toJson(QJsonDocument::Compact),
						sendAppend
					);
					if (!replyReply) {
						qDebug() << "Processor::availableSlots: replyPayloadData error";
					}
					}));
			} else if (currentSum == "QFileInfo&:send") {
				sendArgumentPreparer.reset(new std::function<std::shared_ptr<void>/*NetworkVoidSharedPointer*/()>([]() {
					return std::shared_ptr<void>/*NetworkVoidSharedPointer*/(new QFileInfo, &Processor::deleteFileInfo);
					}));
				sendArgumentMaker.reset(new std::function<QGenericArgument(const std::shared_ptr<void>/*NetworkVoidSharedPointer*/ &sendArg)>(
					[](const auto& sendArg) {
						return QArgument<QFileInfo&>("QFileInfo&", *static_cast<QFileInfo*>(sendArg.get()));
					}));
				sendArgumentAnswer.reset(new std::function<void(
					const QPointer<Connect> &connect,
					const QSharedPointer<Package> &package,
					const std::shared_ptr<void>/*NetworkVoidSharedPointer*/ &sendArg,
					const QVariantMap & sendAppend
				)>([](
					const auto& connect,
					const auto& package,
					const auto& sendArg,
					const auto& sendAppend
				) {
					if (!connect) {
						qDebug() << "Processor::availableSlots: connect is null";
						return;
					}
					if (!package->randomFlag()) {
						qDebug() <<
							"Processor::availableSlots: when the randomFlag is 0, the reply is not allowed";
						return;
					}
					const auto& sendFileInfo = *static_cast<QFileInfo*>(sendArg.get());
					if (!sendFileInfo.isFile()) {
						qDebug() << "Processor::availableSlots: current fileinfo is not file:" <<
							sendFileInfo.filePath();
						return;
					}
					const auto&& replyReply = connect->replyFile(
						package->randomFlag(),
						sendFileInfo,
						sendAppend
					);
					if (!replyReply) {
						qDebug() << "Processor::availableSlots: replyPayloadData error";
					}
					}));
			} else if (!method.parameterNames()[1].isEmpty()) {
				qDebug() << "Processor::availableSlots: Unknow argument:" << currentSum;
				continue;
			}
		}
		if (method.parameterTypes().size() >= 3) {
			const auto&& currentSum = QString("%1:%2").arg(QString(method.parameterTypes()[2]),
				QString(method.parameterNames()[2]));
			if (currentSum == "QVariantMap:receivedAppend") {
				receiveAppendArgumentPreparer.reset(new std::function<std::shared_ptr<void>/*NetworkVoidSharedPointer*/()>([]() {
					return std::shared_ptr<void>/*NetworkVoidSharedPointer*/(new QVariantMap, &Processor::deleteVariantMap);
					}));
				receiveAppendArgumentMaker.reset(
					new std::function<QGenericArgument(const std::shared_ptr<void>/*NetworkVoidSharedPointer*/ &receivedAppendArg,
					const QSharedPointer<Package> &package)>(
					[](const auto& receivedAppendArg, const auto& package) {
						(*static_cast<QVariantMap*>(receivedAppendArg.get())) = package->appendData();
						return QArgument<const QVariantMap&>("const QVariantMap&",
							*static_cast<const QVariantMap*>(receivedAppendArg.
							get()));
					}));
			} else if (!method.parameterNames()[2].isEmpty()) {
				qDebug() << "Processor::availableSlots: Unknow argument:" << currentSum;
				continue;
			}
		}
		if (method.parameterTypes().size() >= 4) {
			const auto&& currentSum = QString("%1:%2").arg(QString(method.parameterTypes()[3]),
				QString(method.parameterNames()[3]));
			if (currentSum == "QVariantMap&:sendAppend") {
				sendAppendArgumentPreparer.reset(new std::function<std::shared_ptr<void>/*NetworkVoidSharedPointer*/()>([]() {
					return std::shared_ptr<void>/*NetworkVoidSharedPointer*/(new QVariantMap, &Processor::deleteVariantMap);
					}));
				sendAppendArgumentMaker.reset(
					new std::function<QGenericArgument(const std::shared_ptr<void>/*NetworkVoidSharedPointer*/ &sendAppendArg)>(
					[](const auto& sendAppendArg) {
						return QArgument<QVariantMap&>("QVariantMap&",
						*static_cast<QVariantMap*>(sendAppendArg.get()));
					}));
			} else if (!method.parameterNames()[3].isEmpty()) {
				qDebug() << "Processor::availableSlots: Unknow argument:" << currentSum;
				continue;
			}
		}
		onpackageReceivedCallbacks_[methodName] =
			[
				this,
					methodName,
					receiveArgumentPreparer,
					receiveArgumentMaker,
					sendArgumentPreparer,
					sendArgumentMaker,
					sendArgumentAnswer,
					receiveAppendArgumentPreparer,
					receiveAppendArgumentMaker,
					sendAppendArgumentPreparer,
					sendAppendArgumentMaker
			]
			(const auto& connect, const auto& package)
		{
			std::shared_ptr<void>/*NetworkVoidSharedPointer*/ receiveArg;
			if (receiveArgumentPreparer) {
				receiveArg = (*receiveArgumentPreparer)();
			}
			std::shared_ptr<void>/*NetworkVoidSharedPointer*/ sendArg;
			if (sendArgumentPreparer) {
				sendArg = (*sendArgumentPreparer)();
			}
			std::shared_ptr<void>/*NetworkVoidSharedPointer*/ receiveAppendArg;
			if (receiveAppendArgumentPreparer) {
				receiveAppendArg = (*receiveAppendArgumentPreparer)();
			}
			std::shared_ptr<void>/*NetworkVoidSharedPointer*/ sendAppendArg;
			if (sendAppendArgumentPreparer) {
				sendAppendArg = (*sendAppendArgumentPreparer)();
			}
			qDebug() << "this: " << this;
			qDebug() << "methodName.data(): " << methodName.data();
			qDebug() << "invokeMethodByProcessorThread_: " << invokeMethodByProcessorThread_;
			qDebug() << "receiveArgumentMaker: " << receiveArgumentMaker << typeid(receiveArgumentMaker).name();
			qDebug() << "sendArgumentMaker: " << sendArgumentMaker << typeid(sendArgumentMaker).name();
			qDebug() << "receiveAppendArgumentMaker: " << receiveAppendArgumentMaker << typeid(
				receiveAppendArgumentMaker).name();
			qDebug() << "sendAppendArgumentMaker: " << sendAppendArgumentMaker << typeid(sendAppendArgumentMaker).
				name();
			const auto&& invokeMethodReply = QMetaObject::invokeMethod(
				this,
				methodName.data(),
				((invokeMethodByProcessorThread_) ? (Qt::QueuedConnection) : (Qt::DirectConnection)),
				((receiveArgumentMaker) ? ((*receiveArgumentMaker)(receiveArg, package)) : (QGenericArgument())),
				((sendArgumentMaker) ? ((*sendArgumentMaker)(sendArg)) : (QGenericArgument())),
				((receiveAppendArgumentMaker)
				? ((*receiveAppendArgumentMaker)(receiveAppendArg, package))
				: (QGenericArgument())),
				((sendAppendArgumentMaker) ? ((*sendAppendArgumentMaker)(sendAppendArg)) : (QGenericArgument()))
			);
			if (!invokeMethodReply) {
				qDebug() << "Processor::availableSlots: invokeMethod slot error:" << methodName;
			}
			if (sendArgumentAnswer) {
				if (sendAppendArg) {
					(*sendArgumentAnswer)(connect, package, sendArg,
						*static_cast<const QVariantMap*>(sendAppendArg.get()));
				} else {
					(*sendArgumentAnswer)(connect, package, sendArg, {});
				}
			}
		};
		availableSlots_.insert(methodName);
	}
	return availableSlots_;
}

bool Processor::handlePackage(const QPointer<Connect>& connect, const QSharedPointer<Package>& package) {
	qDebug() << "connect: " << connect;
	auto currentThreadConnect = connectMapByThread_.find(QThread::currentThread());
	if (currentThreadConnect == connectMapByThread_.end()) {
		qDebug() << "Processor::onPackageReceived: expectation thread:" << QThread::currentThread();
		return false;
	}
	*currentThreadConnect = connect;
	const auto&& targetActionFlag = package->targetActionFlag();
	QMap<QString, std::function<void(const QPointer<Connect>&, const QSharedPointer<Package>&)>>::iterator
		itForCallback = onpackageReceivedCallbacks_.find(targetActionFlag);
	qDebug() << "targetActionFlag: " << targetActionFlag;
	qDebug() << "itForCallback: " << itForCallback.key();

	if (itForCallback == onpackageReceivedCallbacks_.end()) {
		qDebug() << "Processor::onPackageReceived: expectation targetActionFlag:" << targetActionFlag;
		*currentThreadConnect = nullptr;
		return false;
	}
	if (!connect) {
		return false;
	}
	(*itForCallback)(connect, package);
	*currentThreadConnect = nullptr;
	return true;
}

void Processor::setReceivedPossibleThreads(const QSet<QThread*>& threads) {
	for (const auto& thread : threads) {
		connectMapByThread_[thread] = nullptr;
	}
}

bool Processor::checkMapContains(const QStringList& keys, const QVariantMap& received, QVariantMap& send) {
	for (const auto& key : keys) {
		if (!received.contains(key)) {
			NP_FAIL(QString("error: %1 not contains").arg(key));
		}
	}
	return true;
}

bool Processor::checkMapContainsAndNot0(const QStringList& keys, const QVariantMap& received, QVariantMap& send) {
	for (const auto& key : keys) {
		if (!received.contains(key) || !received[key].toLongLong()) {
			NP_FAIL(QString("error: %1 is 0").arg(key));
		}
	}
	return true;
}

bool Processor::checkMapContainsAndNotEmpty(const QStringList& keys, const QVariantMap& received, QVariantMap& send) {
	for (const auto& key : keys) {
		if (!received.contains(key) || received[key].toString().isEmpty()) {
			NP_FAIL(QString("error: %1 is empty").arg(key));
		}
	}
	return true;
}

bool Processor::checkDataContasinsExpectedContent(const QString& key, const QVariantList& expectedContentList, const QVariantMap& received, QVariantMap& send) {
	if (!checkMapContains({ key }, received, send)) {
		return false;
	}
	const auto&& data = received[key];
	if (data.isNull()) {
		NP_FAIL(QString("error: %1 is null").arg(key))
	}
	for (const auto& expectedContent : expectedContentList) {
		if (data == expectedContent) {
			return true;
		}
	}
	auto message = QString("error: %1 not match, expected: ");
	for (auto index = 0; index < expectedContentList.length(); ++index) {
		if (!index) {
			message += "/";
		}
		message += expectedContentList[index].toString();
	}
	NP_FAIL(message);
}

QPointer<Connect> Processor::currentThreadConnect() {
	auto currentThreadConnect = connectMapByThread_.find(QThread::currentThread());
	if (currentThreadConnect == connectMapByThread_.end()) {
		qDebug() << "Processor::currentThreadConnect: expectation thread:" << QThread::currentThread();
		return nullptr;
	}
	return *currentThreadConnect;
}

void Processor::deleteFileInfo(QFileInfo* ptr) {
	delete ptr;
}
