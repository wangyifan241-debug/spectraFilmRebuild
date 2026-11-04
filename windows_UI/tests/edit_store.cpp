// Temporary source files and an explicit store root: this test never reads or
// writes the user's real edit history and needs neither a RAW decoder nor GPU.
#include "EditStore.hpp"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QTemporaryDir>
#include <cstdio>
#include <functional>
#include <limits>
#include <stdexcept>

namespace editing = spk::desktop::editing;
namespace {
int checks = 0;
void check(bool value, const char* message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
void rejects(const std::function<void()>& action, const char* message) {
    try { action(); } catch (const std::exception&) { ++checks; return; }
    throw std::runtime_error(message);
}
QByteArray read(const QString& path) {
    QFile file(path);
    check(file.open(QIODevice::ReadOnly), "cannot read test file");
    return file.readAll();
}
void write(const QString& path, const QByteArray& bytes) {
    QFile file(path);
    check(file.open(QIODevice::WriteOnly | QIODevice::Truncate), "cannot write test file");
    check(file.write(bytes) == bytes.size(), "incomplete test write");
}
QByteArray digest(const QString& path) {
    return QCryptographicHash::hash(read(path), QCryptographicHash::Sha256);
}
void mutateObject(QJsonObject& parent, const char* name,
                  const std::function<void(QJsonObject&)>& change) {
    auto child = parent.value(QLatin1String(name)).toObject();
    change(child);
    parent.insert(QLatin1String(name), child);
}
} // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    try {
        QTemporaryDir temporary;
        check(temporary.isValid(), "temporary root unavailable");
        const auto sources = temporary.filePath(QString::fromUtf8("照片 文件夹"));
        check(QDir().mkpath(sources), "cannot create source folder");
        const auto source = QDir(sources).filePath(QString::fromUtf8("海边 日落.ARW"));
        QByteArray input(200000, 'R');
        for (qsizetype i = 0; i < input.size(); ++i) input[i] = char((i * 31 + i / 257) % 251);
        write(source, input);
        const auto originalHash = digest(source);
        const auto originalTime = QFileInfo(source).lastModified();
        const QUrl url = QUrl::fromLocalFile(source);
        const auto root = temporary.filePath("editor-state");
        const editing::Store store(root);
        const spk::desktop::Catalog catalog{
            {{"kodak_portra_400", "Portra", false}, {"fuji_provia_100f", "Provia", true}},
            {{"kodak_portra_endura", "Portra paper", false}, {"fuji_crystal", "Crystal", false}}};
        check(!store.load(url, catalog).state && !QFileInfo::exists(root),
              "reading missing state must not create a directory");
        check(!store.load(url, catalog).blocked, "missing state is not blocked");
        rejects([&] { editing::Store invalid(QString{}); }, "empty store directory accepted");
        rejects([&] { editing::Store invalid("relative/path"); }, "relative store directory accepted");
        rejects([&] { (void)store.filePath(QUrl("https://example.com/photo.ARW")); },
                "remote source URL accepted");
        editing::State state;
        state.settings.film_stock = "fuji_provia_100f";
        state.settings.print_stock = "fuji_crystal";
        state.settings.print_exposure = 0.625;
        state.settings.film_exposure_ev = -2.25;
        state.settings.film_format_mm = 61.5;
        state.settings.grain_active = false;
        state.settings.grain_amount = 0.3125;
        state.settings.halation_active = false;
        state.settings.halation_amount = 2.625;
        state.settings.glare_active = false;
        state.settings.glare_amount = 11.25;
        state.settings.y_filter_shift = -0.3125;
        state.settings.m_filter_shift = 0.375;
        state.settings.placement={true,"power",1.5,1.25,-1.5,1.1,2,4,1.75,1.5};
        state.settings.edge.active=true;state.settings.edge.format="120_6x6";
        state.settings.edge.frame_number=17;state.settings.edge.camera_seed=8;
        state.settings.date={true,"26 10 30","dots","tl",2.5,1.2};
        state.mode = spk::desktop::DecodeMode::headroom;
        state.geometry.crop = QRectF(0.125, 0.25, 0.5, 0.625);
        state.geometry.quarterTurns = 3;
        state.geometry.flipHorizontal = true;
        state.geometry.flipVertical = true;
        state.geometry.straightenDegrees = -12.75;
        state.develop.temperature=12;state.develop.tint=-7;state.develop.exposure=0.75;
        state.develop.contrast=21;state.develop.brightness=-13;state.develop.saturation=32;
        state.develop.highlights=-44;state.develop.shadows=55;state.develop.blackPoint=3;state.develop.whitePoint=4;
        store.save(url, state);
        check(store.load(url,catalog).state==std::optional(state),"v9 linear exposure did not roundtrip");
        state.develop.legacyExposure=true;
        store.save(url,state);
        const auto path = store.filePath(url);
        const auto result = store.load(url, catalog);
        check(result.state && *result.state == state && result.warning.isEmpty() && !result.blocked,
              "complete state did not roundtrip exactly");
        check(QFileInfo(path).absolutePath() == root && QFileInfo(path).fileName().size() == 69,
              "edit file must be a hash inside the explicit store");
        check(QDir(sources).entryList(QDir::Files).size() == 1,
              "edit store wrote next to the source photograph");
        check(digest(source) == originalHash && QFileInfo(source).lastModified() == originalTime,
              "saving edits touched source bytes or modification time");
        const editing::Store reopened(root);
        check(reopened.load(url, catalog).state == result.state,
              "new store instance did not restore saved edits");
#ifdef _WIN32
        check(store.filePath(QUrl::fromLocalFile(source.toUpper())) == path,
              "Windows path case created two histories");
#endif
        const auto relativeEquivalent = QDir(sources).filePath("../" + QFileInfo(sources).fileName() + "/" + QFileInfo(source).fileName());
        check(store.filePath(QUrl::fromLocalFile(relativeEquivalent)) == path,
              "equivalent local paths created two histories");
        state.settings.print_exposure = 1.25;
        state.geometry.quarterTurns = 1;
        state.develop.zones[2]={215,.3,-.1};state.develop.curves[1]={{0,0},{.4,.3},{1,1}};
        state.develop.vignetteAmount=-35;state.develop.vignetteMidpoint=42;
        store.save(url, state);
        check(store.load(url, catalog).state == std::optional(state), "atomic update did not replace prior state");
        state.settings.auto_negative=true;store.save(url,state);
        check(store.load(url,catalog).state==std::optional(state),"Automatic inversion did not survive save/load");
        state.settings.auto_negative=false;store.save(url,state);
        const auto validBytes = read(path);
        auto validJson = QJsonDocument::fromJson(validBytes).object();
        validJson["version"]=8;
        mutateObject(validJson,"state",[](auto& s){mutateObject(s,"develop",[](auto& d){d.remove("exposureModel");});});
        rejects([&] { reopened.save(url, state); },
                "second instance overwrote edits saved since its load");
        check(read(path) == validBytes, "optimistic conflict changed existing edits");
        check(reopened.load(url, catalog).state == std::optional(state),
              "reopening did not refresh conflict observation");
        reopened.save(url, state);

