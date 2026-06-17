#include "AtmosphericCorrector.h"
#include "dataaccess/IRasterReader.h"
#include "dataaccess/IRasterWriter.h"
#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QtMath>

namespace Radiometric
{

/// 解析外部工具路径：优先 external/<subdir>/<exe>，回退系统 PATH
static QString resolveTool(const QString& exeName, const QString& subdir)
{
    QStringList searchRoots;
    searchRoots << QCoreApplication::applicationDirPath();
    searchRoots << QDir::currentPath();

    for (const QString& start : searchRoots)
    {
        QDir dir(start);
        for (int i = 0; i < 10; ++i)
        {
            QString candidate = dir.absolutePath() + QStringLiteral("/external/")
                                + subdir + QStringLiteral("/") + exeName;
            if (QFileInfo::exists(candidate))
            {
                qDebug() << "[AtmosphericCorrector] found tool:" << candidate;
                return QDir::toNativeSeparators(candidate);
            }
            if (dir.isRoot())
                break;
            dir.cdUp();
        }
    }
    qWarning() << "[AtmosphericCorrector] tool not found:" << exeName
               << "searched from" << searchRoots;
    return exeName;
}

// ─── 6S 大气模型编号 ───
static const QMap<QString, int> s_atmModels = {
    {"Tropical",         1},
    {"MidLatSummer",     2},
    {"MidLatWinter",     3},
    {"SubArcticSummer",  4},
    {"SubArcticWinter",  5}
};

// ─── 6S 气溶胶模型编号 ───
static const QMap<QString, int> s_aerModels = {
    {"Continental",  1},
    {"Maritime",     2},
    {"Urban",        3},
    {"Desert",       4}
};

// ─── Py6S 大气模型名映射 ───
static const QMap<QString, QString> s_py6sAtmProfile = {
    {"Tropical",         QStringLiteral("AtmosProfile.Tropical")},
    {"MidLatSummer",     QStringLiteral("AtmosProfile.MidlatitudeSummer")},
    {"MidLatWinter",     QStringLiteral("AtmosProfile.MidlatitudeWinter")},
    {"SubArcticSummer",  QStringLiteral("AtmosProfile.SubarcticSummer")},
    {"SubArcticWinter",  QStringLiteral("AtmosProfile.SubarcticWinter")}
};

// ─── Py6S 气溶胶模型名映射 ───
static const QMap<QString, QString> s_py6sAerProfile = {
    {"Continental", QStringLiteral("AeroProfile.Continental")},
    {"Maritime",    QStringLiteral("AeroProfile.Maritime")},
    {"Urban",       QStringLiteral("AeroProfile.Urban")},
    {"Desert",      QStringLiteral("AeroProfile.Desert")}
};

// ══════════════════════════════════════════════════════════════════════════════
// 6S 输入文件生成
// ══════════════════════════════════════════════════════════════════════════════

QByteArray AtmosphericCorrector::generate6sInputData(const RadiometricCorrectionParams& params,
                                                  const SensorInfo& sensorInfo,
                                                  double wlMin, double wlMax)
{
    QByteArray data;
    QTextStream out(&data);

    // 6S V2.1 输入格式（与 V1.1 不同，多了 igroun→ro、irapp 参数，无需末尾 -1）
    //
    // 校正路线由 RAPP 符号决定：
    //   正值 → 辐亮度模式 → 6S 输出 xa（路线A: DN→辐亮度→大气校正）
    //   负值 → 反射率模式 → 6S 输出 xap（路线B: DN→反射率→大气校正）
    const bool isRadiance = (params.calibrationType == QStringLiteral("DN2Radiance"));
    const double rapp = isRadiance ? 100.0 : -0.2;

    out << "0\n"
        << sensorInfo.solarZenithAngle << " "
        << sensorInfo.solarAzimuthAngle << " "
        << sensorInfo.sensorZenithAngle << " "
        << sensorInfo.sensorAzimuthAngle << " "
        << sensorInfo.acquisitionTime.date().month() << " "
        << sensorInfo.acquisitionTime.date().day() << "\n"
        << s_atmModels.value(params.atmosphericModel, 2) << "\n"
        << s_aerModels.value(params.aerosolModel, 1) << "\n"
        << "0\n"                // v=0 → 6S V2.1 从下行读取 AOT
        << params.aot550 << "\n" // AOT at 550nm
        << params.targetElevation << "\n"
        << "-1000\n"
        << "-2\n"
        << wlMin << " " << wlMax << "\n"
        << "0\n"       // INHOMO = 0 (homogeneous surface)
        << "0\n"       // IDIREC = 0 (Lambertian, no directional effects)
        << "0\n"       // igroun = 0 (constant reflectance)
        << "0.2\n"     // ro — surface reflectance for radiative transfer
        << "0\n"       // irapp = 0 → activate atmospheric correction
        << rapp << "\n"// rapp — reference radiance/reflectance for correction
        << "0\n";      // irop = 0 (no polarization)

    return data;
}

QString AtmosphericCorrector::generate6sInputFile(const RadiometricCorrectionParams& params,
                                                    const SensorInfo& sensorInfo,
                                                    const QString& workingDir)
{
    const QString filePath = workingDir + QStringLiteral("/sixs_input.txt");
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qWarning() << "[AtmosphericCorrector] Cannot write 6S input:" << filePath;
        return {};
    }

