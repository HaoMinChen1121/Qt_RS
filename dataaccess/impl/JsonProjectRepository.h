#ifndef JSONPROJECTREPOSITORY_H
#define JSONPROJECTREPOSITORY_H

#include "dataaccess/IProjectRepository.h"

class JsonProjectRepository : public IProjectRepository
{
public:
    bool save(const Project& project, const QString& filePath) override;
    Project load(const QString& filePath) override;
};

#endif // JSONPROJECTREPOSITORY_H
