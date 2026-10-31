#include "EditStore.hpp"
#include "PairProject.hpp"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLockFile>
#include <QRegularExpression>
#include <QSaveFile>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace spk::desktop::editing {
namespace {
constexpr qint64 maximumDocumentBytes = 64 * 1024;
constexpr qint64 sampleBytes = 64 * 1024;

[[noreturn]] void fail(const QString& message) {
    throw std::runtime_error(message.toUtf8().toStdString());
}
void require(bool value, const QString& message) { if (!value) fail(message); }

QString sourcePath(const QUrl& url) {
    require(url.isLocalFile() && !url.toLocalFile().isEmpty(),
            QStringLiteral("Saved edits require a local source file"));
    const QFileInfo info(url.toLocalFile());
    const auto canonical = info.canonicalFilePath();
    return QDir::cleanPath(canonical.isEmpty() ? info.absoluteFilePath() : canonical);
}
QString pathIdentity(const QString& path) {
#ifdef _WIN32
    return QDir::fromNativeSeparators(path).toCaseFolded();
#else
    return QDir::fromNativeSeparators(path);
#endif
}
QString observationKey(const QUrl& source) {
    require(source.isLocalFile() && !source.toLocalFile().isEmpty(),
            QStringLiteral("Saved edits require a local source file"));
    // Keep this key tied to the name the user opened. If a symlink is retargeted,
    // its canonical record path changes, but it must still fail the observation
    // check instead of looking like a never-before-opened photograph.
    return pathIdentity(QDir::cleanPath(QFileInfo(source.toLocalFile()).absoluteFilePath()));
}
void keys(const QJsonObject& object, std::initializer_list<const char*> expected,
          const QString& label) {
    require(object.size() == qsizetype(expected.size()),
            QStringLiteral("Saved edits contain missing or unsupported fields in ") + label);
    for (const char* key : expected)
        require(object.contains(QLatin1String(key)),
                QStringLiteral("Saved edits are missing ") + label + "." + QLatin1String(key));
}
QJsonObject object(const QJsonObject& parent, const char* key) {
    const auto value = parent.value(QLatin1String(key));
    require(value.isObject(), QStringLiteral("Saved edit field must be an object: ") + QLatin1String(key));
    return value.toObject();
}
QString string(const QJsonObject& parent, const char* key) {
    const auto value = parent.value(QLatin1String(key));
    require(value.isString(), QStringLiteral("Saved edit field must be text: ") + QLatin1String(key));
    return value.toString();
}
double number(const QJsonObject& parent, const char* key, double low, double high) {
    const auto value = parent.value(QLatin1String(key));
    require(value.isDouble(), QStringLiteral("Saved edit field must be numeric: ") + QLatin1String(key));
    const double result = value.toDouble();
    require(std::isfinite(result) && result >= low && result <= high,
            QStringLiteral("Saved edit field is outside its valid range: ") + QLatin1String(key));
    return result;
}
bool boolean(const QJsonObject& parent, const char* key) {
    const auto value = parent.value(QLatin1String(key));
    require(value.isBool(), QStringLiteral("Saved edit field must be true or false: ") + QLatin1String(key));
    return value.toBool();
}
QString stockId(const QJsonObject& parent, const char* key,
                const std::vector<Stock>* catalog) {
    const QString id = string(parent, key);
    static const QRegularExpression syntax(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9_-]{0,127}$"));
    require(syntax.match(id).hasMatch(), QStringLiteral("Saved edits contain an invalid stock ID"));
    if (catalog)
        require(std::any_of(catalog->begin(), catalog->end(), [&](const Stock& stock) {
                    return QString::fromStdString(stock.id) == id;
                }), QStringLiteral("Saved edits refer to an unavailable stock: ") + id);
    return id;
}

QJsonObject stateJson(const State& state) {
    const auto& s = state.settings;
    const auto& g = state.geometry;
    const auto& d = state.develop;
    const auto& p = s.placement;
    const auto& e = s.edge;const auto& date=s.date;
    output::validateDevelop(d);
    QJsonArray curves,zones;
    for(const auto& curve:d.curves) {
        QJsonArray points;for(const auto& p:curve)points.append(QJsonObject{{"x",p.x},{"y",p.y}});
        curves.append(points);
    }
    for(const auto& z:d.zones)zones.append(QJsonObject{{"hue",z.hue},{"saturation",z.saturation},{"luminance",z.luminance}});
    QString mode;
    switch (state.mode) {
    case DecodeMode::compatible16: mode = QStringLiteral("compatible16"); break;
    case DecodeMode::headroom: mode = QStringLiteral("headroom"); break;
    default: fail(QStringLiteral("Saved edits contain an unsupported RAW decode mode"));
    }
    return {{"settings", QJsonObject{
        {"film_stock", QString::fromStdString(s.film_stock)},
        {"print_stock", QString::fromStdString(s.print_stock)},
        {"print_exposure", s.print_exposure}, {"film_exposure_ev", s.film_exposure_ev},
        {"film_format_mm", s.film_format_mm}, {"grain_active", s.grain_active},
        {"grain_amount", s.grain_amount}, {"halation_active", s.halation_active},
        {"halation_amount", s.halation_amount}, {"glare_active", s.glare_active},
        {"glare_amount", s.glare_amount}, {"y_filter_shift", s.y_filter_shift},
        {"m_filter_shift", s.m_filter_shift}}},
        {"develop", QJsonObject{{"enabled",d.enabled},{"temperature",d.temperature},{"tint",d.tint},
            {"exposure",d.exposure},{"contrast",d.contrast},{"brightness",d.brightness},
            {"saturation",d.saturation},{"highlights",d.highlights},{"shadows",d.shadows},
            {"blackPoint",d.blackPoint},{"whitePoint",d.whitePoint},
            {"workingSpace",d.legacySrgb?"sRGB":"ProPhoto RGB"},{"curves",curves},{"zones",zones},
            {"vignetteAmount",d.vignetteAmount},{"vignetteMidpoint",d.vignetteMidpoint}}},
        {"placement",QJsonObject{{"active",p.active},{"norm",QString::fromStdString(p.norm)},
            {"highlightKnee",p.highlight_knee},{"highlightRoom",p.highlight_room},
            {"shadowKnee",p.shadow_knee},{"shadowRoom",p.shadow_room},
            {"rolloff",p.rolloff},{"maxLift",p.max_lift},
            {"highlightPullBack",p.highlight_pull_back},{"shadowPullBack",p.shadow_pull_back}}},
        {"filmEdge",QJsonObject{{"active",e.active},{"format",QString::fromStdString(e.format)},
            {"mode",QString::fromStdString(e.mode)},{"gate",QString::fromStdString(e.gate)},
            {"holes",QString::fromStdString(e.holes)},{"carrier",QString::fromStdString(e.carrier)},
            {"cameraSeed",e.camera_seed},{"frameSeed",e.frame_seed},{"frameNumber",e.frame_number},{"fog",e.fog},{"leaks",e.leaks}}},
        {"dateBack",QJsonObject{{"active",date.active},{"text",QString::fromStdString(date.text)},
            {"style",QString::fromStdString(date.style)},{"corner",QString::fromStdString(date.corner)},
            {"exposure",date.exposure_ev},{"size",date.size}}},
        {"halfPair",pair_project::recipeJson(s.pair)},
        {"materialVersions",QJsonObject{{"filmSha256",QString::fromStdString(s.film_profile_sha256)},{"paperSha256",QString::fromStdString(s.print_profile_sha256)},{"engineResourcesSha256",QString::fromStdString(s.engine_resources_sha256)}}},
        {"decodeMode", mode}, {"geometry", QJsonObject{
            {"crop", QJsonObject{{"x", g.crop.x()}, {"y", g.crop.y()},
                                  {"width", g.crop.width()}, {"height", g.crop.height()}}},
            {"quarterTurns", g.quarterTurns}, {"flipHorizontal", g.flipHorizontal},
            {"flipVertical", g.flipVertical}, {"straightenDegrees", g.straightenDegrees}}}};
}
State parseState(const QJsonObject& value, const Catalog* catalog) {
    if(value.contains("materialVersions"))keys(value,{"settings","decodeMode","geometry","develop","placement","filmEdge","dateBack","halfPair","materialVersions"},QStringLiteral("state"));
    else if(value.contains("halfPair"))keys(value,{"settings","decodeMode","geometry","develop","placement","filmEdge","dateBack","halfPair"},QStringLiteral("state"));
    else if(value.contains("filmEdge"))keys(value,{"settings","decodeMode","geometry","develop","placement","filmEdge","dateBack"},QStringLiteral("state"));
    else if(value.contains("placement"))keys(value,{"settings","decodeMode","geometry","develop","placement"},QStringLiteral("state"));
    else if(value.contains("develop"))keys(value, {"settings", "decodeMode", "geometry", "develop"}, QStringLiteral("state"));
    else keys(value, {"settings", "decodeMode", "geometry"}, QStringLiteral("state"));
    State state;
    const auto s = object(value, "settings");
    keys(s, {"film_stock", "print_stock", "print_exposure", "film_exposure_ev", "film_format_mm",
             "grain_active", "grain_amount", "halation_active", "halation_amount", "glare_active",
             "glare_amount", "y_filter_shift", "m_filter_shift"}, QStringLiteral("settings"));
    auto& out = state.settings;
    if(value.contains("halfPair"))out.pair=pair_project::parseRecipe(object(value,"halfPair"));
    out.film_stock = stockId(s, "film_stock", catalog ? &catalog->films : nullptr).toStdString();
    out.print_stock = stockId(s, "print_stock", catalog ? &catalog->papers : nullptr).toStdString();
    if(value.contains("materialVersions")) {
        const auto versions=object(value,"materialVersions");if(versions.contains("engineResourcesSha256"))keys(versions,{"filmSha256","paperSha256","engineResourcesSha256"},QStringLiteral("materialVersions"));else keys(versions,{"filmSha256","paperSha256"},QStringLiteral("materialVersions"));
        const auto pin=[&](const char* key,const std::string& id,const std::vector<Stock>* stocks){
            const auto sha=string(versions,key);require(sha.isEmpty()||QRegularExpression("^[0-9a-f]{64}$").match(sha).hasMatch(),QStringLiteral("Invalid material version SHA256"));
            if(stocks&&!sha.isEmpty()){const auto it=std::find_if(stocks->begin(),stocks->end(),[&](const Stock& stock){return stock.id==id;});require(it!=stocks->end()&&(it->profile_sha256.empty()||it->profile_sha256==sha.toStdString()),QStringLiteral("Material version mismatch; saved edits preserved: ")+QString::fromStdString(id));}return sha.toStdString();
        };
        out.film_profile_sha256=pin("filmSha256",out.film_stock,catalog?&catalog->films:nullptr);out.print_profile_sha256=pin("paperSha256",out.print_stock,catalog?&catalog->papers:nullptr);
        if(versions.contains("engineResourcesSha256")){const auto sha=string(versions,"engineResourcesSha256");require(sha.isEmpty()||QRegularExpression("^[0-9a-f]{64}$").match(sha).hasMatch(),QStringLiteral("Invalid engine resource SHA256"));out.engine_resources_sha256=sha.toStdString();require(!catalog||sha.isEmpty()||catalog->engine_resources_sha256.empty()||catalog->engine_resources_sha256==out.engine_resources_sha256,QStringLiteral("Material version mismatch: global spectral/filter/LUT resources changed; saved edits preserved"));}
    }
    out.print_exposure = number(s, "print_exposure", 0.05, 20.0);
    out.film_exposure_ev = number(s, "film_exposure_ev", -8.0, 8.0);
    out.film_format_mm = number(s, "film_format_mm", 4.0, 200.0);
    out.grain_active = boolean(s, "grain_active");
    out.grain_amount = number(s, "grain_amount", 0.0, 2.0);
    out.halation_active = boolean(s, "halation_active");
    out.halation_amount = number(s, "halation_amount", 0.0, 4.0);
    out.glare_active = boolean(s, "glare_active");
    out.glare_amount = number(s, "glare_amount", 0.0, 30.0);
    out.y_filter_shift = number(s, "y_filter_shift", -1.0, 1.0);
    out.m_filter_shift = number(s, "m_filter_shift", -1.0, 1.0);
    if(value.contains("filmEdge")) {
        const auto e=object(value,"filmEdge"),d=object(value,"dateBack");
        keys(e,{"active","format","mode","gate","holes","carrier","cameraSeed","frameSeed","frameNumber","fog","leaks"},QStringLiteral("filmEdge"));
        keys(d,{"active","text","style","corner","exposure","size"},QStringLiteral("dateBack"));
        auto& edge=out.edge;auto& date=out.date;
        edge.active=boolean(e,"active");edge.format=string(e,"format").toStdString();edge.mode=string(e,"mode").toStdString();
        edge.gate=string(e,"gate").toStdString();edge.holes=string(e,"holes").toStdString();edge.carrier=string(e,"carrier").toStdString();
        auto integer=[&](const char* k,int maximum){const double v=number(e,k,0,maximum);require(v==std::floor(v),QStringLiteral("Film registration must be an integer"));return int(v);};
        edge.camera_seed=integer("cameraSeed",2147483647);edge.frame_seed=integer("frameSeed",2147483647);edge.frame_number=integer("frameNumber",99);
        edge.fog=number(e,"fog",0,4);edge.leaks=number(e,"leaks",0,4);
        require(QStringList{"135","135_half","120_645","120_6x6","120_6x7","120_6x8","120_6x9","135_xpan","120_6x12","120_6x17"}.contains(QString::fromStdString(edge.format)),QStringLiteral("Unknown film format"));
        require(edge.mode=="strip"||edge.mode=="filed",QStringLiteral("Unknown film view"));
        require(QStringList{"auto","square","rounded","eared","shouldered","kicked"}.contains(QString::fromStdString(edge.gate)),QStringLiteral("Unknown camera gate"));
        require(edge.holes=="white"||edge.holes=="black",QStringLiteral("Unknown hole backing"));
        require(edge.carrier=="none"||edge.carrier=="black"||edge.carrier=="open",QStringLiteral("Unknown film carrier"));
        date.active=boolean(d,"active");date.text=string(d,"text").toStdString();date.style=string(d,"style").toStdString();date.corner=string(d,"corner").toStdString();
        require(date.text.size()<=64&&std::all_of(date.text.begin(),date.text.end(),[](unsigned char c){return (c>='0'&&c<='9')||c==' '||c=='-'||c==':'||c=='/'||c=='.'||c=='\'';}),QStringLiteral("Invalid date-back text"));
        require(date.style=="lcd"||date.style=="dots"||date.style=="data",QStringLiteral("Unknown date style"));
        require(QStringList{"br","bl","tr","tl"}.contains(QString::fromStdString(date.corner)),QStringLiteral("Unknown date corner"));
        date.exposure_ev=number(d,"exposure",-2,8);date.size=number(d,"size",.4,3);
    }
    if(value.contains("placement")) {
        const auto p=object(value,"placement");
        keys(p,{"active","norm","highlightKnee","highlightRoom","shadowKnee","shadowRoom","rolloff","maxLift","highlightPullBack","shadowPullBack"},QStringLiteral("placement"));
        auto& placed=out.placement;
        placed.active=boolean(p,"active");placed.norm=string(p,"norm").toStdString();
        require(placed.norm=="power"||placed.norm=="y"||placed.norm=="max",QStringLiteral("Unsupported scene-placement norm"));
        placed.highlight_knee=number(p,"highlightKnee",-24,24);placed.highlight_room=number(p,"highlightRoom",0,24);
        placed.shadow_knee=number(p,"shadowKnee",-24,24);placed.shadow_room=number(p,"shadowRoom",0,24);
        placed.rolloff=number(p,"rolloff",1,4);placed.max_lift=number(p,"maxLift",.25,12);
        placed.highlight_pull_back=number(p,"highlightPullBack",0,8);placed.shadow_pull_back=number(p,"shadowPullBack",0,8);
    }
    const auto mode = string(value, "decodeMode");
    require(mode == "compatible16" || mode == "headroom",
            QStringLiteral("Saved edits contain an unsupported RAW decode mode"));
    state.mode = mode == "headroom" ? DecodeMode::headroom : DecodeMode::compatible16;
    const auto g = object(value, "geometry");
    keys(g, {"crop", "quarterTurns", "flipHorizontal", "flipVertical", "straightenDegrees"},
         QStringLiteral("geometry"));
    const auto crop = object(g, "crop");
    keys(crop, {"x", "y", "width", "height"}, QStringLiteral("crop"));
    const double x = number(crop, "x", 0.0, 1.0), y = number(crop, "y", 0.0, 1.0);
    const double width = number(crop, "width", 0.0, 1.0), height = number(crop, "height", 0.0, 1.0);
    require(width > 0 && height > 0 && x + width <= 1.0 && y + height <= 1.0,
            QStringLiteral("Saved crop is empty or outside the image"));
    state.geometry.crop = QRectF(x, y, width, height);
    const double turns = number(g, "quarterTurns", 0.0, 3.0);
    require(std::floor(turns) == turns, QStringLiteral("Saved quarter turns must be an integer"));
    state.geometry.quarterTurns = int(turns);
    state.geometry.flipHorizontal = boolean(g, "flipHorizontal");
    state.geometry.flipVertical = boolean(g, "flipVertical");
    state.geometry.straightenDegrees = number(g, "straightenDegrees", -45.0, 45.0);
    if(value.contains("develop")) {
        const auto d=object(value,"develop");
        const bool extended=d.contains("workingSpace");
        if(extended)keys(d,{"enabled","temperature","tint","exposure","contrast","brightness","saturation",
                "highlights","shadows","blackPoint","whitePoint","workingSpace","curves","zones","vignetteAmount","vignetteMidpoint"},QStringLiteral("develop"));
        else keys(d,{"enabled","temperature","tint","exposure","contrast","brightness","saturation",
                "highlights","shadows","blackPoint","whitePoint"},QStringLiteral("develop"));
        auto& p=state.develop;
        p.enabled=boolean(d,"enabled");p.temperature=number(d,"temperature",-100,100);p.tint=number(d,"tint",-100,100);
        p.exposure=number(d,"exposure",-3,3);p.contrast=number(d,"contrast",-50,50);p.brightness=number(d,"brightness",-50,50);
        p.saturation=number(d,"saturation",-100,100);p.highlights=number(d,"highlights",-100,100);p.shadows=number(d,"shadows",-100,100);
        p.blackPoint=number(d,"blackPoint",0,50);p.whitePoint=number(d,"whitePoint",0,50);
        p.legacySrgb=!extended;
        if(extended) {
            const auto space=string(d,"workingSpace");
            require(space=="sRGB"||space=="ProPhoto RGB",QStringLiteral("Unknown saved working space"));
            p.legacySrgb=space=="sRGB";
            p.vignetteAmount=number(d,"vignetteAmount",-100,100);p.vignetteMidpoint=number(d,"vignetteMidpoint",0,100);
            require(d["curves"].isArray()&&d["curves"].toArray().size()==5&&d["zones"].isArray()&&d["zones"].toArray().size()==4,QStringLiteral("Invalid saved curve/zone count"));
            const auto curves=d["curves"].toArray(),zones=d["zones"].toArray();
            for(int i=0;i<5;++i) {
                require(curves[i].isArray(),QStringLiteral("Curve must be an array"));p.curves[i].clear();
                for(const auto point:curves[i].toArray()) {
                    require(point.isObject(),QStringLiteral("Curve point must be an object"));const auto v=point.toObject();
                    keys(v,{"x","y"},QStringLiteral("curve point"));p.curves[i].push_back({number(v,"x",0,1),number(v,"y",0,1)});
                }
            }
            for(int i=0;i<4;++i) {
                require(zones[i].isObject(),QStringLiteral("Zone must be an object"));const auto z=zones[i].toObject();
                keys(z,{"hue","saturation","luminance"},QStringLiteral("zone"));
                p.zones[i]={number(z,"hue",0,360),number(z,"saturation",0,1),number(z,"luminance",-1,1)};
            }
        }
        output::validateDevelop(p);
    }
    return state;
}

struct Identity {
    QString path;
    qint64 size = 0, mtime = 0;
    QString sample;
    bool operator==(const Identity&) const = default;
};
Identity identity(const QString& source) {
    const QFileInfo before(source);
    require(before.exists() && before.isFile(), QStringLiteral("Cannot find source while checking saved edits"));
    QFile file(source);
    require(file.open(QIODevice::ReadOnly), QStringLiteral("Cannot read source while checking saved edits: ") + file.errorString());
    Identity result{pathIdentity(source), before.size(), before.lastModified().toMSecsSinceEpoch(), {}};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    const auto first = file.read(std::min(result.size, sampleBytes));
    require(first.size() == std::min(result.size, sampleBytes), QStringLiteral("Cannot fingerprint source for saved edits"));
    hash.addData(first);
    if (result.size > sampleBytes) {
        const qint64 tail = std::min(sampleBytes, result.size - sampleBytes);
        require(file.seek(result.size - tail), QStringLiteral("Cannot seek source while checking saved edits"));
        const auto last = file.read(tail);
        require(last.size() == tail, QStringLiteral("Cannot fingerprint the end of the source"));
        hash.addData(last);
    }
    const QFileInfo after(source);
    require(after.exists() && after.size() == result.size &&
                after.lastModified().toMSecsSinceEpoch() == result.mtime,
            QStringLiteral("Source changed while checking saved edits; try again when copying has finished"));
    result.sample = QString::fromLatin1(hash.result().toHex());
    return result;
}
QJsonObject identityJson(const Identity& value) {
    // Decimal strings keep 64-bit timestamps and file sizes exact in JSON.
    return {{"path", value.path}, {"size", QString::number(value.size)},
            {"mtimeMs", QString::number(value.mtime)}, {"sampleSha256", value.sample}};
}
Identity parseIdentity(const QJsonObject& value) {
    keys(value, {"path", "size", "mtimeMs", "sampleSha256"}, QStringLiteral("source"));
    Identity result;
    result.path = string(value, "path");
    require(!result.path.isEmpty() && QDir::isAbsolutePath(result.path),
            QStringLiteral("Saved edits have an invalid source path"));
    bool ok = false;
    const QString size = string(value, "size");
    result.size = size.toLongLong(&ok);
    require(ok && result.size >= 0 && QString::number(result.size) == size,
            QStringLiteral("Saved edits have an invalid source size"));
    const QString mtime = string(value, "mtimeMs");
    result.mtime = mtime.toLongLong(&ok);
    require(ok && QString::number(result.mtime) == mtime,
            QStringLiteral("Saved edits have an invalid source timestamp"));
    result.sample = string(value, "sampleSha256");
    static const QRegularExpression hex(QStringLiteral("^[0-9a-f]{64}$"));
    require(hex.match(result.sample).hasMatch(), QStringLiteral("Saved edits have an invalid source fingerprint"));
    return result;
}
QByteArray fingerprint(const Identity& value) {
    return QCryptographicHash::hash(QJsonDocument(identityJson(value)).toJson(QJsonDocument::Compact),
                                    QCryptographicHash::Sha256);
}
bool recordExists(const QString& path) {
    const QFileInfo info(path);
    return info.exists() || info.isSymLink();
}
QJsonObject readDocument(const QString& path, QByteArray* digest = nullptr) {
    const QFileInfo info(path);
    require(!info.isSymLink() && info.isFile(), QStringLiteral("Saved edit path is not a regular file"));
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), QStringLiteral("Cannot read saved edits: ") + file.errorString());
    require(file.size() <= maximumDocumentBytes, QStringLiteral("Saved edit document exceeds the 64 KiB limit"));
    const auto bytes = file.read(maximumDocumentBytes + 1);
    require(bytes.size() == file.size() && bytes.size() <= maximumDocumentBytes,
            QStringLiteral("Cannot read the complete saved edit document"));
    if (digest) *digest = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(bytes, &error);
    require(error.error == QJsonParseError::NoError && document.isObject(),
            QStringLiteral("Saved edits contain invalid JSON: ") + error.errorString());
    const auto result = document.object();
    keys(result, {"format", "version", "source", "state"}, QStringLiteral("document"));
    require(string(result, "format") == "SpektraLab.Windows.Editor",
            QStringLiteral("Saved edits use an unsupported document format"));
    const auto version = result.value("version");
    require(version.isDouble() && (version.toDouble() == 1.0 || version.toDouble() == 2.0 || version.toDouble() == 3.0 || version.toDouble()==4.0 || version.toDouble()==5.0 || version.toDouble()==6.0 || version.toDouble()==7.0 || version.toDouble()==8.0),
            QStringLiteral("Saved edits use an unsupported version; existing file was preserved"));
    require(object(result,"state").contains("develop") == (version.toDouble()>=2.0),
            QStringLiteral("Saved edit fields do not match their document version"));
    if(version.toDouble()>=2)require(object(object(result,"state"),"develop").contains("workingSpace")== (version.toDouble()>=3),QStringLiteral("Saved develop fields do not match document version"));
    require(object(result,"state").contains("placement")== (version.toDouble()>=4),QStringLiteral("Saved placement fields do not match document version"));
    require(object(result,"state").contains("filmEdge")== (version.toDouble()>=5)&&object(result,"state").contains("dateBack")== (version.toDouble()>=5),QStringLiteral("Saved film/date fields do not match document version"));
    require(object(result,"state").contains("halfPair")== (version.toDouble()>=6),QStringLiteral("Saved pair fields do not match document version"));
    require(object(result,"state").contains("materialVersions")== (version.toDouble()>=7),QStringLiteral("Saved material fields do not match document version"));
    if(version.toDouble()>=7)require(object(object(result,"state"),"materialVersions").contains("engineResourcesSha256")== (version.toDouble()>=8),QStringLiteral("Saved resource context does not match document version"));
    return result;
}
State checkDocument(const QJsonObject& document, const Identity& source, const Catalog* catalog) {
    const auto stored = parseIdentity(object(document, "source"));
    require(stored.path == source.path, QStringLiteral("Saved edits belong to a different source; existing file was preserved"));
    require(stored == source, QStringLiteral("The source file changed since these edits were saved; existing edits were preserved"));
    return parseState(object(document, "state"), catalog);
}
} // namespace

