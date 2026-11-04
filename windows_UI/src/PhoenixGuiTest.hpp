// Independent real-window Phoenix gate. Opt-in test entry; no production defaults.
#pragma once
class PhoenixGuiTest final:public QObject {
 PreviewController& c;QQuickWindow& w;QString input,out;FrameCanvas* canvas;QJsonArray records;QImage first;int stage=0;bool done=false;int publications=0;
 QString hash(QImage i){i=i.convertToFormat(QImage::Format_RGBA8888);return QCryptographicHash::hash(QByteArrayView(reinterpret_cast<const char*>(i.constBits()),i.sizeInBytes()),QCryptographicHash::Sha256).toHex();}
 int index(QString id){auto f=c.films();for(int i=0;i<f.size();i++)if(f[i].toMap()["id"]==id)return i;throw std::runtime_error("missing Phoenix film");}
 void finish(bool ok,QString error={}){if(done)return;done=true;QFile f(out+"/phoenix-report.json");f.open(QIODevice::WriteOnly);f.write(QJsonDocument(QJsonObject{{"passed",ok},{"error",error},{"records",records},{"scope","actual QQuickWindow captures and controller; not physical display calibration"}}).toJson());QCoreApplication::exit(ok?0:1);}
 void settle(){QTimer::singleShot(250,this,[this]{if(done)return;if(c.busy()||c.dirty()){settle();return;}try{step();}catch(const std::exception& e){finish(false,e.what());}});}
 void require(bool v,const char* why){if(!v)throw std::runtime_error(why);}
 void record(QString name){require(canvas,"canvas missing");require(canvas->image()==c.previewImage(),"canvas not published buffer");auto shot=w.grabWindow();require(!shot.isNull()&&shot.save(out+"/"+name+".png"),"capture failed");const auto s=c.frame()->settings;records.append(QJsonObject{{"stage",name},{"film_id",QString::fromStdString(s.film_stock)},{"film_ev",s.film_exposure_ev},{"print_exposure",s.print_exposure},{"grain",s.grain_active},{"halation",s.halation_active},{"glare",s.glare_active},{"rendered_sha256",hash(c.previewImage())},{"original_sha256",hash(c.originalImage())},{"screenshot_sha256",hash(shot)},{"show_original",canvas->showOriginal()},{"publications",publications},{"status",c.status()}});}
 void step(){require(!c.failed(),c.status().toUtf8().constData());
  if(stage==0){require(c.frame()->settings.film_stock=="harman_phoenix_200_round5","wrong first film");first=c.previewImage();record("01-phoenix-I");w.setProperty("originalMode",true);stage++;settle();}
  else if(stage==1){require(canvas->showOriginal()&&canvas->beforeImage()==c.originalImage(),"original toggle buffer mismatch");require(c.previewImage()==first,"original toggle mutated render");record("02-original");w.setProperty("originalMode",false);stage++;settle();}
  else if(stage==2){require(!canvas->showOriginal(),"render toggle failed");record("03-rendered");stage++;c.setFilmIndex(index("harman_phoenix_ii_200_round5"));settle();}
  else if(stage==3){require(c.frame()->settings.film_stock=="harman_phoenix_ii_200_round5"&&c.previewImage()!=first,"film switch stale pixels");record("04-phoenix-II");stage++;c.setFilmIndex(index("harman_phoenix_200_round5"));settle();}
  else if(stage==4){require(c.previewImage()==first,"switch-back cache mismatch");record("05-switch-back");stage++;c.setFilmIndex(index("harman_phoenix_ii_200_round5"));c.apply();c.setFilmIndex(index("harman_phoenix_200_round5"));settle();}
  else {require(c.frame()->settings.film_stock=="harman_phoenix_200_round5"&&c.previewImage()==first,"retired task replaced final selection");record("06-final-after-retired-task");finish(true);}
 }
public:
 PhoenixGuiTest(PreviewController& controller,QQuickWindow& window,QString raw,QString output):c(controller),w(window),input(raw),out(output){canvas=w.findChild<FrameCanvas*>("frameCanvas");connect(&c,&PreviewController::frameChanged,this,[this]{publications++;});connect(&c,&PreviewController::initialized,this,[this](bool ok){try{require(ok,"init failed");c.setFilmIndex(index("harman_phoenix_200_round5"));c.openRaw(QUrl::fromLocalFile(input));settle();}catch(const std::exception&e){finish(false,e.what());}});QTimer::singleShot(180000,this,[this]{finish(false,"Phoenix GUI timeout");});}
};
