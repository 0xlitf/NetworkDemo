
#ifndef NETWORK_INCLUDE_NETWORK_PACKAGE_H_
#define NETWORK_INCLUDE_NETWORK_PACKAGE_H_

#include <QVariant>

#include "foundation.h"
class QFileInfo;
class QDateTime;
class Package {
private:
	Package() = default;

public:
	~Package() = default;

	Package(const Package&) = delete;

	Package& operator=(const Package&) = delete;

public:
	static inline int headSize() {
		return sizeof(m_head);
	}

	static qint32 checkDataIsReadyReceive(const QByteArray& rawData);

	static QSharedPointer<Package> readPackage(QByteArray& rawData);

	static QList<QSharedPointer<Package>> createPayloadTransportPackages(
		const QString& targetActionFlag,
		const QByteArray& payloadData,
		const QVariantMap& appendData,
		const qint32& randomFlag,
		qint64 cutPackageSize = -1,
		const bool& compressionData = false);

	static QSharedPointer<Package> createFileTransportPackage(
		const QString& targetActionFlag,
		const QFileInfo& fileInfo,
		const QByteArray& fileData,
		const QVariantMap& appendData,
		const qint32& randomFlag,
		const bool& compressionData = false);

	static QSharedPointer<Package> createPayloadDataRequestPackage(const qint32& randomFlag);

	static QSharedPointer<Package> createFileDataRequestPackage(const qint32& randomFlag);

	QDateTime fileCreatedTime() const;

	QDateTime fileLastReadTime() const;

	QDateTime fileLastModifiedTime() const;

	inline bool isCompletePackage() const {
		return m_isCompletePackage;
	}

	inline bool isAbandonPackage() const {
		return m_isAbandonPackage;
	}

	inline qint8 bootFlag() const {
		return m_head.bootFlag;
	}

	inline qint8 packageFlag() const {
		return m_head.packageFlag;
	}

	inline qint32 randomFlag() const {
		return m_head.randomFlag;
	}

	inline qint8 metaDataFlag() const {
		return m_head.metaDataFlag;
	}

	inline qint32 metaDataTotalSize() const {
		return m_head.metaDataTotalSize;
	}

	inline qint32 metaDataCurrentSize() const {
		return m_head.metaDataCurrentSize;
	}

	inline qint8 payloadDataFlag() const {
		return m_head.payloadDataFlag;
	}

	inline qint32 payloadDataTotalSize() const {
		return m_head.payloadDataTotalSize;
	}

	inline qint32 payloadDataCurrentSize() const {
		return m_head.payloadDataCurrentSize;
	}

	inline QByteArray metaData() const {
		return m_metaData;
	}

	inline int metaDataSize() const {
		return m_metaData.size();
	}

	inline QByteArray payloadData() const {
		return m_payloadData;
	}

	inline int payloadDataSize() const {
		return m_payloadData.size();
	}

	inline qint32 metaDataOriginalIndex() const {
		return m_metaDataOriginalIndex;
	}

	inline qint32 metaDataOriginalCurrentSize() const {
		return m_metaDataOriginalCurrentSize;
	}

	inline qint32 payloadDataOriginalIndex() const {
		return m_payloadDataOriginalIndex;
	}

	inline qint32 payloadDataOriginalCurrentSize() const {
		return m_payloadDataOriginalCurrentSize;
	}

	inline QVariantMap metaDataInVariantMap() const {
		return m_metaDataInVariantMap;
	}

	inline QString targetActionFlag() const {
		return (m_metaDataInVariantMap.contains("targetActionFlag"))
			? (m_metaDataInVariantMap["targetActionFlag"].toString())
			: (QString());
	}

	inline QVariantMap appendData() const {
		return (m_metaDataInVariantMap.contains("appendData"))
			? (m_metaDataInVariantMap["appendData"].toMap())
			: (QVariantMap());
	}

	inline QString fileName() const {
		return (m_metaDataInVariantMap.contains("fileName")) ? (m_metaDataInVariantMap["fileName"].toString()) : (QString());
	}

	inline qint64 fileSize() const {
		return (m_metaDataInVariantMap.contains("fileSize")) ? (m_metaDataInVariantMap["fileSize"].toLongLong()) : (-1);
	}

	inline qint32 filePermissions() const {
		return (m_metaDataInVariantMap.contains("filePermissions"))
			? (m_metaDataInVariantMap["filePermissions"].toInt())
			: (0);
	}

	inline bool containsFile() const {
		return !m_localFilePath.isEmpty();
	}

	inline QString localFilePath() const {
		return m_localFilePath;
	}

	inline void setLocalFilePath(const QString& localFilePath) {
		m_localFilePath = localFilePath;
	}

	inline void clearMetaData() {
		m_metaData.clear();
	}

	inline void clearPayloadData() {
		m_payloadData.clear();
	}

	inline QByteArray toByteArray() const {
		QByteArray buffer;
		buffer.append(reinterpret_cast<const char*>(&m_head), headSize());

		if (m_head.metaDataCurrentSize > 0) {
			buffer.append(m_metaData);
		}

		if (m_head.payloadDataCurrentSize > 0) {
			buffer.append(m_payloadData);
		}

		return buffer;
	}

	bool mixPackage(const QSharedPointer<Package>& mixPackage);

	void refreshPackage();

private:
	bool m_isCompletePackage = false;
	bool m_isAbandonPackage = false;

#pragma pack(push)
#pragma pack(1)
	struct Head {
		qint8 bootFlag = 0;
		qint8 packageFlag = 0;
		qint32 randomFlag = 0;
		qint8 metaDataFlag = 0;
		qint32 metaDataTotalSize = -1;
		qint32 metaDataCurrentSize = -1;
		qint8 payloadDataFlag = 0;
		qint32 payloadDataTotalSize = -1;
		qint32 payloadDataCurrentSize = -1;
	} m_head;
#pragma pack(pop)

	QByteArray m_metaData;
	QByteArray m_payloadData;
	QString m_localFilePath;
	qint32 m_metaDataOriginalIndex = -1;
	qint32 m_metaDataOriginalCurrentSize = -1;
	qint32 m_payloadDataOriginalIndex = -1;
	qint32 m_payloadDataOriginalCurrentSize = -1;
	QVariantMap m_metaDataInVariantMap;
};

#endif // NETWORK_INCLUDE_NETWORK_PACKAGE_H_