        // Version 1 predates post-develop. It must load neutrally, retain all
        // previous edits, and migrate only on an explicit successful save.
        auto legacy=validJson;legacy["version"]=1;
        mutateObject(legacy,"state",[](auto& s){s.remove("develop");s.remove("placement");s.remove("filmEdge");s.remove("dateBack");s.remove("halfPair");s.remove("materialVersions");});
        write(path,QJsonDocument(legacy).toJson());
        auto legacyExpected=state;legacyExpected.develop={};legacyExpected.settings.placement={};legacyExpected.settings.edge={};legacyExpected.settings.date={};
        const auto migrated=store.load(url,catalog);
        check(migrated.state==std::optional(legacyExpected),"version 1 edits did not restore with neutral post-develop");
        store.save(url,*migrated.state);
        check(QJsonDocument::fromJson(read(path)).object()["version"].toInt()==10,"legacy save did not migrate version");
        write(path,validBytes);
        check(store.load(url,catalog).state==std::optional(state),"restoring migration fixture failed");
        auto version2=validJson;version2["version"]=2;
        mutateObject(version2,"state",[](auto& s){s.remove("placement");s.remove("filmEdge");s.remove("dateBack");s.remove("halfPair");s.remove("materialVersions");mutateObject(s,"develop",[](auto& d){
            for(const auto key:{"workingSpace","curves","zones","vignetteAmount","vignetteMidpoint"})d.remove(key);
        });});
        write(path,QJsonDocument(version2).toJson());
        auto expected2=state;expected2.settings.placement={};expected2.settings.edge={};expected2.settings.date={};expected2.develop.curves=spk::desktop::output::Develop{}.curves;
        expected2.develop.zones={};expected2.develop.vignetteAmount=0;expected2.develop.vignetteMidpoint=50;expected2.develop.legacySrgb=true;
        const auto loaded2=store.load(url,catalog);
        check(loaded2.state==std::optional(expected2),"Version 2 sRGB adjustments were reinterpreted in ProPhoto");
        store.save(url,*loaded2.state);
        check(store.load(url,catalog).state==std::optional(expected2),"Migrated v2 working-space marker was not retained");
        auto version3=validJson;version3["version"]=3;
        mutateObject(version3,"state",[](auto& s){s.remove("placement");s.remove("filmEdge");s.remove("dateBack");s.remove("halfPair");s.remove("materialVersions");});
        write(path,QJsonDocument(version3).toJson());
        auto expected3=state;expected3.settings.placement={};expected3.settings.edge={};expected3.settings.date={};
        const auto loaded3=store.load(url,catalog);
        check(loaded3.state==std::optional(expected3),"Version 3 did not restore neutral scene placement");
        store.save(url,*loaded3.state);
        check(store.load(url,catalog).state==std::optional(expected3),"Version 3 migration changed existing edits");
        auto version4=validJson;version4["version"]=4;
        mutateObject(version4,"state",[](auto& s){s.remove("filmEdge");s.remove("dateBack");s.remove("halfPair");s.remove("materialVersions");});
        write(path,QJsonDocument(version4).toJson());
        auto expected4=state;expected4.settings.edge={};expected4.settings.date={};
        check(store.load(url,catalog).state==std::optional(expected4),"Version 4 did not restore neutral film/date state");
        write(path,validBytes);check(store.load(url,catalog).state==std::optional(state),"restoring v5 fixture failed");
        auto version5=validJson;version5["version"]=5;mutateObject(version5,"state",[](auto& s){s.remove("halfPair");s.remove("materialVersions");});
        write(path,QJsonDocument(version5).toJson());check(store.load(url,catalog).state==std::optional(state),"Version 5 did not restore neutral pair state");
        write(path,validBytes);check(store.load(url,catalog).state==std::optional(state),"Restoring v6 fixture failed");
        auto version6=validJson;version6["version"]=6;mutateObject(version6,"state",[](auto& s){s.remove("materialVersions");});write(path,QJsonDocument(version6).toJson());check(store.load(url,catalog).state==std::optional(state),"Version 6 did not restore unpinned legacy materials");write(path,validBytes);check(store.load(url,catalog).state==std::optional(state),"Restoring v7 fixture failed");
        auto version7=validJson;version7["version"]=7;mutateObject(version7,"state",[](auto& s){mutateObject(s,"materialVersions",[](auto& v){v.remove("engineResourcesSha256");});});write(path,QJsonDocument(version7).toJson());check(store.load(url,catalog).state==std::optional(state),"Version 7 did not restore legacy resource context");write(path,validBytes);check(store.load(url,catalog).state==std::optional(state),"Restoring v8 fixture failed");

