#pragma once

#include "emulator_widget.h"

#include <QMainWindow>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(Ear6SystemType system, QWidget* parent = nullptr);

private:
    void create_menus();
    void on_start();
    void on_stop();
    void on_reset();

    EmulatorWidget* emulator_ = nullptr;
    QAction* start_action_ = nullptr;
    QAction* stop_action_ = nullptr;
};