    QTextStream out(&file);
    for (const auto& band : sensorInfo.bands)
    {
        if (band.wavelengthMin <= 0.0 || band.wavelengthMax <= 0.0)
            continue;
        QByteArray bandInput = generate6sInputData(params, sensorInfo,
                                                    band.wavelengthMin, band.wavelengthMax);
        out << bandInput;
    }
    file.close();
    qDebug() << "[AtmosphericCorrector] 6S input file generated:" << filePath;
    return filePath;
}

// ══════════════════════════════════════════════════════════════════════════════
// Py6S Python 脚本生成
// ══════════════════════════════════════════════════════════════════════════════

QString AtmosphericCorrector::generatePy6sScript(const RadiometricCorrectionParams& params,
                                                   const SensorInfo& sensorInfo,
                                                   const QString& workingDir)
                                                   {
    const QString scriptPath = workingDir + QStringLiteral("/py6s_correction.py");
    QFile file(scriptPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qWarning() << "[AtmosphericCorrector] Cannot write Py6S script:" << scriptPath;
        return {};
    }

    QTextStream out(&file);

    out << "#!/usr/bin/env python3\n"
        << "# Auto-generated Py6S atmospheric correction script\n"
        << "import sys, json, io\n"
        << "try:\n"
        << "    from Py6S import *\n"
        << "except ImportError:\n"
        << "    print(json.dumps({'error': 'Py6S not installed'}))\n"
        << "    sys.exit(1)\n\n"
        << "s = SixS()\n"
        << "s.geometry = Geometry.User()\n"
        << "s.geometry.solar_z = " << sensorInfo.solarZenithAngle << "\n"
        << "s.geometry.solar_a = " << sensorInfo.solarAzimuthAngle << "\n"
        << "s.geometry.view_z = " << sensorInfo.sensorZenithAngle << "\n"
        << "s.geometry.view_a = " << sensorInfo.sensorAzimuthAngle << "\n"
        << "s.geometry.month = " << sensorInfo.acquisitionTime.date().month() << "\n"
        << "s.geometry.day = " << sensorInfo.acquisitionTime.date().day() << "\n\n"
        << "s.atmos_profile = "
            << s_py6sAtmProfile.value(params.atmosphericModel,
                                      QStringLiteral("AtmosProfile.MidlatitudeSummer")) << "\n"
        << "s.aero_profile = "
            << s_py6sAerProfile.value(params.aerosolModel,
                                      QStringLiteral("AeroProfile.Continental")) << "\n"
        << "s.aot550 = " << params.aot550 << "\n"
        << "s.altitudes.set_target_custom_altitude(" << params.targetElevation << ")\n"
        << "s.altitudes.set_sensor_satellite_level()\n\n"
        << "# Atmospheric correction route follows calibration type\n";
    if (params.calibrationType == QStringLiteral("DN2Radiance"))
        out << "s.atmos_corr = AtmosCorr.AtmosCorrLambertianFromRadiance(100.0)\n\n";
    else
        out << "s.atmos_corr = AtmosCorr.AtmosCorrLambertianFromReflectance(0.2)\n\n";
    out
        << "# Wavelength ranges (um) and corresponding band indices\n"
        << "wavelengths = [\n";

    bool first = true;
    for (const auto& band : sensorInfo.bands)
    {
        if (band.wavelengthMin <= 0.0 || band.wavelengthMax <= 0.0)
            continue;
        if (!first) out << ",\n";
        first = false;
        out << "    (" << band.wavelengthMin << ", " << band.wavelengthMax
            << ", " << band.bandNumber << ")";
    }
    out << "\n]\n\n";

    out << "results = []\n"
        << "for wl_min, wl_max, band_num in wavelengths:\n"
        << "    s.wavelength = Wavelength(wl_min, wl_max)\n"
        << "    try:\n"
        << "        _stdout = sys.stdout\n"
        << "        sys.stdout = io.StringIO()\n"
        << "        s.run()\n"
        << "        sys.stdout = _stdout\n"
        << "        results.append({\n"
        << "            'band': band_num,\n"
        << "            'wl_min': wl_min,\n"
        << "            'wl_max': wl_max,\n"
        << "            'xa': s.outputs.coef_xa,\n"
        << "            'xb': s.outputs.coef_xb,\n"
        << "            'xc': s.outputs.coef_xc\n"
        << "        })\n"
        << "    except Exception as e:\n"
        << "        sys.stdout = sys.__stdout__\n"
        << "        results.append({\n"
        << "            'band': band_num,\n"
        << "            'wl_min': wl_min,\n"
        << "            'wl_max': wl_max,\n"
        << "            'xa': None, 'xb': None, 'xc': None,\n"
        << "            'error': str(e)\n"
        << "        })\n\n"
        << "print(json.dumps(results))\n";

    file.close();
    qDebug() << "[AtmosphericCorrector] Py6S script generated:" << scriptPath;
    return scriptPath;
}

