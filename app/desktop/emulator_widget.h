#pragma once

#include <ear6/ear6.h>
#include <QImage>
#include <QKeyEvent>
#include <QTimer>
#include <QWidget>

class EmulatorWidget : public QWidget {
    Q_OBJECT

public:
    explicit EmulatorWidget(Ear6SystemType system, const QString& rom_path = {}, QWidget* parent = nullptr);
    ~EmulatorWidget() override;

signals:
    void load_failed(const QString& path);

public:
    void start();
    void stop();
    void reset(Ear6SystemType system, const QString& rom_path = {});

    bool is_running() const;

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

private:
    void on_step();

    Ear6* ctx_ = nullptr;
    QTimer* timer_ = nullptr;
    QImage frame_image_;
    bool running_ = false;

    static constexpr int TARGET_FPS = 60;
    static constexpr int TIMER_INTERVAL_MS = 1000 / TARGET_FPS;
};