        auto corrupt = [&](const std::function<void(QJsonObject&)>& change) {
            auto document = validJson;
            change(document);
            const auto bytes = QJsonDocument(document).toJson();
            write(path, bytes);
            const auto rejected = store.load(url, catalog);
            check(!rejected.state && rejected.blocked && !rejected.warning.isEmpty(),
                  "invalid document loaded or failed without a warning");
            rejects([&] { store.save(url, state); }, "save replaced an incompatible or corrupt record");
            check(read(path) == bytes, "rejected save changed existing edit history");
            write(path, validBytes);
        };
        corrupt([](auto& d) { d["version"] = 10; });
        corrupt([](auto& d) { d["version"] = 9; }); // missing model must fail
        corrupt([](auto& d) { d["version"] = 9;mutateObject(d,"state",[](auto& s){mutateObject(s,"develop",[](auto& v){v["exposureModel"]="unknown";});}); });
        corrupt([](auto& d) { mutateObject(d,"state",[](auto& s){mutateObject(s,"materialVersions",[](auto& m){m["filmSha256"]="not-a-sha";});}); });
        corrupt([](auto& d) { mutateObject(d,"state",[](auto& s){mutateObject(s,"halfPair",[](auto& p){p["spacing"]=0;});}); });
        corrupt([](auto& d) { mutateObject(d,"state",[](auto& s){s.remove("filmEdge");}); });
        corrupt([](auto& d) { mutateObject(d,"state",[](auto& s){mutateObject(s,"filmEdge",[](auto& e){e["format"]="invalid";});}); });
        corrupt([](auto& d) { mutateObject(d,"state",[](auto& s){mutateObject(s,"dateBack",[](auto& e){e["text"]="not a date";});}); });
        corrupt([](auto& d) { mutateObject(d,"state",[](auto& s){s.remove("placement");}); });
        corrupt([](auto& d) { mutateObject(d,"state",[](auto& s){mutateObject(s,"placement",[](auto& p){p["norm"]="unsupported";});}); });
        corrupt([](auto& d) { mutateObject(d,"state",[](auto& s){mutateObject(s,"placement",[](auto& p){p["shadowPullBack"]=8.1;});}); });
        corrupt([](auto& d) { mutateObject(d,"state",[](auto& s){mutateObject(s,"placement",[](auto& p){p["active"]=1;});}); });
        corrupt([](auto& d) { mutateObject(d,"state",[](auto& s){s.remove("develop");}); });
        corrupt([](auto& d) { mutateObject(d,"state",[](auto& s){mutateObject(s,"develop",[](auto& p){p["exposure"]=4;});}); });
        corrupt([](auto& d) { d["version"] = "1"; });
        corrupt([](auto& d) { d["format"] = "Unrelated.Application"; });
        corrupt([](auto& d) { d["futureField"] = true; });
        corrupt([](auto& d) { d.remove("source"); });
        corrupt([](auto& d) { mutateObject(d, "source", [](auto& s) { s["path"] = "D:/different.ARW"; }); });
        corrupt([](auto& d) { mutateObject(d, "source", [](auto& s) { s["size"] = 123; }); });
        corrupt([](auto& d) { mutateObject(d, "source", [](auto& s) { s["sampleSha256"] = "broken"; }); });
        corrupt([](auto& d) { mutateObject(d, "state", [](auto& s) { s["decodeMode"] = "future-decoder"; }); });
        corrupt([](auto& d) { mutateObject(d, "state", [](auto& s) {
            mutateObject(s, "settings", [](auto& f) { f.remove("film_format_mm"); }); }); });
        corrupt([](auto& d) { mutateObject(d, "state", [](auto& s) {
            mutateObject(s, "settings", [](auto& f) { f["grain_active"] = 1; }); }); });
        corrupt([](auto& d) { mutateObject(d, "state", [](auto& s) {
            mutateObject(s, "settings", [](auto& f) { f["print_exposure"] = "1.0"; }); }); });
        corrupt([](auto& d) { mutateObject(d, "state", [](auto& s) {
            mutateObject(s, "settings", [](auto& f) { f["film_exposure_ev"] = 8.01; }); }); });
        corrupt([](auto& d) { mutateObject(d, "state", [](auto& s) {
            mutateObject(s, "settings", [](auto& f) { f["film_stock"] = "../bad"; }); }); });
        corrupt([](auto& d) { mutateObject(d, "state", [](auto& s) {
            mutateObject(s, "geometry", [](auto& g) { g["quarterTurns"] = 0.5; }); }); });
        corrupt([](auto& d) { mutateObject(d, "state", [](auto& s) {
            mutateObject(s, "geometry", [](auto& g) { g["straightenDegrees"] = -45.1; }); }); });
        corrupt([](auto& d) { mutateObject(d, "state", [](auto& s) {
            mutateObject(s, "geometry", [](auto& g) { g["flipVertical"] = "false"; }); }); });
        corrupt([](auto& d) { mutateObject(d, "state", [](auto& s) {
            mutateObject(s, "geometry", [](auto& g) {
                mutateObject(g, "crop", [](auto& c) { c["width"] = 0; }); }); }); });
        corrupt([](auto& d) { mutateObject(d, "state", [](auto& s) {
            mutateObject(s, "geometry", [](auto& g) {
                mutateObject(g, "crop", [](auto& c) { c["width"] = 1; }); }); }); });

