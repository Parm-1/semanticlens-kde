#include "/home/parm/Documents/KDE Index/src/core-v3/engine.cpp"
#include <iostream>
int main(int argc,char **argv){
 QCoreApplication app(argc,argv);if(argc!=2)return 2;
 QString root=QString::fromUtf8(argv[1]);QDir().mkpath(root);QString path=root+"/source.txt",moved=root+"/moved.txt";
 auto write=[](QString path,QByteArray body){QFile f(path);if(!f.open(QIODevice::WriteOnly|QIODevice::Truncate)||f.write(body)!=body.size())throw std::runtime_error("fixture write failed");};
 QByteArray a="HELD-ORIGINAL-44231 actual descriptor source",b="REPLACEMENT-87319 distinct new inode source";
 write(path,a);auto held=localPath(path);struct stat old{};::fstat(held.value,&old);QByteArray hash=digestHeld(held);if(!QFile::rename(path,moved))return 3;write(path,b);
 auto reader=readableFd(held,false);QFile f;if(!f.open(reader.value,QIODevice::ReadOnly,QFileDevice::AutoCloseHandle))return 4;reader.release();auto body=f.readAll();f.close();auto current=localPath(path);struct stat replacement{};::fstat(current.value,&replacement);
 QJsonObject r{{"original_inode",double(old.st_ino)},{"replacement_inode",double(replacement.st_ino)},{"held_body",QString::fromUtf8(body)},{"replacement_body",QString::fromUtf8(b)},{"held_digest",QString::fromLatin1(hash)},{"held_after_rename_digest",QString::fromLatin1(digestHeld(held))},{"current_digest",QString::fromLatin1(digestHeld(current))},{"rename_preserves_held_source",body==a&&digestHeld(held)==hash},{"replacement_re_resolution_has_different_inode",old.st_ino!=replacement.st_ino}};
 write(moved,"MUTATED-HELD-55918 changed original inode");r["in_place_change_changes_held_digest"]=digestHeld(held)!=hash;r["mutated_held_digest"]=QString::fromLatin1(digestHeld(held));
 std::cout<<QJsonDocument(r).toJson().constData();
}
