#include "emulator_widget.h"

#include <QPainter>
#include <QPaintEvent>

EmulatorWidget::EmulatorWidget(Ear6SystemType system, QWidget* parent)
    : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(256, 240);

    ctx_ = ear6_create(system);
    if (!ctx_) {
        return;
    }

    ear6_load(ctx_, nullptr, 0);

    timer_ = new QTimer(this);
    timer_->setInterval(TIMER_INTERVAL_MS);
    connect(timer_, &QTimer::timeout, this, &EmulatorWidget::on_step);
}

EmulatorWidget::~EmulatorWidget() {
    if (ctx_) {
        ear6_destroy(ctx_);
    }
}

void EmulatorWidget::start() {
    if (!ctx_ || running_) {
        return;
    }
    running_ = true;
    timer_->start();
}

void EmulatorWidget::stop() {
    if (!running_) {
        return;
    }
    running_ = false;
    timer_->stop();
}

void EmulatorWidget::reset(Ear6SystemType system) {
    stop();
    if (ctx_) {
        ear6_destroy(ctx_);
    }
    ctx_ = ear6_create(system);
    if (ctx_) {
        ear6_load(ctx_, nullptr, 0);
    }
    frame_image_ = QImage();
    update();
}

bool EmulatorWidget::is_running() const {
    return running_;
}

void EmulatorWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);

    if (frame_image_.isNull()) {
        painter.fillRect(rect(), Qt::black);
        painter.setPen(Qt::gray);
        painter.drawText(rect(), Qt::AlignCenter, tr("No frame data"));
        return;
    }

    QImage scaled = frame_image_.scaled(size(), Qt::KeepAspectRatio, Qt::FastTransformation);
    int x = (width() - scaled.width()) / 2;
    int y = (height() - scaled.height()) / 2;
    painter.drawImage(x, y, scaled);
}

void EmulatorWidget::keyPressEvent(QKeyEvent* event) {
    if (event->isAutoRepeat()) {
        return;
    }
    QWidget::keyPressEvent(event);
}

void EmulatorWidget::keyReleaseEvent(QKeyEvent* event) {
    if (event->isAutoRepeat()) {
        return;
    }
    QWidget::keyReleaseEvent(event);
}

void EmulatorWidget::on_step() {
    if (!ctx_) {
        return;
    }

    ear6_step(ctx_);

    const uint8_t* fb = ear6_get_framebuffer(ctx_);
    int w = ear6_get_frame_width(ctx_);
    int h = ear6_get_frame_height(ctx_);

    if (fb && w > 0 && h > 0) {
        frame_image_ = QImage(fb, w, h, w * 4, QImage::Format_RGB32).copy();
        update();
    }
}