Store::Store(QString directory) : directory_(QDir::cleanPath(directory)) {
    require(!directory.isEmpty() && QDir::isAbsolutePath(directory),
            QStringLiteral("Saved edit store requires an explicit absolute directory"));
}
void Store::updateCatalog(const Catalog& catalogue)const{for(auto& observation:observations_)observation.catalog=catalogue;}
QString Store::filePath(const QUrl& source) const {
    const auto digest = QCryptographicHash::hash(pathIdentity(sourcePath(source)).toUtf8(),
                                                QCryptographicHash::Sha256).toHex();
    return QDir(directory_).filePath(QString::fromLatin1(digest) + ".json");
}
LoadResult Store::load(const QUrl& source, const Catalog& catalog) const {
    QString key;
    try {
        key = observationKey(source);
        const auto path = filePath(source);
        const auto sourceId = identity(sourcePath(source));
        if (!recordExists(path)) {
            observations_.insert(key, {fingerprint(sourceId), {}, {}, catalog});
            return {};
        }
        QByteArray recordDigest;
        const auto document = readDocument(path, &recordDigest);
        observations_.insert(key, {fingerprint(sourceId), recordDigest, {}, catalog});
        return {checkDocument(document, sourceId, &catalog), {}, false};
    } catch (const std::exception& error) {
        const auto warning = QString::fromUtf8(error.what());
        if (!key.isEmpty()) {
            auto& observation = observations_[key];
            observation.warning = warning;
            observation.catalog = catalog;
        }
        return {{}, warning, true,warning.contains("unavailable stock")||warning.contains("Material version mismatch")};
    }
}
void Store::save(const QUrl& source, const State& state) const {
    const auto key = observationKey(source);
    const auto path = filePath(source);
    const auto observed = observations_.constFind(key);
    if (observed != observations_.cend() && !observed->warning.isEmpty()) fail(observed->warning);
    const auto value = stateJson(state);
    const auto catalog = observed != observations_.cend() ? observed->catalog : std::optional<Catalog>{};
    // Root callers load with a current catalogue first. Direct save callers
    // have no catalogue, but still receive complete shape/range validation.
    (void)parseState(value, catalog ? &*catalog : nullptr);
    const auto sourceFile = sourcePath(source);
    const auto sourceId = identity(sourceFile);
    if (observed != observations_.cend())
        require(observed->source == fingerprint(sourceId),
                QStringLiteral("The source file changed after it was opened; edits remain unsaved"));
    require(pathIdentity(QFileInfo(sourceFile).absolutePath()) !=
                pathIdentity(QFileInfo(directory_).canonicalFilePath().isEmpty()
                    ? directory_ : QFileInfo(directory_).canonicalFilePath()),
            QStringLiteral("Saved edit store must not be beside the source photograph"));
    require(QDir().mkpath(directory_), QStringLiteral("Cannot create the saved edit directory"));
    QLockFile lock(path + ".lock");
    lock.setStaleLockTime(30000);
    require(lock.tryLock(0), QStringLiteral("Saved edits are being written by another process; try again"));
    QByteArray previousDigest;
    if (recordExists(path)) {
        const auto previous = readDocument(path, &previousDigest);
        (void)checkDocument(previous, sourceId, nullptr);
    }
    if (observed != observations_.cend())
        require(observed->record == previousDigest,
                QStringLiteral("Saved edits changed in another instance; restart the app and reopen the photo before saving"));
    const auto bytes = QJsonDocument(QJsonObject{{"format", "SpektraLab.Windows.Editor"},
        {"version", 8}, {"source", identityJson(sourceId)}, {"state", value}}).toJson(QJsonDocument::Indented);
    require(bytes.size() <= maximumDocumentBytes, QStringLiteral("Saved edit document exceeds the 64 KiB limit"));
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    require(file.open(QIODevice::WriteOnly), QStringLiteral("Cannot open saved edits for atomic writing: ") + file.errorString());
    require(file.write(bytes) == bytes.size(), QStringLiteral("Cannot write saved edits: ") + file.errorString());
    require(file.commit(), QStringLiteral("Cannot atomically save edits: ") + file.errorString());
    observations_.insert(key, {fingerprint(sourceId),
                                QCryptographicHash::hash(bytes, QCryptographicHash::Sha256), {}, catalog});
}

} // namespace spk::desktop::editing
