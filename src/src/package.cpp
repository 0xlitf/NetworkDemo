
#include "package.h"

#include <QDebug>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFileInfo>
#include <QDateTime>

#define BOOL_CHECK( actual, message )                           \
    if ( !( actual ) )                                          \
    {                                                           \
        qDebug() << "Package::mixPackage:" << message;          \
        this->m_isAbandonPackage = true;                        \
        mixPackage->m_isAbandonPackage = true;                  \
        return false;                                           \
    }

qint32 Package::checkDataIsReadyReceive(const QByteArray& rawData) {
	/*
	 * Return value:
	 * > 0: Wait for more byte
	 * < 0: Error data, need to abandon
	 * = 0: Data is ready for receive
	 */
	if (rawData.size() < headSize()) {
		return headSize() - rawData.size();
	}

	const auto* head = reinterpret_cast<const Head*>(rawData.data());
	auto dataSize = rawData.size() - headSize();
	if (head->bootFlag != NETWORKPACKAGE_BOOTFLAG) {
		return -1;
	}

	switch (head->packageFlag) {
		case NETWORKPACKAGE_PAYLOADDATATRANSPORTPACKGEFLAG:
		case NETWORKPACKAGE_PAYLOADDATAREQUESTPACKGEFLAG:
		{
			break;
		}
		case NETWORKPACKAGE_FILEDATATRANSPORTPACKGEFLAG:
		case NETWORKPACKAGE_FILEDATAREQUESTPACKGEFLAG:
		{
			break;
		}
		default:
		{
			return -1;
		}
	}

	if (head->randomFlag <= 0) {
		return -1;
	}

	switch (head->metaDataFlag) {
		case NETWORKPACKAGE_UNCOMPRESSEDFLAG:
		case NETWORKPACKAGE_COMPRESSEDFLAG:
		{
			break;
		}
		default:
		{
			return -1;
		}
	}

	if (head->metaDataTotalSize < -1) {
		return -1;
	}

	if (head->metaDataCurrentSize < -1) {
		return -1;
	}

	if (head->metaDataTotalSize < head->metaDataCurrentSize) {
		return -1;
	}

	switch (head->payloadDataFlag) {
		case NETWORKPACKAGE_UNCOMPRESSEDFLAG:
		case NETWORKPACKAGE_COMPRESSEDFLAG:
		{
			break;
		}
		default:
		{
			return -1;
		}
	}
	if (head->payloadDataTotalSize < -1) {
		return -1;
	}
	if (head->payloadDataCurrentSize < -1) {
		return -1;
	}
	if (head->payloadDataTotalSize < head->payloadDataCurrentSize) {
		return -1;
	}
	auto expectDataSize = 0;
	if (head->metaDataCurrentSize > 0) {
		expectDataSize += head->metaDataCurrentSize;
	}
	if (head->payloadDataCurrentSize > 0) {
		expectDataSize += head->payloadDataCurrentSize;
	}
	if (dataSize < expectDataSize) {
		return expectDataSize - dataSize;
	}
	return 0;
}

QSharedPointer<Package> Package::readPackage(QByteArray& rawData) {
	auto package = QSharedPointer<Package>(new Package);
	auto data = rawData.data() + headSize();
	package->m_head = *reinterpret_cast<const Head*>(rawData.data());
	if (package->metaDataCurrentSize() > 0) {
		package->m_metaData.append(data, package->metaDataCurrentSize());
		data += package->metaDataCurrentSize();
	}
	if (package->payloadDataCurrentSize() > 0) {
		package->m_payloadData.append(data, package->payloadDataCurrentSize());
		data += package->payloadDataCurrentSize();
	}
	rawData.remove(0, static_cast<int>(data - rawData.data()));
	package->refreshPackage();
	return package;
}

