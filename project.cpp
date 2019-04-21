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

bool Project::parseProject()
{
    bool allok = true;
    // find zscript.txt
    QList<ProjectFile*> zsFiles;
    QStringList includeTree;
    QStringList allIncludes;
    bool rootfound = false;
    for (ProjectFile& f : files)
    {
        if (f.relativePath == projectName + "/zscript.txt")
        {
            zsFiles.append(&f);
            f.fileType = ProjectFile::ZScript;
            // parse file
            bool thisok = f.parse();
            allok &= thisok;
            if (!thisok)
                qDebug("parseProject: %s/zscript.txt failed", projectName.toUtf8().data());
            rootfound = true;
            allIncludes.append(f.relativePath);
            // find and add includes
            if (f.parser && f.parser->root)
            {
                for (QSharedPointer<ZTreeNode> node : f.parser->root->children)
                {
                    if (node->type() == ZTreeNode::Include)
                    {
                        QSharedPointer<ZInclude> inc = node.dynamicCast<ZInclude>();
                        includeTree.append(inc->location);
                    }
                }
            }
            break;
        }
    }

    if (!rootfound)
    {
        qDebug("parseProject: %s/zscript.txt not found", projectName.toUtf8().data());
        return false;
    }

    if (includeTree.size())
    {
        do
        {
            bool incfound = false;
            QString currentInclude = includeTree.first();
            if (allIncludes.contains(projectName + "/" + currentInclude))
            {
                qDebug("parseProject: circular include %s/%s", projectName.toUtf8().data(), currentInclude.toUtf8().data());
                includeTree.removeFirst();
                continue;
            }
            for (ProjectFile& f : files)
            {
                if (f.relativePath == projectName + "/" + currentInclude)
                {
                    zsFiles.append(&f);
                    f.fileType = ProjectFile::ZScript;
                    // parse file
                    bool thisok = f.parse();
                    allok &= thisok;
                    if (!thisok)
                        qDebug("parseProject: %s/%s failed", projectName.toUtf8().data(), currentInclude.toUtf8().data());
                    includeTree.removeFirst();
                    incfound = true;
                    allIncludes.append(f.relativePath);
                    // find and add includes
                    if (f.parser && f.parser->root)
                    {
                        for (QSharedPointer<ZTreeNode> node : f.parser->root->children)
                        {
                            if (node->type() == ZTreeNode::Include)
                            {
                                QSharedPointer<ZInclude> inc = node.dynamicCast<ZInclude>();
                                includeTree.append(inc->location);
                            }
                        }
                    }
                    break;
                }
            }

            if (!incfound)
            {
                qDebug("parseProject: %s/%s not found", projectName.toUtf8().data(), currentInclude.toUtf8().data());
                allok = false;
            }
        }
        while (includeTree.size());
    }

    allok &= parseProjectClasses(); // this can be separate from parseProject
    return allok;
}

bool Project::parseProjectClasses()
{
    QList<QSharedPointer<ZTreeNode>> allTypes;
    for (ProjectFile& f : files)
    {
        if (!f.parser) continue;
        QList<QSharedPointer<ZTreeNode>> localTypes = f.parser->getOwnTypeInformation();
        allTypes.append(localTypes);
    }

    bool allok = true;
    for (ProjectFile& f : files)
    {
        if (!f.parser) continue;
        f.parser->setTypeInformation(allTypes);

        for (QSharedPointer<ZTreeNode> node : f.parser->root->children)
        {
            if (node->type() == ZTreeNode::Class)
            {
                allok &= f.parser->parseClassFields(node.dynamicCast<ZClass>());
            }
            else if (node->type() == ZTreeNode::Struct)
            {
                allok &= f.parser->parseStructFields(node.dynamicCast<ZStruct>());
            }
        }

        // later this also needs to be done outside of the parser after all fields are processed
        for (QSharedPointer<ZTreeNode> node : f.parser->root->children)
        {
            if (node->type() == ZTreeNode::Class)
            {
                allok &= f.parser->parseClassMethods(node.dynamicCast<ZClass>());
            }
            else if (node->type() == ZTreeNode::Struct)
            {
                allok &= f.parser->parseStructMethods(node.dynamicCast<ZStruct>());
            }
        }
    }

    return allok;
}

bool ProjectFile::parse()
{
    if (parser) delete parser;
    parser = nullptr;

    // read file
    QFileInfo fi(fullPath);
    if (!fi.isFile())
        return false;

    //
    QFile f(fullPath);
    if (f.open(QIODevice::ReadOnly|QIODevice::Text))
    {
        contents = f.readAll();

        Tokenizer t(contents);
        QList<Tokenizer::Token> tokens = t.readAllTokens();
        parser = new Parser(tokens);

        f.close();
        bool okparsed = parser->parse();
        if (parser->root)
        {
            parser->root->fullPath = fullPath;
            parser->root->relativePath = relativePath;
        }

        return okparsed;
    }

    return false; // failed to open
}
