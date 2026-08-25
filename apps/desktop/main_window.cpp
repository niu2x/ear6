#include "main_window.h"

#include <QAction>
#include <QFileDialog>
#include <QFileInfo>
#include <QIcon>
#include <QLocale>
#include <QMenuBar>
#include <QPixmap>
#include <QStatusBar>

MainWindow::MainWindow(Ear6SystemType system, const QString& rom_path, QWidget* parent)
    : QMainWindow(parent), system_(system), current_rom_path_(rom_path) {
    resize(640, 480);

    emulator_ = new EmulatorWidget(system_, rom_path, this);
    setCentralWidget(emulator_);

    connect(emulator_, &EmulatorWidget::load_failed, this, [this](const QString& path) {
        statusBar()->showMessage(tr("Failed to load: %1").arg(path), 5000);
    });

    create_menus();
    update_title(rom_path);

    const bool has_content = emulator_->has_content();
    save_action_->setEnabled(has_content);
    start_action_->setEnabled(has_content);
    reset_action_->setEnabled(has_content);

    if (!rom_path.isEmpty() && !has_content) {
        statusBar()->showMessage(tr("Failed to load: %1").arg(rom_path), 5000);
    } else {
        statusBar()->showMessage(tr("Ready"));
    }
}

void MainWindow::create_menus() {
    QMenu* file_menu = menuBar()->addMenu(tr("&File"));

    QAction* open_action = file_menu->addAction(tr("&Open ROM..."));
    open_action->setShortcut(QKeySequence::Open);
    connect(open_action, &QAction::triggered, this, &MainWindow::on_open_rom);

    save_action_ = file_menu->addAction(tr("&Save"));
    save_action_->setShortcut(QKeySequence::Save);
    save_action_->setEnabled(false);
    connect(save_action_, &QAction::triggered, this, &MainWindow::on_save);

    load_save_menu_ = file_menu->addMenu(tr("&Load Save"));
    connect(load_save_menu_, &QMenu::aboutToShow, this, &MainWindow::refresh_load_save_menu);

    file_menu->addSeparator();

    QAction* quit_action = file_menu->addAction(tr("&Quit"));
    quit_action->setShortcut(QKeySequence::Quit);
    connect(quit_action, &QAction::triggered, this, &QWidget::close);

    QMenu* emulation_menu = menuBar()->addMenu(tr("&Emulation"));

    start_action_ = emulation_menu->addAction(tr("Start"));
    connect(start_action_, &QAction::triggered, this, &MainWindow::on_start);

    stop_action_ = emulation_menu->addAction(tr("Pause"));
    stop_action_->setEnabled(false);
    connect(stop_action_, &QAction::triggered, this, &MainWindow::on_stop);

    emulation_menu->addSeparator();

    reset_action_ = emulation_menu->addAction(tr("Reset"));
    connect(reset_action_, &QAction::triggered, this, &MainWindow::on_reset);

    QMenu* help_menu = menuBar()->addMenu(tr("&Help"));
    QAction* about_action = help_menu->addAction(tr("&About"));
    connect(about_action, &QAction::triggered, this, [this]() {
        statusBar()->showMessage(tr("Ear6 Emulator - v0.1.0"), 3000);
    });
}

void MainWindow::update_title(const QString& rom_path) {
    if (rom_path.isEmpty()) {
        setWindowTitle(tr("Ear6 Emulator"));
    } else {
        setWindowTitle(tr("Ear6 Emulator - %1").arg(rom_path));
    }
}

void MainWindow::on_open_rom() {
    QString path = QFileDialog::getOpenFileName(
        this, tr("Open ROM"),
        {}, tr("ROM files (*.nes *.bin);;All files (*)"));

    if (path.isEmpty()) {
        return;
    }

    emulator_->stop();
    system_ = (path.endsWith(".nes", Qt::CaseInsensitive)) ? EAR6_SYSTEM_NES : EAR6_SYSTEM_TEST;
    if (!emulator_->reset(system_, path)) {
        statusBar()->showMessage(tr("Failed to load: %1").arg(path), 5000);
        return;
    }
    current_rom_path_ = path;
    save_action_->setEnabled(true);
    reset_action_->setEnabled(true);
    start_action_->setEnabled(true);
    stop_action_->setEnabled(false);
    update_title(path);
    statusBar()->showMessage(tr("Loaded: %1").arg(path));
}

void MainWindow::on_start() {
    if (!emulator_) {
        return;
    }
    emulator_->start();
    start_action_->setEnabled(false);
    stop_action_->setEnabled(true);
    statusBar()->showMessage(tr("Running"));
}

void MainWindow::on_stop() {
    if (!emulator_ || !emulator_->is_running()) {
        return;
    }
    emulator_->stop();
    start_action_->setEnabled(true);
    stop_action_->setEnabled(false);
    statusBar()->showMessage(tr("Paused"));
}

void MainWindow::on_reset() {
    if (!emulator_) {
        return;
    }
    if (!emulator_->reset(system_, current_rom_path_)) {
        statusBar()->showMessage(tr("Reset failed"), 5000);
        return;
    }
    start_action_->setEnabled(true);
    stop_action_->setEnabled(false);
    statusBar()->showMessage(tr("Reset"));
}

void MainWindow::on_save() {
    if (!emulator_) return;

    QByteArray state;
    if (!emulator_->save_state(&state)) {
        statusBar()->showMessage(tr("Save failed"), 5000);
        return;
    }

    QString path;
    QString error;
    if (!save_store_.save(state, &path, &error)) {
        statusBar()->showMessage(tr("Save failed: %1").arg(error), 5000);
        return;
    }
    statusBar()->showMessage(tr("Saved: %1").arg(path), 5000);
}

void MainWindow::refresh_load_save_menu() {
    load_save_menu_->clear();
    QString error;
    QVector<SaveEntry> entries = save_store_.list(&error);
    if (entries.isEmpty()) {
        QAction* empty_action = load_save_menu_->addAction(
            error.isEmpty() ? tr("No saves") : error
        );
        empty_action->setEnabled(false);
        return;
    }

    for (const SaveEntry& entry : entries) {
        QIcon icon;
        if (!entry.preview.isNull()) {
            icon = QIcon(QPixmap::fromImage(entry.preview.scaled(
                96, 90, Qt::KeepAspectRatio, Qt::FastTransformation
            )));
        }
        QAction* action = load_save_menu_->addAction(icon, entry.display_name);
        action->setToolTip(tr("%1\nSaved %2")
            .arg(entry.path, QLocale().toString(entry.saved_at, QLocale::ShortFormat)));
        connect(action, &QAction::triggered, this, [this, entry]() {
            load_save(entry);
        });
    }
}

void MainWindow::load_save(const SaveEntry& entry) {
    QByteArray state;
    QString error;
    if (!save_store_.load(entry, &state, &error)) {
        statusBar()->showMessage(tr("Load save failed: %1").arg(error), 5000);
        return;
    }
    if (!emulator_->load_state(entry.system_type, state)) {
        statusBar()->showMessage(tr("Load save failed"), 5000);
        return;
    }

    system_ = entry.system_type;
    current_rom_path_.clear();
    update_title(entry.display_name);
    save_action_->setEnabled(true);
    reset_action_->setEnabled(false);
    start_action_->setEnabled(true);
    stop_action_->setEnabled(false);
    statusBar()->showMessage(tr("Loaded save: %1").arg(entry.display_name), 5000);
}
