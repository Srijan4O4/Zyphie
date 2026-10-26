#include "window.hpp"
#include "zyphie/error.hpp"
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTimer>
#include <QTimeZone>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <map>
namespace {
std::filesystem::path native(const QString& text) {
#ifdef _WIN32
    return std::filesystem::path(text.toStdWString());
#else
    return std::filesystem::u8path(text.toUtf8().constData());
#endif
}
QString sizeText(std::uint64_t size) { return QLocale().formattedDataSize(static_cast<qint64>(size)); }
QString phaseName(zyphie::Phase phase) {
    switch(phase) {
    case zyphie::Phase::scan: return "Scanning";
    case zyphie::Phase::pack: return "Compressing";
    case zyphie::Phase::verify: return "Verifying";
    case zyphie::Phase::extract: return "Extracting";
    }
    return {};
}
}
Window::Window() {
    setWindowTitle("Zyphie — Archive workspace"); resize(1040,760); setAcceptDrops(true);
    auto* root=new QWidget(this); setCentralWidget(root); auto* layout=new QVBoxLayout(root);
    auto* heading=new QHBoxLayout; auto* title=new QLabel("ZYPHIE  /  Archive workspace");
    title->setStyleSheet("font-size:24px;font-weight:600"); heading->addWidget(title); heading->addStretch();
    auto* themes=new QComboBox; themes->setObjectName("theme"); themes->setAccessibleName("Color theme");
    themes->addItems({"Light","Dark"}); heading->addWidget(themes); layout->addLayout(heading);
    connect(themes,&QComboBox::currentIndexChanged,this,[this](int index){theme(index==1);});
    auto* intro=new QLabel("Create, browse and restore your files. Drop files or folders here to get started.");
    intro->setWordWrap(true); layout->addWidget(intro);
    auto* split=new QSplitter; layout->addWidget(split,1);
    auto* create=new QGroupBox("Create an archive"); auto* left=new QVBoxLayout(create);
    sources_=new QListWidget; sources_->setObjectName("sources"); sources_->setAccessibleName("Files and folders to compress");
    left->addWidget(sources_,1); controls_ << sources_;
    auto* sourceButtons=new QHBoxLayout;
    auto button=[this](QString text,QString name,QLayout* parent,auto action) {
        auto* b=new QPushButton(text); b->setObjectName(name); parent->addWidget(b);
        connect(b,&QPushButton::clicked,this,action); controls_ << b; return b;
    };
    button("Add &files…","addFiles",sourceButtons,[this]{addSources(QFileDialog::getOpenFileNames(this,"Choose files"));});
    button("Add f&older…","addFolder",sourceButtons,[this]{auto p=QFileDialog::getExistingDirectory(this,"Choose a folder"); if(!p.isEmpty())addSources({p});});
    button("Clear","clearSources",sourceButtons,[this]{sources_->clear();}); left->addLayout(sourceButtons);
    auto* form=new QFormLayout; preset_=new QComboBox; preset_->setObjectName("preset");
    preset_->addItems({"Fast","Balanced","Maximum","Store (no compression)"}); preset_->setCurrentIndex(1);
    threads_=new QSpinBox; threads_->setRange(1,16); threads_->setValue(1); threads_->setObjectName("threads");
    form->addRow("Compression &mode",preset_); form->addRow("&Workers",threads_); left->addLayout(form); controls_ << preset_ << threads_;
    button("&Create archive","pack",left,[this]{start(Job::pack);}); split->addWidget(create);
    auto* browse=new QGroupBox("Browse and restore"); auto* right=new QVBoxLayout(browse);
    tree_=new QTreeWidget; tree_->setObjectName("entries"); tree_->setAccessibleName("Archive entries; select files or folders to restore");
    tree_->setHeaderLabels({"Name","Original size","Stored size","Modified (UTC)"});
    tree_->setSelectionMode(QAbstractItemView::ExtendedSelection); right->addWidget(tree_,1); controls_ << tree_;
    button("&Open archive","open",right,[this]{start(Job::open);});
    auto* restore=new QHBoxLayout;
    auto* extractAll=button("&Extract all","extract",restore,[this]{start(Job::extract);});
    extractAll->setToolTip("Restore every file and folder, regardless of the highlighted entries.");
    extractSelected_=button("Extract &selected","extractSelected",restore,[this]{start(Job::extractSelected);});
    extractSelected_->setToolTip("Restore only the selected entries. Folders include all their contents.");
    connect(tree_,&QTreeWidget::itemSelectionChanged,this,[this]{updateExtractionSelection();});
    updateExtractionSelection();
    button("&Verify archive","verify",restore,[this]{start(Job::verify);}); right->addLayout(restore);
    auto* hint=new QLabel("Extract all restores the entire archive, even with highlighted entries. Extract selected restores only your selection, including folder contents."); hint->setWordWrap(true); right->addWidget(hint); split->addWidget(browse);
    auto* paths=new QFormLayout;
    auto* archiveRow=new QHBoxLayout; archive_=new QLineEdit; archive_->setObjectName("archive"); archive_->setPlaceholderText("Choose a .zyph file to create or open"); archiveRow->addWidget(archive_);
    button("Browse…","archivePicker",archiveRow,[this]{auto p=QFileDialog::getOpenFileName(this,"Open archive",{},"Zyphie archives (*.zyph)");if(!p.isEmpty())archive_->setText(p);});
    button("New…","newArchivePicker",archiveRow,[this]{auto p=QFileDialog::getSaveFileName(this,"Create archive",{},"Zyphie archives (*.zyph)",nullptr,QFileDialog::DontConfirmOverwrite);if(!p.isEmpty())archive_->setText(p.endsWith(".zyph",Qt::CaseInsensitive)?p:p+".zyph");});
    connect(archive_,&QLineEdit::textChanged,this,[this]{tree_->clear();});
    auto* archiveLabel=new QLabel("&Archive file"); archiveLabel->setBuddy(archive_);
    paths->addRow(archiveLabel,archiveRow); archive_->setAccessibleName("Archive file");
    auto* destinationRow=new QHBoxLayout; destination_=new QLineEdit; destination_->setObjectName("destination"); destination_->setPlaceholderText("A new folder that does not already exist"); destinationRow->addWidget(destination_);
    button("Choose parent…","destinationPicker",destinationRow,[this]{auto p=QFileDialog::getExistingDirectory(this,"Choose parent for new restored folder");if(!p.isEmpty())destination_->setText(p+"/Restored files");});
    auto* destinationLabel=new QLabel("Restore &destination"); destinationLabel->setBuddy(destination_);
    paths->addRow(destinationLabel,destinationRow); destination_->setAccessibleName("Restore destination"); layout->addLayout(paths); controls_ << archive_ << destination_;
    progress_=new QProgressBar; progress_->setObjectName("progress"); progress_->setRange(0,1000); progress_->setValue(0); progress_->setAccessibleName("Operation progress"); layout->addWidget(progress_);
    status_=new QLabel("Ready — choose files to create an archive, or open an existing archive."); status_->setObjectName("status"); status_->setWordWrap(true); status_->setTextFormat(Qt::PlainText); layout->addWidget(status_);
    details_=new QLabel; details_->setTextFormat(Qt::PlainText); details_->setWordWrap(true); layout->addWidget(details_);
    cancel_=new QPushButton("&Cancel operation"); cancel_->setObjectName("cancel"); cancel_->setEnabled(false); layout->addWidget(cancel_);
    connect(cancel_,&QPushButton::clicked,this,[this]{worker_.request_stop();status_->setText("Cancelling safely — waiting for the current block…");cancel_->setEnabled(false);});
    timer_=new QTimer(this); timer_->setInterval(50); connect(timer_,&QTimer::timeout,this,[this]{poll();}); theme(false);
}
Window::~Window() { worker_.request_stop(); if(worker_.joinable())worker_.join(); }
void Window::theme(bool dark) {
    setStyleSheet(dark ? "QWidget{background:#19212c;color:#edf2fa;font-size:13px} QLineEdit,QListWidget,QTreeWidget,QComboBox,QSpinBox{background:#253244;border:1px solid #52627a;padding:6px} QPushButton{background:#304764;padding:9px;border-radius:5px} QPushButton:disabled{color:#8e9caf} QGroupBox{border:1px solid #52627a;border-radius:6px;margin-top:12px;padding-top:14px} QProgressBar::chunk{background:#73b3ff}" : "QWidget{background:#f3f6fb;color:#18283d;font-size:13px} QLineEdit,QListWidget,QTreeWidget,QComboBox,QSpinBox{background:white;border:1px solid #b7c5d8;padding:6px} QPushButton{background:#dce8f8;padding:9px;border-radius:5px} QPushButton:disabled{color:#768399} QGroupBox{border:1px solid #b7c5d8;border-radius:6px;margin-top:12px;padding-top:14px} QProgressBar::chunk{background:#2875ce}");
}
void Window::addSources(const QStringList& paths) { if(busy_)return;for(const auto& p:paths)if(!p.isEmpty()&&sources_->findItems(p,Qt::MatchExactly).isEmpty())sources_->addItem(p); }
void Window::dragEnterEvent(QDragEnterEvent* e) { if(!busy_&&e->mimeData()->hasUrls())e->acceptProposedAction(); }
void Window::dropEvent(QDropEvent* e) {
    QStringList paths;for(const auto& u:e->mimeData()->urls())if(u.isLocalFile())paths<<u.toLocalFile();
    if(paths.size()==1&&paths.front().endsWith(".zyph",Qt::CaseInsensitive)){archive_->setText(paths.front());start(Job::open);}else addSources(paths);
    e->acceptProposedAction();
}
void Window::closeEvent(QCloseEvent* e) {
    if(busy_){worker_.request_stop();status_->setText("Cancelling safely. Close the window again when the operation finishes.");e->ignore();}else e->accept();
}
void Window::updateExtractionSelection() {
    const auto count=tree_->selectedItems().size();
    extractSelected_->setEnabled(!busy_ && count>0);
    extractSelected_->setText(count?QString("Extract &selected (%1)").arg(count):"Extract &selected");
}
void Window::start(Job job) {
    if(busy_)return;
    const bool extracting=job==Job::extract || job==Job::extractSelected;
    std::vector<std::string> selected;
    if(job==Job::extractSelected) {
        for(auto* item:tree_->selectedItems())selected.push_back(item->data(0,Qt::UserRole).toString().toUtf8().toStdString());
        // Empty paths mean all in the core API: never fall back implicitly.
        if(selected.empty()){status_->setText("Select files or folders to restore, or choose Extract all.");return;}
    }
    const auto archive=native(archive_->text()), destination=native(destination_->text());
    std::vector<std::filesystem::path> sources;
    for(int i=0;i<sources_->count();++i)sources.push_back(native(sources_->item(i)->text()));
    if(archive.empty()||(job==Job::pack&&sources.empty())||(extracting&&destination.empty())) {
        status_->setText("Choose an archive path and the required source files or restore destination.");return;
    }
    // Refuse conflicts before launch, and again atomically inside the core.
    std::error_code ec; const auto output=job==Job::pack?archive:destination;
    if((job==Job::pack||extracting)&&std::filesystem::exists(output,ec)) {
        status_->setText("Destination already exists. Choose a new name; existing files have not been changed.");
        QMessageBox::warning(this,"Choose a new destination",status_->text());return;
    }
    zyphie::PackOptions options;options.threads=static_cast<unsigned>(threads_->value());
    options.store=preset_->currentIndex()==3;options.compression_level=preset_->currentIndex()==0?1:preset_->currentIndex()==2?19:3;
    {std::lock_guard lock(mutex_);done_=false;success_=false;latest_={};entries_.clear();result_.clear();}
    job_=job;busy_=true;for(auto* c:controls_)c->setEnabled(false);cancel_->setEnabled(true);elapsed_.start();
    phase_=zyphie::Phase::scan;phaseElapsed_.start();
    status_->setText("Starting…");progress_->setRange(0,0);timer_->start();
    worker_=std::jthread([this,job,archive,destination,sources=std::move(sources),selected=std::move(selected),options](std::stop_token stop) mutable {
        auto report=[this](const zyphie::Progress& p){std::lock_guard lock(mutex_);latest_=p;};
        QString result;bool success=false;std::vector<zyphie::ArchiveEntry> entries;
        try {
            if(job==Job::pack){options.cancellation=stop;options.progress=report;zyphie::ArchiveWriter::pack(sources,archive,options);
                zyphie::ArchiveReader reader(archive);std::uint64_t total=0;for(const auto& entry:reader.entries())total+=entry.size;
                const double savings=total?100.0*(1.0-static_cast<double>(reader.archive_size())/static_cast<double>(total)):0;
                result=QString("Success — archive created and fully verified. %1; savings %2%.").arg(sizeText(reader.archive_size())).arg(savings,0,'f',1);
            }else{
                zyphie::ArchiveReader reader(archive);
                if(job==Job::open){zyphie::require(!stop.stop_requested(),zyphie::ErrorCode::cancelled,"Archive opening cancelled");entries=reader.entries();result=QString("Opened %1 entries. Payload integrity has not yet been verified.").arg(entries.size());}
                else if(job==Job::verify){reader.verify(stop,report);result="Success — all file and block hashes verified.";}
                else{zyphie::ExtractOptions extract;extract.paths=selected;extract.cancellation=stop;extract.progress=report;reader.extract(destination,extract);result=selected.empty()?"Success — all files restored and verified.":"Success — selected files restored and verified. Unselected payloads were not verified.";}
            }
            success=true;
        }catch(const zyphie::Error& e){result=QString(e.code()==zyphie::ErrorCode::cancelled?"Cancelled — ":"Failed — ")+QString::fromUtf8(e.what());}
        catch(const std::exception& e){result="Failed — "+QString::fromUtf8(e.what());}
        std::lock_guard lock(mutex_);result_=result;success_=success;entries_=std::move(entries);done_=true;
    });
}
void Window::poll() {
    zyphie::Progress p;bool done,success;QString result;std::vector<zyphie::ArchiveEntry> entries;
    {std::lock_guard lock(mutex_);p=latest_;done=done_;success=success_;if(done){result=result_;entries=std::move(entries_);}}
    const double seconds=static_cast<double>(elapsed_.elapsed())/1000;
    if(p.phase!=phase_){phase_=p.phase;phaseElapsed_.restart();}
    if(p.bytes_total){progress_->setRange(0,1000);progress_->setValue(static_cast<int>(1000.0*static_cast<double>(p.bytes_done)/static_cast<double>(p.bytes_total)));}
    if(cancel_->isEnabled())status_->setText(phaseName(p.phase)+" — "+QString::fromUtf8(p.path));
    details_->setText(QString("%1 / %2 · %3 entries · elapsed %4 s").arg(sizeText(p.bytes_done),p.bytes_total?sizeText(p.bytes_total):"unknown").arg(p.entries_done).arg(seconds,0,'f',1));
    // Only estimate the current phase after measuring it for at least one second.
    if(phaseElapsed_.elapsed()>1000 && p.bytes_done && p.bytes_total>p.bytes_done) {
        const double remaining=static_cast<double>(phaseElapsed_.elapsed())/1000*
            static_cast<double>(p.bytes_total-p.bytes_done)/static_cast<double>(p.bytes_done);
        details_->setText(details_->text()+QString(" · estimated %1 s remaining in this phase").arg(remaining,0,'f',0));
    }
    if(!done)return;
    worker_.join();timer_->stop();busy_=false;for(auto* c:controls_)c->setEnabled(true);cancel_->setEnabled(false);
    updateExtractionSelection();
    progress_->setRange(0,1000);progress_->setValue(success?1000:0);status_->setText(result);
    if(!success && !result.startsWith("Cancelled")) {
        auto* message=new QMessageBox(QMessageBox::Warning,"Operation failed",result,QMessageBox::Ok,this);
        message->setTextFormat(Qt::PlainText);message->setAttribute(Qt::WA_DeleteOnClose);message->open();
    }
    if(job_==Job::open){tree_->clear();if(success){std::map<std::string,QTreeWidgetItem*> parents;
        for(const auto& entry:entries){const auto slash=entry.path.rfind('/');const auto parent=slash==std::string::npos?std::string{}:entry.path.substr(0,slash);
            auto* item=new QTreeWidgetItem;item->setText(0,QString::fromUtf8(slash==std::string::npos?entry.path:entry.path.substr(slash+1)));
            item->setData(0,Qt::UserRole,QString::fromUtf8(entry.path));item->setText(1,entry.type==zyphie::EntryType::directory?"Folder":sizeText(entry.size));item->setText(2,sizeText(entry.stored_size));
            if(entry.modified)item->setText(3,QDateTime::fromSecsSinceEpoch(*entry.modified,QTimeZone::UTC).toString(Qt::ISODate));
            if(parent.empty())tree_->addTopLevelItem(item);else parents.at(parent)->addChild(item);parents.emplace(entry.path,item);
        }tree_->expandToDepth(0);tree_->resizeColumnToContents(0);}}
}