// ══════════════════════════════════════════════════════════════════════════════
// 6S 进程调用
// ══════════════════════════════════════════════════════════════════════════════

QString AtmosphericCorrector::run6sProcess(const QByteArray& inputData,
                                           const QString& workingDir)
{
    QString exe = resolveTool(QStringLiteral("sixsV2.1.exe"), QStringLiteral("sixs"));

    QProcess proc;
    proc.setWorkingDirectory(workingDir);
    proc.start(exe, {}, QIODevice::ReadWrite);
    if (!proc.waitForStarted(10000))
    {
        qWarning() << "[AtmosphericCorrector] 6S executable not found:" << exe;
        return {};
    }

    proc.write(inputData);
    proc.closeWriteChannel();

    if (!proc.waitForFinished(120000))
    {
        qWarning() << "[AtmosphericCorrector] 6S timed out";
        proc.kill();
        return {};
    }

    if (proc.exitCode() != 0)
    {
        qWarning() << "[AtmosphericCorrector] 6S exited with code" << proc.exitCode()
                   << ":" << QString::fromUtf8(proc.readAllStandardError());
        return {};
    }

    QString stdoutText = QString::fromUtf8(proc.readAllStandardOutput());
    qDebug() << "[AtmosphericCorrector] 6S completed, output"
             << stdoutText.size() << "chars";
    return stdoutText;
}

QString AtmosphericCorrector::run6sProcess(const QString& inputFilePath,
                                           const QString& workingDir)
{
    QFile inputFile(inputFilePath);
    if (!inputFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qWarning() << "[AtmosphericCorrector] Cannot read 6S input file:" << inputFilePath;
        return {};
    }
    QByteArray inputData = inputFile.readAll();
    inputFile.close();
    return run6sProcess(inputData, workingDir);
}

// ══════════════════════════════════════════════════════════════════════════════
// 6S 输出解析：从标准输出文本中提取 xa/xb/xc（6S V2.1 格式）
//
// 6S V2.1 输出格式（每波段两行）：
//   *       coefficients xa xb xc                 :  0.00262  0.11175  0.16690    *
//   *       coefficients xap xb xc                :  1.37159  0.11176  0.16690    *
//
// isRadianceMode=true  → 提取 xa（路线A: DN→辐亮度）
// isRadianceMode=false → 提取 xap（路线B: DN→反射率）
// ══════════════════════════════════════════════════════════════════════════════

