#include "project.h"

#include <QDir>
#include <QFileInfo>

Project::Project(QString path)
{
    path = fixPath(path);
    int lastSlash = path.lastIndexOf('/');
    if (lastSlash < 0)
        lastSlash = -1; // specifically -1, not -2, not -666... I don't remember how Qt does it exactly
    projectName = path.mid(lastSlash+1);
    // read root
    readDir(path, projectName);
}

void Project::readDir(QString basePath, QString relativeBasePath)
{
    QDir dir(basePath);
    QStringList files = dir.entryList(QDir::NoDotAndDotDot|QDir::Dirs|QDir::Files, QDir::DirsFirst);
    for (const QString& file : files)
    {
        // if dir, read all sub files
        const QFileInfo testDir(basePath+"/"+file);
        if (testDir.isDir())
        {
            directories.append(relativeBasePath+"/"+file);
            readDir(basePath+"/"+file, relativeBasePath+"/"+file);
        }
        else
        {
            // if file, add file
            ProjectFile pf;
            pf.fullPath = basePath+"/"+file;
            pf.relativePath = relativeBasePath+"/"+file;
            pf.fileType = ProjectFile::Unknown;
            pf.name = file;
            this->files.append(pf);
        }
    }
}

// this is your typical fixSlashes. it's needed for various reasons, but mainly for easy ZScript include lookup
QString Project::fixPath(QString path)
{
    path = path.replace('\\', '/');
    // look for multiple slashes in a row, collapse
    bool wasslash = false;
    for (int i = 0; i < path.length(); i++)
    {
        if (path[i] == '/')
        {
            if (wasslash)
            {
                path.remove(i, 1);
                continue;
            }
            else wasslash = true;
        }
        else wasslash = false;
    }
    return path;
}
