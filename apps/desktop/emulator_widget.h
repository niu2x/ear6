#pragma once

#include <ear6/ear6.h>
#include <QByteArray>
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
    bool reset(Ear6SystemType system, const QString& rom_path = {});
    bool save_state(QByteArray* state) const;
    bool load_state(Ear6SystemType system, const QByteArray& state);

    bool is_running() const;
    bool has_content() const;

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

private:
    void on_step();
    void update_frame();

    Ear6* ctx_ = nullptr;
    QTimer* timer_ = nullptr;
    QImage frame_image_;
    bool running_ = false;
    bool has_content_ = false;

    static constexpr int TARGET_FPS = 60;
    static constexpr int TIMER_INTERVAL_MS = 1000 / TARGET_FPS;
};