QVector<AtmosphericCoefficients> AtmosphericCorrector::parse6sOutput(
    const QString& stdoutText, const SensorInfo& sensorInfo, bool isRadianceMode)
    {
    QVector<AtmosphericCoefficients> result;

    // 匹配 6S V2.1 格式: coefficients (xa|xap) xb xc : val val val
    static const QRegularExpression coefRx(
        QStringLiteral(R"(coefficients\s+(xa|xap)\s+xb\s+xc\s*:\s*)"
                       R"(([0-9.]+(?:[Ee][+-]?\d+)?)\s+)"
                       R"(([0-9.]+(?:[Ee][+-]?\d+)?)\s+)"
                       R"(([0-9.]+(?:[Ee][+-]?\d+)?))"));

    // 收集所有匹配，然后配对 xa 和 xap（每个波段输出一对）
    struct RawCoef { bool isXa; double a, b, c; };
    QVector<RawCoef> rawCoefs;

    QRegularExpressionMatchIterator it = coefRx.globalMatch(stdoutText);
    while (it.hasNext())
    {
        QRegularExpressionMatch m = it.next();
        RawCoef rc;
        rc.isXa = (m.captured(1) == QStringLiteral("xa"));
        rc.a = m.captured(2).toDouble();
        rc.b = m.captured(3).toDouble();
        rc.c = m.captured(4).toDouble();
        rawCoefs.append(rc);
    }

    if (rawCoefs.isEmpty())
    {
        qWarning() << "[AtmosphericCorrector] Failed to parse 6S output,"
                   << "no coefficient patterns found. First 500 chars:"
                   << stdoutText.left(500);
        return {};
    }

    // 每波段输出一对（xa行 + xap行），按顺序配对
    for (int i = 0; i + 1 < rawCoefs.size(); i += 2)
    {
        const RawCoef& first  = rawCoefs[i];
        const RawCoef& second = rawCoefs[i + 1];
        const RawCoef& src = isRadianceMode
            ? (first.isXa ? first : second)
            : (first.isXa ? second : first);

        AtmosphericCoefficients coef;
        coef.xa = src.a;
        coef.xb = src.b;
        coef.xc = src.c;
        result.append(coef);
    }

    // 奇数个匹配（异常）→ 按顺序直接使用每个匹配
    if (result.isEmpty() && !rawCoefs.isEmpty())
    {
        for (const auto& rc : rawCoefs)
        {
            if (rc.isXa == isRadianceMode)
            {
                AtmosphericCoefficients coef;
                coef.xa = rc.a; coef.xb = rc.b; coef.xc = rc.c;
                result.append(coef);
            }
        }
    }

    if (result.isEmpty())
    {
        qWarning() << "[AtmosphericCorrector] Failed to find matching 6S coefficients"
                   << "(isRadianceMode=" << isRadianceMode << "), raw matches:"
                   << rawCoefs.size();
        return {};
    }

    // 按 sensorInfo.bands 顺序填充 bandIndex 和波长范围
    // 6S 输出顺序与输入卡片中的波段顺序一致
    int bandIdx = 0;
    for (const auto& band : sensorInfo.bands)
    {
        if (band.wavelengthMin <= 0.0 || band.wavelengthMax <= 0.0)
            continue;
        if (bandIdx < result.size())
        {
            result[bandIdx].bandIndex     = band.bandNumber;
            result[bandIdx].wavelengthMin = band.wavelengthMin;
            result[bandIdx].wavelengthMax = band.wavelengthMax;
        }
        ++bandIdx;
    }

    if (bandIdx != result.size())
    {
        qWarning() << "[AtmosphericCorrector] Coefficient count mismatch:"
                   << result.size() << "parsed vs" << bandIdx << "bands";
    }

    qDebug() << "[AtmosphericCorrector] Parsed" << result.size()
             << "6S coefficients";
    return result;
}

// ══════════════════════════════════════════════════════════════════════════════
// Py6S 进程调用
// ══════════════════════════════════════════════════════════════════════════════

QString AtmosphericCorrector::runPy6sProcess(const QString& scriptPath,
                                             const QString& workingDir)
                                             {
    QString python = resolveTool(QStringLiteral("python.exe"),
                                 QStringLiteral("python/venv/Scripts"));
    QString sixsDir = QFileInfo(resolveTool(QStringLiteral("sixsV2.1.exe"),
                                             QStringLiteral("sixs"))).absolutePath();

    QProcess proc;
    proc.setWorkingDirectory(workingDir);

    // Py6S 需要在 PATH 中找到 6S 可执行文件(sixsV1.1.exe)
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString path = env.value(QStringLiteral("PATH"));
    if (!sixsDir.isEmpty())
    {
        env.insert(QStringLiteral("PATH"), sixsDir + QStringLiteral(";") + path);
        qDebug() << "[AtmosphericCorrector] Py6S sixs dir added to PATH:" << sixsDir;
    }
    proc.setProcessEnvironment(env);
    proc.start(python, {scriptPath});

    if (!proc.waitForStarted(10000))
    {
        qWarning() << "[AtmosphericCorrector] Python not found:" << python;
        return {};
    }

    if (!proc.waitForFinished(300000))
    {
        qWarning() << "[AtmosphericCorrector] Py6S timed out";
        proc.kill();
        return {};
    }

    QString stdoutText = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    QString stderrText = QString::fromUtf8(proc.readAllStandardError()).trimmed();

    if (proc.exitCode() != 0)
    {
        qWarning() << "[AtmosphericCorrector] Py6S exited with code"
                   << proc.exitCode() << ":" << stderrText;
        return {};
    }

    if (!stderrText.isEmpty())
        qDebug() << "[AtmosphericCorrector] Py6S stderr:" << stderrText;

    qDebug() << "[AtmosphericCorrector] Py6S completed, output"
             << stdoutText.size() << "chars";
    return stdoutText;
}

