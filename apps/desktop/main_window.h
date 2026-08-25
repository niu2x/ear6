#pragma once

#include "emulator_widget.h"
#include "save_store.h"

#include <QMainWindow>
#include <QString>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(Ear6SystemType system, const QString& rom_path = {}, QWidget* parent = nullptr);

private slots:
    void on_open_rom();
    void on_start();
    void on_stop();
    void on_reset();
    void on_save();
    void refresh_load_save_menu();

private:
    void create_menus();
    void update_title(const QString& rom_path = {});
    void load_save(const SaveEntry& entry);

    EmulatorWidget* emulator_ = nullptr;
    Ear6SystemType system_;
    SaveStore save_store_;
    QString current_rom_path_;
    QAction* start_action_ = nullptr;
    QAction* stop_action_ = nullptr;
    QAction* reset_action_ = nullptr;
    QAction* save_action_ = nullptr;
    QMenu* load_save_menu_ = nullptr;
};
