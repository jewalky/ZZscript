#ifndef PROJECT_H
#define PROJECT_H

#include <QString>
#include <QList>
#include "parser.h"

struct ProjectFile
{
    QString name;
    QString relativePath;
    QString fullPath;

    enum ProjectFileType
    {
        Unknown,
        GenericText,
        ZScript,
        DECORATE
    };

    ProjectFileType fileType;

    Parser* parser;

    ProjectFile()
    {
        parser = nullptr;
    }

    ~ProjectFile()
    {
        if (parser) delete parser;
        parser = nullptr;
    }
};

class Project
{
public:
    Project(QString basePath);
    QList<ProjectFile> files;
    QList<QString> directories;
    QString projectName;

    static QString fixPath(QString path);

    bool parseProject();

private:
    void readDir(QString basePath, QString relativeBasePath);
};

#endif // PROJECT_H
