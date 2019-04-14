#include <QTabWidget>
#include <QBoxLayout>

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

    createDocument();
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
    DocumentTab* tab = new DocumentTab();
    tab->setDocument(doc);
    ui->editorTabs->addTab(tab, doc->location);

    // create text document
    DocumentEditor* textDoc = new DocumentEditor(tab);
    QLayout* tabLayout = new QBoxLayout(QBoxLayout::TopToBottom);
    tab->setLayout(tabLayout);
    tabLayout->addWidget(textDoc);
    tabLayout->setMargin(2);

    textDoc->textChanged();

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
