#include "save_store.h"

#include <ear6/ear6.h>
#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>

namespace {

QByteArray make_state(const QByteArray& content, const char* name, int steps = 0) {
    Ear6* ctx = ear6_create(EAR6_SYSTEM_TEST);
    if (!ctx) return {};
    if (ear6_load_from_memory(ctx, content.constData(), content.size(), name) != 0) {
        ear6_destroy(ctx);
        return {};
    }
    for (int i = 0; i < steps; ++i) {
        if (ear6_step(ctx) != 0) {
            ear6_destroy(ctx);
            return {};
        }
    }

    size_t size = 0;
    if (ear6_save_state_to_memory(ctx, nullptr, 0, &size) != 0) {
        ear6_destroy(ctx);
        return {};
    }
    QByteArray state;
    state.resize(static_cast<qsizetype>(size));
    if (ear6_save_state_to_memory(ctx, state.data(), size, &size) != 0) {
        state.clear();
    } else {
        state.resize(static_cast<qsizetype>(size));
    }
    ear6_destroy(ctx);
    return state;
}

TEST(SaveStore, DefaultDirectoryIsInsideApplicationData) {
    SaveStore store;
    const QString expected = QDir(
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
    ).filePath(QStringLiteral("states"));
    EXPECT_EQ(store.get_directory(), expected);
}

TEST(SaveStore, NewStateForSameContentAtomicallyReplacesOldState) {
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    SaveStore store(directory.path());

    const QByteArray first = make_state(QByteArrayLiteral("same-rom"), "Same Game.bin", 1);
    const QByteArray second = make_state(QByteArrayLiteral("same-rom"), "Same Game.bin", 2);
    ASSERT_FALSE(first.isEmpty());
    ASSERT_FALSE(second.isEmpty());
    ASSERT_NE(first, second);

    QString first_path;
    QString second_path;
    QString error;
    ASSERT_TRUE(store.save(first, &first_path, &error)) << error.toStdString();
    ASSERT_TRUE(store.save(second, &second_path, &error)) << error.toStdString();
    EXPECT_EQ(first_path, second_path);
    EXPECT_EQ(QDir(directory.path()).entryList({QStringLiteral("*.e6s")}, QDir::Files).size(), 1);

    const QVector<SaveEntry> entries = store.list(&error);
    ASSERT_EQ(entries.size(), 1) << error.toStdString();
    EXPECT_EQ(entries.front().display_name, QStringLiteral("Same Game.bin"));
    EXPECT_FALSE(entries.front().preview.isNull());

    QByteArray loaded;
    ASSERT_TRUE(store.load(entries.front(), &loaded, &error)) << error.toStdString();
    EXPECT_EQ(loaded, second);
}

TEST(SaveStore, DifferentContentHasOneIndependentStateEach) {
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    SaveStore store(directory.path());

    const QByteArray first = make_state(QByteArrayLiteral("rom-a"), "A.bin");
    const QByteArray second = make_state(QByteArrayLiteral("rom-b"), "B.bin");
    ASSERT_FALSE(first.isEmpty());
    ASSERT_FALSE(second.isEmpty());

    QString error;
    ASSERT_TRUE(store.save(first, nullptr, &error)) << error.toStdString();
    ASSERT_TRUE(store.save(second, nullptr, &error)) << error.toStdString();
    EXPECT_EQ(QDir(directory.path()).entryList({QStringLiteral("*.e6s")}, QDir::Files).size(), 2);
    EXPECT_EQ(store.list(&error).size(), 2) << error.toStdString();
}

TEST(SaveStore, InvalidStateIsRejectedWithoutCreatingAFile) {
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    SaveStore store(directory.path());

    QString error;
    EXPECT_FALSE(store.save(QByteArrayLiteral("not an e6s"), nullptr, &error));
    EXPECT_FALSE(error.isEmpty());
    EXPECT_TRUE(QDir(directory.path()).entryList(QDir::Files).isEmpty());
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Ear6"));
    QCoreApplication::setApplicationName(QStringLiteral("Ear6 Emulator"));
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