        auto noStocks = catalog;
        noStocks.films.clear();
        const auto unavailable = store.load(url, noStocks);
        check(!unavailable.state && unavailable.blocked && !unavailable.warning.isEmpty(),
              "unavailable catalog stock silently fell back to defaults");
        rejects([&] { store.save(url, state); }, "blocked catalog load could still overwrite its history");
        check(read(path) == validBytes, "catalog validation modified saved data");
        for (const QByteArray& bytes : {QByteArray("{\"version\":1,"), QByteArray(65537, ' ')}) {
            write(path, bytes);
            const auto bad = store.load(url, catalog);
            check(!bad.state && bad.blocked && !bad.warning.isEmpty(), "bad JSON or oversize document loaded");
            rejects([&] { store.save(url, state); }, "save overwrote truncated or oversize history");
            check(read(path) == bytes, "failed save modified unreadable history");
        }
        write(path, validBytes);
        check(store.load(url, catalog).state == std::optional(state), "reopen did not clear a resolved load warning");

        auto invalidState = [&](const std::function<void(editing::State&)>& change) {
            auto candidate = state;
            change(candidate);
            rejects([&] { store.save(url, candidate); }, "invalid in-memory state was saved");
            check(read(path) == validBytes, "invalid state modified valid history");
        };
        invalidState([](auto& s) { s.settings.grain_amount = std::numeric_limits<double>::quiet_NaN(); });
        invalidState([](auto& s) { s.settings.glare_amount = std::numeric_limits<double>::infinity(); });
        invalidState([](auto& s) { s.settings.film_format_mm = 3.99; });
        invalidState([](auto& s) { s.settings.print_exposure = 0.049; });
        invalidState([](auto& s) { s.settings.halation_amount = 4.01; });
        invalidState([](auto& s) { s.settings.m_filter_shift = 1.01; });
        invalidState([](auto& s) { s.settings.film_stock = "syntactically_valid_but_missing"; });
        invalidState([](auto& s) { s.mode = static_cast<spk::desktop::DecodeMode>(17); });
        invalidState([](auto& s) { s.geometry.quarterTurns = 4; });
        invalidState([](auto& s) { s.geometry.crop = QRectF(-0.1, 0, 1, 1); });
        invalidState([](auto& s) { s.geometry.straightenDegrees = std::numeric_limits<double>::quiet_NaN(); });

