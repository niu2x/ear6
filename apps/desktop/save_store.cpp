#include "save_store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>
#include <limits>
#include <utility>

namespace {

QString state_display_name(const Ear6StateInfo& info) {
    if (info.content_name_hint && info.content_name_hint_size > 0
        && info.content_name_hint_size
            <= static_cast<size_t>(std::numeric_limits<int>::max())) {
        return QString::fromUtf8(
            info.content_name_hint,
            static_cast<int>(info.content_name_hint_size)
        );
    }
    return QStringLiteral("System %1 / %2")
        .arg(static_cast<int>(info.system_type))
        .arg(info.content_identity, 16, 16, QLatin1Char('0'));
}

bool inspect_state(const QByteArray& state, Ear6StateInfo* info, QString* error) {
    if (state.isEmpty() || !info
        || ear6_get_state_info(state.constData(), static_cast<size_t>(state.size()), info) != 0) {
        if (error) *error = QStringLiteral("Invalid or unsupported Ear6 state");
        return false;
    }
    return true;
}

} // namespace

SaveStore::SaveStore(QString directory)
    : directory_(std::move(directory)) {
    if (directory_.isEmpty()) {
        directory_ = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
            .filePath(QStringLiteral("states"));
    }
}

QString SaveStore::get_directory() const {
    return directory_;
}

QString SaveStore::state_path(
    Ear6SystemType system_type,
    uint64_t content_identity
) const {
    QString key = QStringLiteral("%1-%2.e6s")
        .arg(static_cast<int>(system_type))
        .arg(content_identity, 16, 16, QLatin1Char('0'));
    return QDir(directory_).filePath(key);
}

bool SaveStore::save(const QByteArray& state, QString* path, QString* error) const {
    if (error) error->clear();
    Ear6StateInfo info = {};
    if (!inspect_state(state, &info, error)) return false;

    QDir directory;
    if (!directory.mkpath(directory_)) {
        if (error) *error = QStringLiteral("Unable to create save directory: %1").arg(directory_);
        return false;
    }

    QString destination = state_path(info.system_type, info.content_identity);
    QSaveFile file(destination);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    if (file.write(state) != state.size()) {
        if (error) *error = file.errorString();
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    if (path) *path = destination;
    return true;
}

QVector<SaveEntry> SaveStore::list(QString* error) const {
    if (error) error->clear();
    QVector<SaveEntry> entries;
    QDir directory(directory_);
    if (!directory.exists()) return entries;

    const QFileInfoList files = directory.entryInfoList(
        {QStringLiteral("*.e6s")},
        QDir::Files | QDir::Readable,
        QDir::Time
    );
    for (const QFileInfo& file_info : files) {
        QFile file(file_info.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly)) continue;
        QByteArray state = file.readAll();
        Ear6StateInfo info = {};
        if (!inspect_state(state, &info, nullptr)) continue;

        SaveEntry entry;
        entry.system_type = info.system_type;
        entry.content_identity = info.content_identity;
        entry.display_name = state_display_name(info);
        entry.path = file_info.absoluteFilePath();
        entry.saved_at = file_info.lastModified();
        if (info.preview_format == EAR6_STATE_PREVIEW_RGBA8888
            && info.preview_data && info.preview_width > 0 && info.preview_height > 0
            && info.preview_width <= std::numeric_limits<int>::max() / 4) {
            entry.preview = QImage(
                info.preview_data,
                info.preview_width,
                info.preview_height,
                info.preview_width * 4,
                QImage::Format_RGBA8888
            ).copy();
        }
        entries.push_back(std::move(entry));
    }

    if (entries.isEmpty() && !files.isEmpty() && error) {
        *error = QStringLiteral("No valid .e6s files in %1").arg(directory_);
    }
    std::sort(entries.begin(), entries.end(), [](const SaveEntry& left, const SaveEntry& right) {
        return left.saved_at > right.saved_at;
    });
    return entries;
}

bool SaveStore::load(const SaveEntry& entry, QByteArray* state, QString* error) const {
    if (error) error->clear();
    if (!state) {
        if (error) *error = QStringLiteral("Missing output buffer");
        return false;
    }

    QFile file(entry.path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    QByteArray candidate = file.readAll();
    Ear6StateInfo info = {};
    if (!inspect_state(candidate, &info, error)
        || info.system_type != entry.system_type
        || info.content_identity != entry.content_identity) {
        if (error && error->isEmpty()) *error = QStringLiteral("Save identity mismatch");
        return false;
    }
    *state = std::move(candidate);
    return true;
}
