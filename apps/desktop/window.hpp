#pragma once
#include "zyphie/archive.hpp"
#include <QMainWindow>
#include <QElapsedTimer>
#include <mutex>
#include <thread>
class QLineEdit; class QListWidget; class QTreeWidget; class QProgressBar;
class QLabel; class QPushButton; class QComboBox; class QSpinBox; class QTimer;
class Window final : public QMainWindow {
public:
    Window();
    ~Window() override;
    void addSources(const QStringList& paths);
    bool busy() const { return busy_; }
protected:
    void closeEvent(QCloseEvent*) override;
    void dragEnterEvent(QDragEnterEvent*) override;
    void dropEvent(QDropEvent*) override;
private:
    enum class Job { open, pack, extract, verify };
    void start(Job);
    void poll();
    void theme(bool dark);
    QLineEdit *archive_, *destination_;
    QListWidget* sources_;
    QTreeWidget* tree_;
    QComboBox* preset_;
    QSpinBox* threads_;
    QProgressBar* progress_;
    QLabel *status_, *details_;
    QPushButton* cancel_;
    QList<QWidget*> controls_;
    QTimer* timer_;
    QElapsedTimer elapsed_;
    QElapsedTimer phaseElapsed_;
    zyphie::Phase phase_=zyphie::Phase::scan;
    std::jthread worker_;
    std::mutex mutex_;
    zyphie::Progress latest_;
    bool busy_=false, done_=false, success_=false;
    QString result_;
    std::vector<zyphie::ArchiveEntry> entries_;
    Job job_=Job::open;
};