        QLockFile lock(path + ".lock");
        check(lock.tryLock(0), "test could not acquire store lock");
        rejects([&] { store.save(url, state); }, "concurrent writer bypassed store lock");
        check(read(path) == validBytes, "locked save changed history");
        lock.unlock();

        // Combined timestamp + global material-context mismatch: current edits
        // go to an independent history automatically; old bytes remain intact.
        const auto sessionSource=QDir(sources).filePath("independent-history.ARW");write(sessionSource,input);
        const auto sessionUrl=QUrl::fromLocalFile(sessionSource);
        const auto sessionRoot=temporary.filePath("independent-store");editing::Store sessionStore(sessionRoot);
        auto oldCatalog=catalog;oldCatalog.engine_resources_sha256=std::string(64,'a');
        auto newCatalog=catalog;newCatalog.engine_resources_sha256=std::string(64,'b');
        auto oldState=state;oldState.settings.engine_resources_sha256=oldCatalog.engine_resources_sha256;
        (void)sessionStore.load(sessionUrl,oldCatalog);sessionStore.save(sessionUrl,oldState);
        const auto sessionMain=sessionStore.filePath(sessionUrl);
        auto oldDocument=QJsonDocument::fromJson(read(sessionMain)).object();
        mutateObject(oldDocument,"source",[](auto& s){s["mtimeMs"]=QString::number(s["mtimeMs"].toString().toLongLong()-1000);});
        const auto oldHistory=QJsonDocument(oldDocument).toJson();write(sessionMain,oldHistory);
        const auto blocked=sessionStore.load(sessionUrl,newCatalog);
        check(blocked.blocked&&!blocked.state,"Combined conflict incorrectly restored old settings");
        auto currentState=state;currentState.settings.engine_resources_sha256=newCatalog.engine_resources_sha256;currentState.settings.print_exposure=1.4;
        rejects([&]{sessionStore.save(sessionUrl,currentState);},"Ordinary save bypassed a conflicting original history");
        const auto independent=sessionStore.saveCurrentSession(sessionUrl,currentState);
        check(QFileInfo::exists(independent)&&read(sessionMain)==oldHistory,"Independent save overwrote old history");
        editing::Store sessionReopened(sessionRoot);
        check(sessionReopened.load(sessionUrl,newCatalog).state==std::optional(currentState),"New session history was not restored under its current context");
        currentState.settings.print_exposure=1.5;sessionReopened.save(sessionUrl,currentState);
        check(editing::Store(sessionRoot).load(sessionUrl,newCatalog).state==std::optional(currentState),"Subsequent independent saves did not retain latest state");
        check(read(sessionMain)==oldHistory,"Later session save rewrote the protected original");
        const auto checkpoint=sessionReopened.saveUnsavedSnapshot(sessionUrl,currentState,"owned test: source no longer trusted");
        const auto checkpointDoc=QJsonDocument::fromJson(read(checkpoint)).object();
        check(!checkpointDoc["autoApply"].toBool(true)&&checkpointDoc["format"]=="SpektraLab.Windows.UnsavedSession","Unverified checkpoint could auto-apply to a photo");