// ══════════════════════════════════════════════════════════════════════════════
// Py6S 输出解析：从 JSON 中提取逐波段系数
// ══════════════════════════════════════════════════════════════════════════════

QVector<AtmosphericCoefficients> AtmosphericCorrector::parsePy6sOutput(
    const QString& jsonText)
    {
    QVector<AtmosphericCoefficients> result;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(jsonText.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError)
    {
        qWarning() << "[AtmosphericCorrector] Py6S JSON parse error:" << err.errorString();
        qWarning() << "[AtmosphericCorrector] Raw output:" << jsonText.left(500);
        return {};
    }

    if (doc.isObject() && doc.object().contains(QStringLiteral("error")))
    {
        qWarning() << "[AtmosphericCorrector] Py6S script error:"
                   << doc.object().value(QStringLiteral("error")).toString();
        return {};
    }

    if (!doc.isArray())
    {
        qWarning() << "[AtmosphericCorrector] Expected JSON array, got:"
                   << jsonText.left(200);
        return {};
    }

    const QJsonArray arr = doc.array();
    for (const auto& val : arr)
    {
        if (!val.isObject()) continue;
        QJsonObject obj = val.toObject();

        AtmosphericCoefficients coef;
        coef.bandIndex     = obj.value(QStringLiteral("band")).toInt(0);
        coef.wavelengthMin = obj.value(QStringLiteral("wl_min")).toDouble(0.0);
        coef.wavelengthMax = obj.value(QStringLiteral("wl_max")).toDouble(0.0);

        // 检查该波段是否有错误
        if (obj.contains(QStringLiteral("error")))
        {
            qWarning() << "[AtmosphericCorrector] Py6S band" << coef.bandIndex
                       << "error:" << obj.value(QStringLiteral("error")).toString();
            coef.xa = 1.0;
            coef.xb = 0.0;
            coef.xc = 0.0;
        }
        else
        {
            coef.xa = obj.value(QStringLiteral("xa")).toDouble(1.0);
            coef.xb = obj.value(QStringLiteral("xb")).toDouble(0.0);
            coef.xc = obj.value(QStringLiteral("xc")).toDouble(0.0);
        }

        result.append(coef);
    }

    qDebug() << "[AtmosphericCorrector] Parsed" << result.size()
             << "Py6S coefficients";
    return result;
}

// ══════════════════════════════════════════════════════════════════════════════
// 公共接口：计算大气校正系数
// ══════════════════════════════════════════════════════════════════════════════

