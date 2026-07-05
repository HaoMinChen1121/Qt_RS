#ifndef SENSORPRODUCTFACTORY_H
#define SENSORPRODUCTFACTORY_H

#include <QFileInfo>
#include <QDir>
#include <QString>
#include "dataaccess/ISensorProduct.h"
#include "dataaccess/impl/Sentinel2Product.h"
#include "dataaccess/impl/LandsatProduct.h"
#include "dataaccess/impl/GfProduct.h"
#include "dataaccess/impl/DescriptorProduct.h"

/**
 * @brief 根据文件路径自动识别传感器类型并创建对应产品
 * @return 产品指针（调用方负责 delete），无法识别返回 nullptr
 */
inline ISensorProduct* createSensorProduct(const QString& path)
{
    QFileInfo fi(path);

    // ── 通用处理产物描述符 (.rpp) ──
    if (path.endsWith(".rpp", Qt::CaseInsensitive))
        return new DescriptorProduct();

    // ── Sentinel-2 ──
    if (path.endsWith(".zip", Qt::CaseInsensitive))
    {
        QString base = fi.completeBaseName();
        if (base.startsWith("S2A_", Qt::CaseInsensitive) ||
            base.startsWith("S2B_", Qt::CaseInsensitive) ||
            base.startsWith("S2_",  Qt::CaseInsensitive))
            return new Sentinel2Product();
    }
    if (path.endsWith(".SAFE", Qt::CaseInsensitive))
        return new Sentinel2Product();
    if (fi.isDir() && QFileInfo::exists(path + "/manifest.safe"))
        return new Sentinel2Product();

    // ── Landsat ──
    if (fi.isFile() && fi.fileName().endsWith("_MTL.txt", Qt::CaseInsensitive))
        return new LandsatProduct();
    // Landsat Collection 2 tar 归档
    if (path.endsWith(".tar", Qt::CaseInsensitive))
    {
        QString fn = fi.completeBaseName();
        if (fn.startsWith("LC0") || fn.startsWith("LO0") ||
            fn.startsWith("LM0") || fn.startsWith("LT0"))
            return new LandsatProduct();
    }
    if (fi.isDir())
    {
        QStringList mtl = QDir(path).entryList({"*_MTL.txt"}, QDir::Files);
        if (!mtl.isEmpty())
            return new LandsatProduct();
    }

    // ── 高分系列 ──
    if (fi.isDir())
    {
        QString dn = fi.fileName();
        if (dn.contains("GF-", Qt::CaseInsensitive) ||
            dn.contains("GF1", Qt::CaseInsensitive) ||
            dn.contains("GF2", Qt::CaseInsensitive) ||
            dn.contains("GF6", Qt::CaseInsensitive))
            return new GfProduct();
    }

    return nullptr;
}

#endif // SENSORPRODUCTFACTORY_H