        // Same name, size and timestamp, but changed sampled bytes: the hash
        // prevents an unrelated replacement from inheriting these edits.
        QByteArray replaced = input;
        replaced[replaced.size() - 9] = char(replaced[replaced.size() - 9] ^ 63);
        write(source, replaced);
        QFile sourceHandle(source);
        check(sourceHandle.open(QIODevice::ReadWrite), "cannot open owned source timestamp");
        check(sourceHandle.setFileTime(originalTime, QFileDevice::FileModificationTime),
              "cannot restore owned source timestamp");
        sourceHandle.close();
        const auto stale = store.load(url, catalog);
        check(!stale.state && stale.blocked && stale.warning.contains("changed"),
              "changed sampled source content was not detected");
        rejects([&] { store.save(url, state); }, "save overwrote edits after the source changed");
        check(read(path) == validBytes, "stale source check discarded old edits");

        const auto another = QDir(sources).filePath("separate.ARW");
        write(another, QByteArray("independent source"));
        const auto otherUrl = QUrl::fromLocalFile(another);
        check(store.filePath(otherUrl) != path && !store.load(otherUrl, catalog).state,
              "separate source inherited another photograph's settings");
        store.save(otherUrl, editing::State{});
        check(store.load(otherUrl, catalog).state == std::optional(editing::State{}),
              "default state failed to roundtrip independently");
        const auto firstSaveSource = QDir(sources).filePath("changed-before-first-save.ARW");
        write(firstSaveSource, "first source contents");
        const auto firstSaveUrl = QUrl::fromLocalFile(firstSaveSource);
        check(!store.load(firstSaveUrl, catalog).state,
              "new source unexpectedly had edit history");
        write(firstSaveSource, "replacement contents");
        rejects([&] { store.save(firstSaveUrl, state); },
                "first save adopted a replaced source file");
        check(!QFileInfo::exists(store.filePath(firstSaveUrl)),
              "first-save source conflict created an edit record");
        const auto contested = QDir(sources).filePath("two-first-writers.ARW");
        write(contested, "contested source");
        const auto contestedUrl = QUrl::fromLocalFile(contested);
        editing::Store firstWriter(root), secondWriter(root);
        check(!firstWriter.load(contestedUrl, catalog).state && !secondWriter.load(contestedUrl, catalog).state,
              "contested source had prior state");
        firstWriter.save(contestedUrl, state);
        const auto firstBytes = read(firstWriter.filePath(contestedUrl));
        rejects([&] { secondWriter.save(contestedUrl, editing::State{}); },
                "second first-save writer replaced new history from another instance");
        check(read(firstWriter.filePath(contestedUrl)) == firstBytes,
              "competing first save modified the winner's edits");
        const auto blockedRoot = temporary.filePath("store-is-a-file");
        write(blockedRoot, "keep");
        rejects([&] { editing::Store(blockedRoot).save(otherUrl, state); },
                "save succeeded with a regular file as root");
        check(read(blockedRoot) == "keep", "failed directory creation changed its obstacle");
        rejects([&] { editing::Store(sources).save(otherUrl, state); },
                "store wrote edits beside the source image");
        check(QDir(root).entryList({"*.lock", "*.tmp"}, QDir::Files).isEmpty(),
              "store left lock or temporary files behind");

        std::printf("Edit store: %d checks passed; explicit temporary files only.\n", checks);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Edit store FAILED after %d checks: %s\n", checks, error.what());
        return 1;
    }
}
