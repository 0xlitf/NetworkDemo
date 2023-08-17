
#ifndef NETWORK_INCLUDE_NETWORK_PACKAGE_H_
#define NETWORK_INCLUDE_NETWORK_PACKAGE_H_

#include <QVariant>

#include "foundation.h"
class QFileInfo;
class QDateTime;
class Package
{
private:
	Package() = default;

public:
	~Package() = default;
	Package(const Package &) = delete;
	Package &operator=(const Package &) = delete;

public:
	static inline int headSize()
	{
		return sizeof(head_);
	}
	static qint32 checkDataIsReadyReceive(const QByteArray &rawData);
	static QSharedPointer<Package> readPackage(QByteArray &rawData);
	static QList<QSharedPointer<Package>> createPayloadTransportPackages(
		const QString &targetActionFlag,
		const QByteArray &payloadData,
		const QVariantMap &appendData,
		const qint32 &randomFlag,
		qint64 cutPackageSize = -1,
		const bool &compressionData = false);
	static QSharedPointer<Package> createFileTransportPackage(
		const QString &targetActionFlag,
		const QFileInfo &fileInfo,
		const QByteArray &fileData,
		const QVariantMap &appendData,
		const qint32 &randomFlag,
		const bool &compressionData = false);
	static QSharedPointer<Package> createPayloadDataRequestPackage(const qint32 &randomFlag);
	static QSharedPointer<Package> createFileDataRequestPackage(const qint32 &randomFlag);

	QDateTime fileCreatedTime() const;
	QDateTime fileLastReadTime() const;
	QDateTime fileLastModifiedTime() const;

	inline bool isCompletePackage() const
	{
		return isCompletePackage_;
	}
	inline bool isAbandonPackage() const
	{
		return isAbandonPackage_;
	}
	inline qint8 bootFlag() const
	{
		return head_.bootFlag_;
	}
	inline qint8 packageFlag() const
	{
		return head_.packageFlag_;
	}
	inline qint32 randomFlag() const
	{
		return head_.randomFlag_;
	}
	inline qint8 metaDataFlag() const
	{
		return head_.metaDataFlag_;
	}
	inline qint32 metaDataTotalSize() const
	{
		return head_.metaDataTotalSize_;
	}
	inline qint32 metaDataCurrentSize() const
	{
		return head_.metaDataCurrentSize_;
	}
	inline qint8 payloadDataFlag() const
	{
		return head_.payloadDataFlag_;
	}
	inline qint32 payloadDataTotalSize() const
	{
		return head_.payloadDataTotalSize_;
	}
	inline qint32 payloadDataCurrentSize() const
	{
		return head_.payloadDataCurrentSize_;
	}
	inline QByteArray metaData() const
	{
		return metaData_;
	}
	inline int metaDataSize() const
	{
		return metaData_.size();
	}
	inline QByteArray payloadData() const
	{
		return payloadData_;
	}
	inline int payloadDataSize() const
	{
		return payloadData_.size();
	}
	inline qint32 metaDataOriginalIndex() const
	{
		return metaDataOriginalIndex_;
	}
	inline qint32 metaDataOriginalCurrentSize() const
	{
		return metaDataOriginalCurrentSize_;
	}
	inline qint32 payloadDataOriginalIndex() const
	{
		return payloadDataOriginalIndex_;
	}
	inline qint32 payloadDataOriginalCurrentSize() const
	{
		return payloadDataOriginalCurrentSize_;
	}
	inline QVariantMap metaDataInVariantMap() const
	{
		return metaDataInVariantMap_;
	}
	inline QString targetActionFlag() const
	{
		return (metaDataInVariantMap_.contains("targetActionFlag"))
				   ? (metaDataInVariantMap_["targetActionFlag"].toString())
				   : (QString());
	}
	inline QVariantMap appendData() const
	{
		return (metaDataInVariantMap_.contains("appendData"))
				   ? (metaDataInVariantMap_["appendData"].toMap())
				   : (QVariantMap());
	}
	inline QString fileName() const
	{
		return (metaDataInVariantMap_.contains("fileName")) ? (metaDataInVariantMap_["fileName"].toString()) : (QString());
	}
	inline qint64 fileSize() const
	{
		return (metaDataInVariantMap_.contains("fileSize")) ? (metaDataInVariantMap_["fileSize"].toLongLong()) : (-1);
	}
	inline qint32 filePermissions() const
	{
		return (metaDataInVariantMap_.contains("filePermissions"))
				   ? (metaDataInVariantMap_["filePermissions"].toInt())
				   : (0);
	}
	inline bool containsFile() const
	{
		return !localFilePath_.isEmpty();
	}
	inline QString localFilePath() const
	{
		return localFilePath_;
	}
	inline void setLocalFilePath(const QString &localFilePath)
	{
		localFilePath_ = localFilePath;
	}
	inline void clearMetaData()
	{
		metaData_.clear();
	}
	inline void clearPayloadData()
	{
		payloadData_.clear();
	}
	inline QByteArray toByteArray() const
	{
		QByteArray buffer;
		buffer.append(reinterpret_cast<const char *>(&head_), headSize());
		if (head_.metaDataCurrentSize_ > 0)
		{
			buffer.append(metaData_);
		}
		if (head_.payloadDataCurrentSize_ > 0)
		{
			buffer.append(payloadData_);
		}
		return buffer;
	}
	bool mixPackage(const QSharedPointer<Package> &mixPackage);
	void refreshPackage();

private:
	bool isCompletePackage_ = false;
	bool isAbandonPackage_ = false;
#pragma pack(push)
#pragma pack(1)
	struct Head
	{
		qint8 bootFlag_ = 0;
		qint8 packageFlag_ = 0;
		qint32 randomFlag_ = 0;
		qint8 metaDataFlag_ = 0;
		qint32 metaDataTotalSize_ = -1;
		qint32 metaDataCurrentSize_ = -1;
		qint8 payloadDataFlag_ = 0;
		qint32 payloadDataTotalSize_ = -1;
		qint32 payloadDataCurrentSize_ = -1;
	} head_;
#pragma pack(pop)
	QByteArray metaData_;
	QByteArray payloadData_;
	QString localFilePath_;
	qint32 metaDataOriginalIndex_ = -1;
	qint32 metaDataOriginalCurrentSize_ = -1;
	qint32 payloadDataOriginalIndex_ = -1;
	qint32 payloadDataOriginalCurrentSize_ = -1;
	QVariantMap metaDataInVariantMap_;
};

#endif // NETWORK_INCLUDE_NETWORK_PACKAGE_H_
