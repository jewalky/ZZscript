#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QList>
#include <QTreeWidgetItem>
#include "document.h"
#include "project.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    static MainWindow* get();

    Document* createDocument();
    Document* loadDocument(QString filename);
    Document* getDocumentForTab(DocumentTab* tab);
    void unloadDocument(Document* doc);

    Project* getProject();
    void loadProject(QString path);

public slots:
    void projectTreeDoubleClicked(QTreeWidgetItem* item, int column);

private slots:
    void on_actionQuit_triggered();

    void on_actionSave_File_triggered();

private:
    Ui::MainWindow *ui;
    static MainWindow *ptr;

    QList<Document*> documents;

    //
    void makeTabForDocument(Document* doc);
    void reloadTreeFromProject();

    //
    Project* project;
};

#endif // MAINWINDOW_H
