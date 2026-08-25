#pragma once

#include <ear6/ear6.h>

#include <QByteArray>
#include <QDateTime>
#include <QImage>
#include <QString>
#include <QVector>

struct SaveEntry {
    Ear6SystemType system_type = EAR6_SYSTEM_TEST;
    uint64_t content_identity = 0;
    QString display_name;
    QString path;
    QDateTime saved_at;
    QImage preview;
};

class SaveStore {
public:
    explicit SaveStore(QString directory = {});

    QString get_directory() const;
    bool save(const QByteArray& state, QString* path, QString* error) const;
    QVector<SaveEntry> list(QString* error = nullptr) const;
    bool load(const SaveEntry& entry, QByteArray* state, QString* error) const;

private:
    QString state_path(Ear6SystemType system_type, uint64_t content_identity) const;

    QString directory_;
};