QList<QSharedPointer<Package>> Package::createPayloadTransportPackages(
	const QString& targetActionFlag,
	const QByteArray& payloadData,
	const QVariantMap& appendData,
	const qint32& randomFlag,
	const qint64 cutPackageSize,
	const bool& compressionData
) {
	QList<QSharedPointer<Package>> result;
	QByteArray metaData;
	if (!targetActionFlag.isEmpty() || !appendData.isEmpty()) {
		QVariantMap metaDataInVariantMap;
		metaDataInVariantMap["targetActionFlag"] = targetActionFlag;
		metaDataInVariantMap["appendData"] = appendData;
		metaData = QJsonDocument(QJsonObject::fromVariantMap(metaDataInVariantMap)).toJson(QJsonDocument::Compact);
	}
	if (payloadData.isEmpty()) {
		auto package = QSharedPointer<Package>(new Package);
		package->m_head.bootFlag = NETWORKPACKAGE_BOOTFLAG;
		package->m_head.packageFlag = NETWORKPACKAGE_PAYLOADDATATRANSPORTPACKGEFLAG;
		package->m_head.randomFlag = randomFlag;
		package->m_head.metaDataFlag = NETWORKPACKAGE_UNCOMPRESSEDFLAG;
		package->m_head.payloadDataFlag = NETWORKPACKAGE_UNCOMPRESSEDFLAG;
		if (!metaData.isEmpty()) {
			package->m_head.metaDataTotalSize = metaData.size();
			package->m_head.metaDataCurrentSize = metaData.size();
			package->m_metaData = metaData;
		}
		package->m_metaDataOriginalIndex = 0;
		package->m_metaDataOriginalCurrentSize = 0;
		package->m_payloadDataOriginalIndex = 0;
		package->m_payloadDataOriginalCurrentSize = 0;
		package->m_isCompletePackage = true;
		result.push_back(package);
	} else {
		for (auto index = 0; index < payloadData.size();) {
			auto package = QSharedPointer<Package>(new Package);
			package->m_head.bootFlag = NETWORKPACKAGE_BOOTFLAG;
			package->m_head.packageFlag = NETWORKPACKAGE_PAYLOADDATATRANSPORTPACKGEFLAG;
			package->m_head.randomFlag = randomFlag;
			package->m_head.metaDataFlag = NETWORKPACKAGE_UNCOMPRESSEDFLAG;
			if (!metaData.isEmpty()) {
				package->m_head.metaDataTotalSize = metaData.size();
				if (!index) {
					package->m_head.metaDataCurrentSize = metaData.size();
					package->m_metaData = metaData;
				} else {
					package->m_head.metaDataCurrentSize = 0;
				}
			}
			package->m_head.payloadDataFlag = (compressionData)
				? (NETWORKPACKAGE_COMPRESSEDFLAG)
				: (NETWORKPACKAGE_UNCOMPRESSEDFLAG);
			package->m_head.payloadDataTotalSize = payloadData.size();
			if (cutPackageSize == -1) {
				package->m_payloadData = (compressionData) ? (qCompress(payloadData, 4)) : (payloadData);
				package->m_head.payloadDataCurrentSize = package->m_payloadData.size();
				package->m_isCompletePackage = true;
				package->m_metaDataOriginalIndex = 0;
				package->m_metaDataOriginalCurrentSize = 0;
				package->m_payloadDataOriginalIndex = index;
				package->m_payloadDataOriginalCurrentSize = payloadData.size();
				index = payloadData.size();
			} else {
				if ((index + cutPackageSize) > payloadData.size()) {
					package->m_payloadData = (compressionData)
						? (qCompress(payloadData.mid(index), 4))
						: (payloadData.mid(index));
					package->m_head.payloadDataCurrentSize = package->m_payloadData.size();
					package->m_isCompletePackage = result.isEmpty();
					if (index == 0) {
						package->m_metaDataOriginalIndex = 0;
						package->m_metaDataOriginalCurrentSize = 0;
					}
					package->m_payloadDataOriginalIndex = index;
					package->m_payloadDataOriginalCurrentSize = payloadData.size() - index;
					index = payloadData.size();
				} else {
					package->m_payloadData = (compressionData)
						? (qCompress(payloadData.mid(index, static_cast<int>(cutPackageSize)),
							4))
						: (payloadData.mid(index, static_cast<int>(cutPackageSize)));
					package->m_head.payloadDataCurrentSize = package->m_payloadData.size();
					package->m_isCompletePackage = !index && ((index + cutPackageSize) == payloadData.size());
					package->m_payloadDataOriginalIndex = index;
					package->m_payloadDataOriginalCurrentSize = static_cast<int>(cutPackageSize);
					index += cutPackageSize;
				}
			}
			result.push_back(package);
		}
	}
	return result;
}

