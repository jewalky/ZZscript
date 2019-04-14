#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QList>
#include "document.h"

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

private:
    Ui::MainWindow *ui;
    static MainWindow *ptr;

    QList<Document*> documents;
    void makeTabForDocument(Document* doc);
};

#endif // MAINWINDOW_H
