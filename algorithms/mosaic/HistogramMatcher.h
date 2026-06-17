#ifndef HISTOGRAMMATCHER_H
#define HISTOGRAMMATCHER_H

#include <QString>

class HistogramMatcher
{
public:
    /// 将 targetPath 的直方图匹配到 refPath，输出到 outputPath
    /// 逐波段匹配，使用 256 级查找表
    static bool match(const QString& refPath,
                      const QString& targetPath,
                      const QString& outputPath);
};

#endif // HISTOGRAMMATCHER_H