QSharedPointer<Package> Package::createFileTransportPackage(
	const QString& targetActionFlag,
	const QFileInfo& fileInfo,
	const QByteArray& fileData,
	const QVariantMap& appendData,
	const qint32& randomFlag,
	const bool& compressionData
) {
	QSharedPointer<Package> package(new Package);
	QByteArray metaData;
	{
		QVariantMap metaDataInVariantMap;
		metaDataInVariantMap["targetActionFlag"] = targetActionFlag;
		metaDataInVariantMap["appendData"] = appendData;
		if (fileInfo.isFile()) {
			metaDataInVariantMap["fileName"] = fileInfo.fileName();
			metaDataInVariantMap["fileSize"] = fileInfo.size();
			metaDataInVariantMap["filePermissions"] = static_cast<qint32>(fileInfo.permissions());
			metaDataInVariantMap["fileCreatedTime"] = fileInfo.birthTime().toMSecsSinceEpoch();
			metaDataInVariantMap["fileLastReadTime"] = fileInfo.lastRead().toMSecsSinceEpoch();
			metaDataInVariantMap["fileLastModifiedTime"] = fileInfo.lastModified().toMSecsSinceEpoch();
		}
		metaData = QJsonDocument(QJsonObject::fromVariantMap(metaDataInVariantMap)).toJson(QJsonDocument::Compact);
	}
	package->m_head.bootFlag = NETWORKPACKAGE_BOOTFLAG;
	package->m_head.packageFlag = NETWORKPACKAGE_FILEDATATRANSPORTPACKGEFLAG;
	package->m_head.randomFlag = randomFlag;
	package->m_head.metaDataFlag = NETWORKPACKAGE_UNCOMPRESSEDFLAG;
	package->m_head.metaDataTotalSize = metaData.size();
	package->m_head.metaDataCurrentSize = metaData.size();
	package->m_metaData = metaData;
	package->m_head.payloadDataFlag = (compressionData)
		? (NETWORKPACKAGE_COMPRESSEDFLAG)
		: (NETWORKPACKAGE_UNCOMPRESSEDFLAG);
	package->m_head.payloadDataTotalSize = fileData.size();
	package->m_payloadData = (compressionData) ? (qCompress(fileData, 4)) : (fileData);
	package->m_head.payloadDataCurrentSize = package->m_payloadData.size();
	package->m_metaDataOriginalIndex = 0;
	package->m_metaDataOriginalCurrentSize = 0;
	package->m_payloadDataOriginalIndex = 0;
	package->m_payloadDataOriginalCurrentSize = fileData.size();
	return package;
}

QSharedPointer<Package> Package::createPayloadDataRequestPackage(const qint32& randomFlag) {
	auto package = QSharedPointer<Package>(new Package);
	package->m_head.bootFlag = NETWORKPACKAGE_BOOTFLAG;
	package->m_head.packageFlag = NETWORKPACKAGE_PAYLOADDATAREQUESTPACKGEFLAG;
	package->m_head.randomFlag = randomFlag;
	package->m_head.metaDataFlag = NETWORKPACKAGE_UNCOMPRESSEDFLAG;
	package->m_head.payloadDataFlag = NETWORKPACKAGE_UNCOMPRESSEDFLAG;
	return package;
}

QSharedPointer<Package> Package::createFileDataRequestPackage(const qint32& randomFlag) {
	auto package = QSharedPointer<Package>(new Package);
	package->m_head.bootFlag = NETWORKPACKAGE_BOOTFLAG;
	package->m_head.packageFlag = NETWORKPACKAGE_FILEDATAREQUESTPACKGEFLAG;
	package->m_head.randomFlag = randomFlag;
	package->m_head.metaDataFlag = NETWORKPACKAGE_UNCOMPRESSEDFLAG;
	package->m_head.payloadDataFlag = NETWORKPACKAGE_UNCOMPRESSEDFLAG;
	return package;
}

