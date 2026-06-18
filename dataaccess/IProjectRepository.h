#ifndef IPROJECTREPOSITORY_H
#define IPROJECTREPOSITORY_H

#include "domain/Project.h"
#include <QString>

class IProjectRepository
{
public:
    virtual ~IProjectRepository() = default;
    virtual bool save(const Project& project, const QString& filePath) = 0;
    virtual Project load(const QString& filePath) = 0;
};

#endif // IPROJECTREPOSITORY_H
