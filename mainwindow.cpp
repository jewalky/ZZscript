#include <QTabWidget>
#include <QBoxLayout>
#include <QSplitter>

#include <QTreeWidget>
#include <QTreeWidgetItem>

#include <QDir>

#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow* MainWindow::ptr = nullptr;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    assert(ptr == nullptr);
    ptr = this;
    ui->setupUi(this);

    project = nullptr;

    //createDocument();
    connect(ui->currentProjectTree, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)), this, SLOT(projectTreeDoubleClicked(QTreeWidgetItem*,int)));
    loadProject("../ZZscript/Reference"); // todo: unhardcode this
}

MainWindow::~MainWindow()
{
    assert (ptr == this);
    ptr = nullptr;
    delete ui;
}

MainWindow* MainWindow::get()
{
    return ptr;
}

void MainWindow::makeTabForDocument(Document* doc)
{
    ui->editorTabs->setUpdatesEnabled(false);
    DocumentTab* tab = new DocumentTab(nullptr, doc);
    doc->setTab(tab);
    ui->editorTabs->addTab(tab, doc->location);

    ui->editorTabs->setUpdatesEnabled(true);
}

Document* MainWindow::createDocument()
{
    static int untitledIndex = 0;
    untitledIndex++;
    Document* doc = new Document();
    doc->isnew = true;
    doc->location = QString("Untitled %1").arg(untitledIndex);
    documents.append(doc);
    makeTabForDocument(doc);
    // todo: template
    return doc;
}

Document* MainWindow::loadDocument(QString filename)
{
    // todo: load from file
    return nullptr;
}

Document* MainWindow::getDocumentForTab(DocumentTab* tab)
{
    return tab->document();
}

void MainWindow::unloadDocument(Document* doc)
{
    for (int i = 0; i < ui->editorTabs->count(); i++)
    {
        DocumentTab* tab = qobject_cast<DocumentTab*>(ui->editorTabs->widget(i));
        assert(tab != nullptr);
        if (tab->document() == doc)
        {
            ui->editorTabs->removeTab(i);
            i--;
        }
    }

    delete doc;
}

static QTreeWidgetItem* makeItem(QTreeWidget* w, QString path)
{
    QStringList p = path.split('/');
    QTreeWidgetItem* parent = nullptr;
    for (int i = 0; i < p.size(); i++)
    {
        if (!i)
        {
            QList<QTreeWidgetItem*> items = w->findItems(p[i], Qt::MatchExactly);
            if (items.size() > 1)
                return nullptr; // wtf?
            if (!items.size())
            {
                if (parent)
                    parent = new QTreeWidgetItem(parent);
                else parent = new QTreeWidgetItem(w);
                parent->setText(0, p[i]);
            }
            else parent = items[0];
        }
        else if (parent)
        {
            QTreeWidgetItem* child = nullptr;
            for (int j = 0; j < parent->childCount(); j++)
            {
                if (!parent->child(j)->text(0).compare(p[i], Qt::CaseInsensitive))
                {
                    child = parent->child(j);
                    break;
                }
            }
            if (!child)
            {
                child = new QTreeWidgetItem(parent);
                child->setText(0, p[i]);
            }
            parent = child;
        }
        else return nullptr; // wtf?
    }

    return parent;
}

void MainWindow::reloadTreeFromProject()
{
    ui->currentProjectTree->clear();
    if (!project) return;

    QTreeWidgetItem* root = makeItem(ui->currentProjectTree, project->projectName);
    for (const QString& dir : project->directories)
    {
        QTreeWidgetItem* dirItem = makeItem(ui->currentProjectTree, dir);
        // todo mark directory
    }
    for (const ProjectFile& pf : project->files)
    {
        QTreeWidgetItem* fileItem = makeItem(ui->currentProjectTree, pf.relativePath);
        // todo mark file type
        fileItem->setData(0, Qt::UserRole, pf.fullPath);
    }
    // expand all
    ui->currentProjectTree->expandAll();
}

void MainWindow::loadProject(QString path)
{
    if (project) delete project;
    project = new Project(path);
    reloadTreeFromProject();
    project->parseProject();
}

Project* MainWindow::getProject()
{
    return project;
}

void MainWindow::projectTreeDoubleClicked(QTreeWidgetItem* item, int column)
{
    QString filePath = item->data(column, Qt::UserRole).toString();
    if (filePath.isEmpty() || filePath.isNull())
        return; // do nothing
    // check if already open
    for (int i = 0; i < ui->editorTabs->count(); i++)
    {
        DocumentTab* tab = qobject_cast<DocumentTab*>(ui->editorTabs->widget(i));
        if (!tab) continue;
        if (tab->document()->fullPath == filePath)
        {
            ui->editorTabs->setCurrentIndex(i);
            return;
        }
    }

    // get project file
    ProjectFile* pf = nullptr;
    for (ProjectFile& spf : project->files)
    {
        if (spf.fullPath == filePath)
        {
            pf = &spf;
            break;
        }
    }

    Document* doc = createDocument();
    doc->isnew = false;
    QStringList pathSep = filePath.split('/');
    doc->location = pathSep.last();
    doc->fullPath = filePath;
    doc->syncFromSource(pf);
    // now, if there is a project, this means we can pull parsed data from there
    DocumentTab* tab = doc->getTab();
    ui->editorTabs->setTabText(ui->editorTabs->indexOf(tab), doc->location);
    ui->editorTabs->setCurrentWidget(tab);
}

void MainWindow::on_actionQuit_triggered()
{
    // todo check for unsaved files
    close();
}

void MainWindow::on_actionSave_File_triggered()
{
    // get current tab and store to filesystem
    DocumentTab* tab = qobject_cast<DocumentTab*>(ui->editorTabs->currentWidget());
    if (!tab) return;
    tab->document()->save();
}