QDateTime Package::fileCreatedTime() const {
	return (m_metaDataInVariantMap.contains("fileCreatedTime"))
		? (QDateTime::fromMSecsSinceEpoch(m_metaDataInVariantMap["fileCreatedTime"].toLongLong()))
		: (QDateTime());
}

QDateTime Package::fileLastReadTime() const {
	return (m_metaDataInVariantMap.contains("fileLastReadTime"))
		? (QDateTime::fromMSecsSinceEpoch(m_metaDataInVariantMap["fileLastReadTime"].toLongLong()))
		: (QDateTime());
}

QDateTime Package::fileLastModifiedTime() const {
	return (m_metaDataInVariantMap.contains("fileLastModifiedTime"))
		? (QDateTime::fromMSecsSinceEpoch(m_metaDataInVariantMap["fileLastModifiedTime"].toLongLong()))
		: (QDateTime());
}

bool Package::mixPackage(const QSharedPointer<Package>& mixPackage) {
	BOOL_CHECK(!this->isCompletePackage(), "current package is complete");
	BOOL_CHECK(!mixPackage->isCompletePackage(), "mix package is complete");
	BOOL_CHECK(!this->isAbandonPackage(), "current package is abandon package");
	BOOL_CHECK(!mixPackage->isAbandonPackage(), "mix package is abandon package");
	BOOL_CHECK(this->randomFlag() == mixPackage->randomFlag(), "randomFlag not same");
	BOOL_CHECK(this->metaDataTotalSize() == mixPackage->metaDataTotalSize(), "metaDataTotalSize not same");
	BOOL_CHECK((this->metaDataCurrentSize() + mixPackage->metaDataCurrentSize()) <= this->metaDataTotalSize(),
		"metaDataCurrentSize overmuch");
	BOOL_CHECK(this->payloadDataTotalSize() == mixPackage->payloadDataTotalSize(), "payloadDataTotalSize not same");
	BOOL_CHECK(
		(this->payloadDataCurrentSize() + mixPackage->payloadDataCurrentSize()) <= this->payloadDataTotalSize(),
		"payloadDataCurrentSize overmuch");
	BOOL_CHECK(((this->metaDataTotalSize() > 0) || (this->payloadDataTotalSize() > 0)), "data error");
	if (this->metaDataTotalSize() > 0) {
		this->m_metaData.append(mixPackage->metaData());
		this->m_head.metaDataCurrentSize += mixPackage->metaDataCurrentSize();
	}
	if (this->payloadDataTotalSize() > 0) {
		this->m_payloadData.append(mixPackage->payloadData());
		this->m_head.payloadDataCurrentSize += mixPackage->payloadDataCurrentSize();
	}
	this->refreshPackage();
	return true;
}

void Package::refreshPackage() {
	if (m_head.metaDataFlag == NETWORKPACKAGE_COMPRESSEDFLAG) {
		m_head.metaDataFlag = NETWORKPACKAGE_UNCOMPRESSEDFLAG;
		m_metaData = qUncompress(m_metaData);
		m_head.metaDataCurrentSize = m_metaData.size();
	}
	if (m_head.payloadDataFlag == NETWORKPACKAGE_COMPRESSEDFLAG) {
		m_head.payloadDataFlag = NETWORKPACKAGE_UNCOMPRESSEDFLAG;
		m_payloadData = qUncompress(m_payloadData);
		m_head.payloadDataCurrentSize = m_payloadData.size();
	}
	if (this->metaDataTotalSize() != this->metaDataCurrentSize()) {
		return;
	}
	if (this->payloadDataTotalSize() != this->payloadDataCurrentSize()) {
		return;
	}
	this->m_isCompletePackage = true;
	if (!m_metaData.isEmpty()) {
		m_metaDataInVariantMap = QJsonDocument::fromJson(m_metaData).object().toVariantMap();
	}
}
