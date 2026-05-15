#pragma once

#include "emulator_widget.h"

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

private:
    void create_menus();
    void update_title(const QString& rom_path = {});

    EmulatorWidget* emulator_ = nullptr;
    Ear6SystemType system_;
    QAction* start_action_ = nullptr;
    QAction* stop_action_ = nullptr;
};
