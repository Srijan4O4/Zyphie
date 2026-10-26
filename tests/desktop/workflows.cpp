#include "window.hpp"
#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QTreeWidget>
#include <QComboBox>
#include <QTimer>
#include <QMimeData>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QUrl>
#include <QProcess>
#include <QMessageBox>
#include <QRandomGenerator>
class Workflows : public QObject {
    Q_OBJECT
private slots:
    void explicitExtractionScope() {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const auto source=dir.path()+"/Machine_Learning";
        QVERIFY(QDir().mkpath(source+"/Theory/empty"));
        QVERIFY(QDir().mkpath(source+"/Python"));
        const QList<QPair<QString,QByteArray>> files={
            {"Theory/notes.txt","Theory notes\n"},
            {"Python/train.py","print('training')\n"},
            {"require.txt","dependencies\n"}, {"empty.txt",{}}};
        for(const auto& [name,bytes]:files){QFile file(source+"/"+name);QVERIFY(file.open(QIODevice::WriteOnly));QCOMPARE(file.write(bytes),qint64(bytes.size()));}
        const auto archive=dir.path()+"/learning.zyph";
        QProcess cli;cli.start(ZYPH_CLI,{"pack",source,"-o",archive});QVERIFY(cli.waitForFinished());QCOMPARE(cli.exitCode(),0);
        Window w;w.show();
        auto* all=w.findChild<QPushButton*>("extract");
        auto* selected=w.findChild<QPushButton*>("extractSelected");
        QVERIFY(all);QVERIFY(selected);
        QVERIFY(!selected->isEnabled());
        auto* tree=w.findChild<QTreeWidget*>("entries");
        auto* destination=w.findChild<QLineEdit*>("destination");
        auto* status=w.findChild<QLabel*>("status");
        w.findChild<QLineEdit*>("archive")->setText(archive);
        QTest::mouseClick(w.findChild<QPushButton*>("open"),Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
        QCOMPARE(tree->topLevelItemCount(),1);
        const auto theory=tree->findItems("Theory",Qt::MatchExactly|Qt::MatchRecursive);
        QCOMPARE(theory.size(),1);
        theory.front()->setSelected(true);QVERIFY(selected->isEnabled());
        // Merely browsing/highlighting a folder must not limit Extract all.
        destination->setText(dir.path()+"/all");QTest::mouseClick(all,Qt::LeftButton);
        QVERIFY(w.busy());QVERIFY(!selected->isEnabled());
        QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
        QVERIFY2(status->text().contains("all files restored"),qPrintable(status->text()));
        for(const auto& [name,bytes]:files){QFile file(dir.path()+"/all/Machine_Learning/"+name);QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),bytes);}
        QVERIFY(QDir(dir.path()+"/all/Machine_Learning/Theory/empty").exists());
        QVERIFY(selected->isEnabled());
        // Only an explicit selected action should omit sibling files/folders.
        destination->setText(dir.path()+"/subset");QTest::mouseClick(selected,Qt::LeftButton);
        QVERIFY(w.busy());QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
        QVERIFY(status->text().contains("selected files restored"));
        QVERIFY(status->text().contains("Unselected payloads were not verified"));
        QFile notes(dir.path()+"/subset/Machine_Learning/Theory/notes.txt");
        QVERIFY(notes.open(QIODevice::ReadOnly));QCOMPARE(notes.readAll(),files.front().second);
        QVERIFY(QDir(dir.path()+"/subset/Machine_Learning/Theory/empty").exists());
        QVERIFY(!QFile::exists(dir.path()+"/subset/Machine_Learning/require.txt"));
        QVERIFY(!QDir(dir.path()+"/subset/Machine_Learning/Python").exists());
        const auto requirements=tree->findItems("require.txt",Qt::MatchExactly|Qt::MatchRecursive);
        QCOMPARE(requirements.size(),1);requirements.front()->setSelected(true);
        QVERIFY(selected->text().contains("(2)"));
        destination->setText(dir.path()+"/multi");QTest::mouseClick(selected,Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
        QVERIFY(status->text().contains("selected files restored"));
        QFile dependencyFile(dir.path()+"/multi/Machine_Learning/require.txt");
        QVERIFY(dependencyFile.open(QIODevice::ReadOnly));QCOMPARE(dependencyFile.readAll(),files[2].second);
        QVERIFY(QFile::exists(dir.path()+"/multi/Machine_Learning/Theory/notes.txt"));
        QVERIFY(!QDir(dir.path()+"/multi/Machine_Learning/Python").exists());
        tree->clearSelection();QVERIFY(!selected->isEnabled());
        destination->setText(dir.path()+"/no-selection");QTest::mouseClick(selected,Qt::LeftButton);
        QVERIFY(!w.busy());QVERIFY(!QDir(destination->text()).exists());
        // Defensive dispatch must also refuse an empty selected scope.
        QVERIFY(QMetaObject::invokeMethod(selected,"clicked",Qt::DirectConnection,Q_ARG(bool,false)));
        QVERIFY(!w.busy());QVERIFY(!QDir(destination->text()).exists());
        QVERIFY(status->text().contains("Select files or folders"));
        QTest::mouseClick(all,Qt::LeftButton);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
        QVERIFY(status->text().contains("all files restored"));
        for(const auto& [name,bytes]:files){QFile file(dir.path()+"/no-selection/Machine_Learning/"+name);QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),bytes);}
        theory.front()->setSelected(true);QVERIFY(selected->isEnabled());
        w.findChild<QLineEdit*>("archive")->setText(dir.path()+"/other.zyph");
        QCOMPARE(tree->topLevelItemCount(),0);QVERIFY(!selected->isEnabled());
    }
    void createBrowseVerifyExtract() {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const auto input=dir.path()+"/hello.txt", archive=dir.path()+"/sample.zyph";
        QFile file(input);QVERIFY(file.open(QIODevice::WriteOnly));file.write("Byte exact data\n");file.close();
        Window w;w.show();w.addSources({input});
        w.findChild<QLineEdit*>("archive")->setText(archive);
        QTest::mouseClick(w.findChild<QPushButton*>("pack"),Qt::LeftButton);
        QVERIFY(w.busy());QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),20000);
        QVERIFY2(w.findChild<QLabel*>("status")->text().startsWith("Success"),qPrintable(w.findChild<QLabel*>("status")->text()));
        QProcess cli;cli.start(ZYPH_CLI,{"verify",archive});QVERIFY(cli.waitForFinished());QCOMPARE(cli.exitCode(),0);
        QTest::mouseClick(w.findChild<QPushButton*>("open"),Qt::LeftButton);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
        auto* tree=w.findChild<QTreeWidget*>("entries");QCOMPARE(tree->topLevelItemCount(),1);
        QCOMPARE(tree->topLevelItem(0)->text(0),QString("hello.txt"));
        QTest::mouseClick(w.findChild<QPushButton*>("verify"),Qt::LeftButton);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
        QVERIFY(w.findChild<QLabel*>("status")->text().startsWith("Success"));
        tree->topLevelItem(0)->setSelected(true);
        w.findChild<QLineEdit*>("destination")->setText(dir.path()+"/restored");
        QTest::mouseClick(w.findChild<QPushButton*>("extract"),Qt::LeftButton);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
        QFile restored(dir.path()+"/restored/hello.txt");QVERIFY(restored.open(QIODevice::ReadOnly));QCOMPARE(restored.readAll(),QByteArray("Byte exact data\n"));
        w.findChild<QComboBox*>("theme")->setCurrentIndex(1);
        QVERIFY(w.grab().save("desktop-dark.png"));
        w.findChild<QComboBox*>("theme")->setCurrentIndex(0);QVERIFY(w.grab().save("desktop-light.png"));
        cli.start(ZYPH_CLI,{"pack",input,"-o",dir.path()+"/cli.zyph"});QVERIFY(cli.waitForFinished());QCOMPARE(cli.exitCode(),0);
        w.findChild<QLineEdit*>("archive")->setText(dir.path()+"/cli.zyph");
        QTest::mouseClick(w.findChild<QPushButton*>("verify"),Qt::LeftButton);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
        QVERIFY(w.findChild<QLabel*>("status")->text().startsWith("Success"));
        QTimer::singleShot(0,[]{for(auto* widget:QApplication::topLevelWidgets())if(auto* box=qobject_cast<QMessageBox*>(widget))box->accept();});
        QTest::mouseClick(w.findChild<QPushButton*>("extract"),Qt::LeftButton);
        QVERIFY(!w.busy());QVERIFY(w.findChild<QLabel*>("status")->text().contains("already exists"));
    }
    void cancellationAndResponsiveness() {
        QTemporaryDir dir;QFile file(dir.path()+"/large.bin");QVERIFY(file.open(QIODevice::WriteOnly));
        QByteArray block(1024*1024,Qt::Uninitialized);QRandomGenerator random(20261022);
        for(auto& byte:block)byte=static_cast<char>(random.generate()&255U);
        for(int i=0;i<64;++i)QCOMPARE(file.write(block),qint64(block.size()));file.close();
        Window w;w.show();w.addSources({file.fileName()});w.findChild<QLineEdit*>("archive")->setText(dir.path()+"/cancel.zyph");
        w.findChild<QComboBox*>("preset")->setCurrentIndex(2);
        QTest::mouseClick(w.findChild<QPushButton*>("pack"),Qt::LeftButton);
        QVERIFY(w.busy());QVERIFY(!w.findChild<QPushButton*>("pack")->isEnabled());
        int ticks=0;QTimer heartbeat;connect(&heartbeat,&QTimer::timeout,[&]{++ticks;});heartbeat.start(0);
        // Observe event-loop dispatch, rather than assuming Windows timer precision.
        QTRY_VERIFY_WITH_TIMEOUT(ticks>0,2000);heartbeat.stop();QVERIFY(w.busy());
        QTest::mouseClick(w.findChild<QPushButton*>("cancel"),Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),20000);QVERIFY(w.findChild<QLabel*>("status")->text().startsWith("Cancelled"));
        QVERIFY(!QFile::exists(dir.path()+"/cancel.zyph"));QVERIFY(QFile::exists(file.fileName()));
        QCOMPARE(QDir(dir.path()).entryList({".zyph-part-*"},QDir::AllEntries|QDir::Hidden).size(),0);
    }
    void corruptArchiveAndDrop() {
        QTemporaryDir dir;QFile file(dir.path()+"/bad.zyph");QVERIFY(file.open(QIODevice::WriteOnly));file.write("bad");file.close();
        Window w;w.show();QMimeData mime;mime.setUrls({QUrl::fromLocalFile(file.fileName())});
        QDragEnterEvent enter(QPoint(20,20),Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(&w,&enter);QVERIFY(enter.isAccepted());
        QDropEvent drop(QPointF(20,20),Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(&w,&drop);
        QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QVERIFY(w.findChild<QLabel*>("status")->text().startsWith("Failed"));
        QCOMPARE(w.findChild<QTreeWidget*>("entries")->topLevelItemCount(),0);
    }
};
QTEST_MAIN(Workflows)
#include "workflows.moc"
