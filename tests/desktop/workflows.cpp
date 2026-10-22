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
class Workflows : public QObject {
    Q_OBJECT
private slots:
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
        QByteArray block(1024*1024,'x');for(int i=0;i<64;++i)QCOMPARE(file.write(block),qint64(block.size()));file.close();
        Window w;w.show();w.addSources({file.fileName()});w.findChild<QLineEdit*>("archive")->setText(dir.path()+"/cancel.zyph");
        w.findChild<QComboBox*>("preset")->setCurrentIndex(2);
        QTest::mouseClick(w.findChild<QPushButton*>("pack"),Qt::LeftButton);
        QVERIFY(w.busy());QVERIFY(!w.findChild<QPushButton*>("pack")->isEnabled());
        int ticks=0;QTimer heartbeat;connect(&heartbeat,&QTimer::timeout,[&]{++ticks;});heartbeat.start(1);
        QTest::qWait(20);QVERIFY(ticks>0);QTest::mouseClick(w.findChild<QPushButton*>("cancel"),Qt::LeftButton);
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