QVector<AtmosphericCoefficients> AtmosphericCorrector::computeCorrectionCoefficients(
    const RadiometricCorrectionParams& params,
    const SensorInfo& sensorInfo,
    const QString& workingDir,
    ProgressCallback progress)
    {
    const QString& model = params.atmModel;

    if (model == QStringLiteral("6S"))
    {
        QVector<QPair<double,double>> bandWavelengths;
        for (const auto& band : sensorInfo.bands)
        {
            if (band.wavelengthMin > 0.0 && band.wavelengthMax > 0.0)
                bandWavelengths.append({band.wavelengthMin, band.wavelengthMax});
        }

        if (bandWavelengths.isEmpty())
        {
            qWarning() << "[AtmosphericCorrector] No valid bands for 6S correction";
            return {};
        }

        const int nBands = bandWavelengths.size();
        if (progress) progress(5, QStringLiteral("Running 6S (%1 bands)...").arg(nBands));

        QString combinedOutput;
        for (int i = 0; i < nBands; ++i)
        {
            if (progress)
            {
                int pct = 5 + (i * 80 / nBands);
                if (!progress(pct, QStringLiteral("6S band %1/%2...").arg(i + 1).arg(nBands)))
                {
                    qWarning() << "[AtmosphericCorrector] 6S cancelled at band" << (i + 1);
                    return {};
                }
            }

            QByteArray inputData = generate6sInputData(params, sensorInfo,
                                                        bandWavelengths[i].first,
                                                        bandWavelengths[i].second);
            QString bandOutput = run6sProcess(inputData, workingDir);
            if (bandOutput.isEmpty())
            {
                qWarning() << "[AtmosphericCorrector] 6S failed for band" << (i + 1);
                return {};
            }
            combinedOutput += bandOutput + QStringLiteral("\n");
        }

        if (progress) progress(90, QStringLiteral("Parsing 6S output..."));
        const bool isRadiance = (params.calibrationType == QStringLiteral("DN2Radiance"));
        return parse6sOutput(combinedOutput, sensorInfo, isRadiance);
    }

    if (model == QStringLiteral("Py6S"))
    {
        if (progress) progress(10, QStringLiteral("Generating Py6S script..."));
        QString scriptPath = generatePy6sScript(params, sensorInfo, workingDir);
        if (scriptPath.isEmpty())
            return {};

        if (progress) progress(30, QStringLiteral("Running Py6S..."));
        QString stdoutText = runPy6sProcess(scriptPath, workingDir);
        if (stdoutText.isEmpty())
            return {};

        if (progress) progress(60, QStringLiteral("Parsing Py6S output..."));
        return parsePy6sOutput(stdoutText);
    }

    qWarning() << "[AtmosphericCorrector] computeCorrectionCoefficients"
               << "not applicable for model:" << model;
    return {};
}

// ══════════════════════════════════════════════════════════════════════════════
// 公共接口：逐像素大气校正
// ══════════════════════════════════════════════════════════════════════════════

AlgorithmResult AtmosphericCorrector::applyCorrection(
    IRasterReader* reader,
    IRasterWriter* writer,
    int bandIndex,
    const AtmosphericCoefficients& coef,
    ProgressCallback progress)
    {
    AlgorithmResult result;

    if (!reader || !writer)
    {
        result.errorMessage = QStringLiteral("IRasterReader or IRasterWriter is null");
        return result;
    }

    if (bandIndex < 1 || bandIndex > reader->bandCount())
    {
        result.errorMessage = QStringLiteral("Band index %1 out of range").arg(bandIndex);
        return result;
    }

    const QSize rasterSize = reader->rasterSize();
    const int w = rasterSize.width();
    const int h = rasterSize.height();
    if (w <= 0 || h <= 0)
    {
        result.errorMessage = QStringLiteral("Invalid raster size");
        return result;
    }

    const double ndv = reader->noDataValue();

    // 分块处理
    const int blockSize = 512;
    const int xBlocks = (w + blockSize - 1) / blockSize;
    const int yBlocks = (h + blockSize - 1) / blockSize;
    const int totalBlocks = xBlocks * yBlocks;

    const double xa = coef.xa;
    const double xb = coef.xb;
    const double xc = coef.xc;

    // ═══════════════════════════════════════════════════════════════════════════
    // Pass 1: 统计全局最小地表反射率（用于暗像元减法 DOS）
    // ═══════════════════════════════════════════════════════════════════════════
    double globalMin = 0.0;
    int pass1Blocks = 0;

    for (int y = 0; y < h; y += blockSize)
    {
        for (int x = 0; x < w; x += blockSize)
        {
            int bw = std::min(blockSize, w - x);
            int bh = std::min(blockSize, h - y);

            QVector<float> data = reader->readBandWindow(bandIndex, x, y, bw, bh);
            if (data.isEmpty())
            {
                result.errorMessage = QStringLiteral(
                    "Failed to read band %1 at (%2,%3)").arg(bandIndex).arg(x).arg(y);
                return result;
            }

            for (int i = 0; i < data.size(); ++i)
            {
                float pixel = data[i];
                if (pixel == ndv || pixel < 0.0f)
                    continue;

                double rho_toa = static_cast<double>(pixel);
                double yy = xa * rho_toa - xb;
                double denom = 1.0 + xc * yy;
                if (denom < 0.0001) denom = 0.0001;
                double rho_surf = yy / denom;

                if (rho_surf < globalMin) globalMin = rho_surf;
            }

            ++pass1Blocks;
        }
    }

    // 计算 DOS 偏移量：将全局最小值平移到 0
    const double dosOffset = (globalMin < 0.0) ? (-globalMin) : 0.0;

    // ═══════════════════════════════════════════════════════════════════════════
    // Pass 2: 逐块应用大气校正 + DOS 平移，写入输出
    // ═══════════════════════════════════════════════════════════════════════════
    int blockIdx = 0;

    for (int y = 0; y < h; y += blockSize)
    {
        for (int x = 0; x < w; x += blockSize)
        {
            int bw = std::min(blockSize, w - x);
            int bh = std::min(blockSize, h - y);

            QVector<float> data = reader->readBandWindow(bandIndex, x, y, bw, bh);
            if (data.isEmpty())
            {
                result.errorMessage = QStringLiteral(
                    "Failed to read band %1 at (%2,%3)").arg(bandIndex).arg(x).arg(y);
                return result;
            }

            // 大气校正（Lambertian）：y = xa*ρ_toa - xb;  ρ_surf = y / (1 + xc*y) + dosOffset
            for (int i = 0; i < data.size(); ++i)
            {
                float pixel = data[i];
                if (pixel == ndv || pixel < 0.0f)
                    continue;

                double rho_toa = static_cast<double>(pixel);
                double yy = xa * rho_toa - xb;

                double denom = 1.0 + xc * yy;
                if (denom < 0.0001) denom = 0.0001;

                double rho_surf = yy / denom + dosOffset;
                if (rho_surf > 2.0) rho_surf = 2.0;

                data[i] = static_cast<float>(rho_surf);
            }

            if (!writer->writeBandWindow(bandIndex, x, y, bw, bh, data))
            {
                result.errorMessage = QStringLiteral(
                    "Failed to write band %1 at (%2,%3)").arg(bandIndex).arg(x).arg(y);
                return result;
            }

            ++blockIdx;
            if (progress)
            {
                int pct = blockIdx * 100 / totalBlocks;
                if (!progress(pct, QStringLiteral("Atmospheric correction band %1").arg(bandIndex)))
                {
                    result.errorMessage = QStringLiteral("Cancelled by user");
                    return result;
                }
            }
        }
    }

    result.success = true;
    return result;
}

