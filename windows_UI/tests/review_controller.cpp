#include "PreviewController.hpp"
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QLockFile>
#include <QElapsedTimer>
#include <QThread>
#include <atomic>
#include <iostream>
#include <stdexcept>

static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class F> static void until(F done) {
    QElapsedTimer timer;timer.start();
    while(!done()&&timer.elapsed()<30000){QCoreApplication::processEvents();QThread::msleep(2);}
    check(done(),"controller deadline exceeded");
}
struct PreviewControllerReviewTest {
    static void run(PreviewController& c,const QString& root) {
        until([&]{return !c.busy();});check(c.ready(),"engine initialization failed");
        const auto started=std::make_shared<std::atomic_bool>(false),release=std::make_shared<std::atomic_bool>(false);
        struct ReleaseOnExit {std::shared_ptr<std::atomic_bool> flag;~ReleaseOnExit(){*flag=true;}} releaseOnExit{release};
        auto queue=[&](QString operation,auto work) {
            const auto gen=++c.generation_;const auto rev=c.settings_revision_;
            c.submit([=](auto& host){work(host);PreviewController::Reply r;r.generation=gen;r.settings_revision=rev;r.operation=operation;return r;},"review",operation);
        };
        queue("render",[started,release](auto&){*started=true;while(!*release)QThread::msleep(1);});
        until([&]{return started->load();});
        // A retired completion may not clear a new task's state, even if a
        // future entry point allows queueing work while the first is running.
        const auto retired=c.generation_;
        queue("export",[](auto&){});
        PreviewController::Reply old;old.generation=retired;old.operation="render";
        c.accept(std::move(old));check(c.busy()&&c.active_operation_=="export","retired reply cleared newer busy");
        *release=true;until([&]{return !c.busy();});check(!c.failed(),"newer export state lost");
        *started=false;*release=false;
        queue("render",[started,release](auto&){*started=true;while(!*release)QThread::msleep(1);});
        until([&]{return started->load();});++c.settings_revision_;*release=true;
        until([&]{return !c.busy();});check(!c.failed(),"superseded edit became a failure");
        queue("export",[](auto&){throw std::runtime_error("injected worker failure");});
        until([&]{return !c.busy();});check(c.failed()&&c.status().contains("export: injected worker failure"),"std exception metadata lost");
        c.cancel_batch_=std::make_shared<std::atomic_bool>(false);
        *started=false;*release=false;
        queue("batch-export",[started,release](auto&){*started=true;while(!*release)QThread::msleep(1);throw 42;});
        until([&]{return started->load();});c.cancelExport();check(c.cancel_batch_->load(),"cancel not delivered");*release=true;
        until([&]{return !c.busy();});check(c.failed()&&!c.cancel_batch_&&c.status().contains("batch-export"),"non-standard exception/cancel did not retire");
        queue("export",[](auto&){});until([&]{return !c.busy();});check(!c.failed(),"worker did not survive exception");

        const auto source=root+"/owned.ARW";QFile file(source);check(file.open(QIODevice::WriteOnly),"source fixture failed");file.write("owned source identity");file.close();
        PreviewController::LibraryItem item;item.url=QUrl::fromLocalFile(source);item.pending_save=true;
        c.edit_store_.load(item.url,c.catalog_);c.library_.push_back(item);
        const auto record=c.edit_store_.filePath(item.url);
        QLockFile lock(record+".lock");check(lock.tryLock(),"test lock failed");
        c.flushEdits();check(c.library_.back().pending_save&&!c.persistenceWarning().isEmpty()&&c.save_timer_.isActive(),"save failure was silent or not retried");
        lock.unlock();until([&]{return !c.library_.back().pending_save;});check(c.persistenceWarning().isEmpty(),"recovered save kept warning");
        const auto saved=c.edit_store_.load(item.url,c.catalog_);check(saved.state.has_value(),"recovered record did not reopen");
        check(lock.tryLock(),"second test lock failed");c.library_.back().pending_save=true;c.library_.back().save_failures=0;
        for(int n=0;n<4;++n)c.flushEdits();
        check(!c.save_timer_.isActive()&&!c.prepareToClose(),"retry was unbounded or failed save allowed normal close");
        lock.unlock();c.retrySave();check(c.prepareToClose()&&c.persistenceWarning().isEmpty(),"explicit retry did not recover close");
    }
};
int main(int argc,char** argv) {
    QGuiApplication app(argc,argv);
    try {
        check(argc==2,"resources required");QTemporaryDir owned;check(owned.isValid(),"owned directory failed");
        PreviewController c(std::filesystem::path(QString::fromLocal8Bit(argv[1]).toStdWString()),nullptr,{},owned.filePath("edits"));
        PreviewControllerReviewTest::run(c,owned.path());
        std::cout<<"PASS retired generation, superseded edit, export/cancel, std/nonstd exception recovery, save retry/reopen/bound/close\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