// ══════════════════════════════════════════════════════════════════════════════
// 公共接口：Sen2Cor 大气校正
// ══════════════════════════════════════════════════════════════════════════════

AlgorithmResult AtmosphericCorrector::runSen2Cor(
    const QString& inputPath,
    const RadiometricCorrectionParams& params,
    const QString& workingDir,
    ProgressCallback progress)
    {
    AlgorithmResult result;

    if (inputPath.isEmpty())
    {
        result.errorMessage = QStringLiteral("Sen2Cor requires a Sentinel-2 L1C product path");
        return result;
    }

    // 从输入路径定位 .SAFE 根目录
    QString safeRoot = inputPath;
    int safeIdx = safeRoot.indexOf(QStringLiteral(".SAFE"), 0, Qt::CaseInsensitive);
    if (safeIdx >= 0)
    {
        safeRoot = safeRoot.left(safeIdx + 5); // include ".SAFE"
    }
    else if (inputPath.endsWith(".zip", Qt::CaseInsensitive))
    {
        // Sen2Cor 需要解压后的 .SAFE 目录，直接解压 ZIP（其内部顶层就是 .SAFE 目录）
        QFileInfo zipFi(inputPath);
        QString safeName = zipFi.completeBaseName(); // stem without .zip
        QString extractDir = workingDir + QStringLiteral("/") + safeName + QStringLiteral(".SAFE");

        if (!QFileInfo::exists(extractDir))
        {
            if (progress)
                progress(6, QStringLiteral("解压 ZIP 中..."));
            qDebug() << "[AtmosphericCorrector] Extracting ZIP for Sen2Cor:" << inputPath;
            QProcess unzip;
            unzip.setWorkingDirectory(workingDir);
#ifdef Q_OS_WIN
            // 直接解压到 workingDir，ZIP 内顶层目录就是 .SAFE，不需要额外嵌套
            unzip.start(QStringLiteral("powershell"), {
                QStringLiteral("-NoProfile"), QStringLiteral("-Command"),
                QStringLiteral("Expand-Archive -Path '%1' -DestinationPath '%2' -Force")
                    .arg(QDir::toNativeSeparators(inputPath),
                         QDir::toNativeSeparators(workingDir))
            });
#else
            unzip.start(QStringLiteral("unzip"), {inputPath, QStringLiteral("-d"), workingDir});
#endif
            if (!unzip.waitForFinished(300000)) // 5 min timeout for extraction
            {
                result.errorMessage = QStringLiteral("ZIP extraction timed out for Sen2Cor");
                return result;
            }
            if (unzip.exitCode() != 0 || !QFileInfo::exists(extractDir))
            {
                result.errorMessage = QStringLiteral("Failed to extract ZIP for Sen2Cor. "
                    "Please manually extract the .SAFE directory.");
                return result;
            }
        }
        safeRoot = extractDir;
    }
    else
    {
        // 尝试把输入路径的父目录作为 SAFE 根
        QFileInfo fi(inputPath);
        if (fi.isDir() && fi.suffix().compare(QStringLiteral("SAFE"), Qt::CaseInsensitive) == 0)
            safeRoot = fi.absoluteFilePath();
        else
            qWarning() << "[AtmosphericCorrector] Could not locate .SAFE root from:" << inputPath;
    }

    if (progress)
        progress(10, QStringLiteral("Sen2Cor 准备中..."));
    qDebug() << "[AtmosphericCorrector] Sen2Cor SAFE root:" << safeRoot;

    QString program = resolveTool(QStringLiteral("L2A_Process.bat"), QStringLiteral("sen2cor"));
    qDebug() << "[AtmosphericCorrector] Sen2Cor program:" << program;

    if (progress)
        progress(15, QStringLiteral("启动 Sen2Cor: ") + program);
    // Sen2Cor L2A_Process.bat 只接受 SAFE 目录路径，不传其他参数
    QStringList args;
    args << QDir::toNativeSeparators(safeRoot);

    QProcess proc;
    proc.setWorkingDirectory(workingDir);
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start(program, args);

    if (!proc.waitForStarted(15000))
    {
        result.errorMessage = QStringLiteral(
            "Sen2Cor (L2A_Process) not found. "
            "Download from ESA and place in external/sen2cor/ or add to PATH.");
        return result;
    }

    if (progress)
        progress(20, QStringLiteral("Sen2Cor 运行中（需 5-15 分钟）..."));

    // Sen2Cor 处理可能需要较长时间（数分钟至十几分钟）
    if (!proc.waitForFinished(900000)) // 15 min timeout
    {
        qWarning() << "[AtmosphericCorrector] Sen2Cor timed out";
        proc.kill();
        result.errorMessage = QStringLiteral("Sen2Cor timed out after 15 minutes");
        return result;
    }

    if (proc.exitCode() != 0)
    {
        QString output = QString::fromUtf8(proc.readAll());
        result.errorMessage = QStringLiteral("Sen2Cor exited with code %1:\n%2")
            .arg(proc.exitCode())
            .arg(output);
        return result;
    }

    // 查找 Sen2Cor 输出的 L2A 目录
    // 命名规则：将 L1C 替换为 L2A
    QString l2aPath = safeRoot;
    l2aPath.replace(QStringLiteral("_L1C_"), QStringLiteral("_L2A_"));
    l2aPath.replace(QStringLiteral("_MSIL1C_"), QStringLiteral("_MSIL2A_"));
    l2aPath.replace(QStringLiteral("_OPER_"), QStringLiteral("_USER_"));

    // 验证 L2A 产物是否存在
    if (!QFileInfo::exists(l2aPath))
    {
        // 在 safeRoot 的父目录下搜索 L2A
        QFileInfo srFi(safeRoot);
        QDir parentDir = srFi.absoluteDir();
        QStringList l2aDirs = parentDir.entryList({QStringLiteral("*MSIL2A*")}, QDir::Dirs);
        if (!l2aDirs.isEmpty())
            l2aPath = parentDir.absoluteFilePath(l2aDirs.first());
    }
    if (!QFileInfo::exists(l2aPath))
    {
        // 也在 workingDir 下搜索
        QDir wd(workingDir);
        QStringList l2aDirs = wd.entryList({QStringLiteral("*MSIL2A*")}, QDir::Dirs);
        if (!l2aDirs.isEmpty())
            l2aPath = wd.absoluteFilePath(l2aDirs.first());
    }

    if (!QFileInfo::exists(l2aPath))
    {
        result.errorMessage = QStringLiteral(
            "Sen2Cor finished but L2A product not found.\n"
            "Expected: %1\n"
            "Check if Sen2Cor created output elsewhere.").arg(l2aPath);
        return result;
    }

    result.success = true;
    result.outputPath = l2aPath;
    qDebug() << "[AtmosphericCorrector] Sen2Cor completed, L2A:" << l2aPath;
    return result;
}

} // namespace Radiometric
