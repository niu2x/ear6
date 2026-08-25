#include "emulator_widget.h"

#include <QPainter>
#include <QPaintEvent>

#include <limits>

EmulatorWidget::EmulatorWidget(Ear6SystemType system, const QString& rom_path, QWidget* parent)
    : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(256, 240);

    timer_ = new QTimer(this);
    timer_->setInterval(TIMER_INTERVAL_MS);
    connect(timer_, &QTimer::timeout, this, &EmulatorWidget::on_step);
    reset(system, rom_path);
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

bool EmulatorWidget::reset(Ear6SystemType system, const QString& rom_path) {
    Ear6* candidate = ear6_create(system);
    if (!candidate) return false;

    int result = rom_path.isEmpty()
        ? ear6_load_from_memory(candidate, nullptr, 0, nullptr)
        : ear6_load_from_file(candidate, rom_path.toUtf8().constData());
    if (result != 0) {
        ear6_destroy(candidate);
        if (!rom_path.isEmpty()) emit load_failed(rom_path);
        return false;
    }

    stop();
    if (ctx_) ear6_destroy(ctx_);
    ctx_ = candidate;
    has_content_ = true;
    frame_image_ = QImage();
    update();
    return true;
}

bool EmulatorWidget::save_state(QByteArray* state) const {
    if (!ctx_ || !has_content_ || !state) return false;

    size_t state_size = 0;
    if (ear6_save_state_to_memory(ctx_, nullptr, 0, &state_size) != 0
        || state_size > static_cast<size_t>(std::numeric_limits<qsizetype>::max())) {
        return false;
    }
    state->resize(static_cast<qsizetype>(state_size));
    if (ear6_save_state_to_memory(
            ctx_, state->data(), static_cast<size_t>(state->size()), &state_size) != 0) {
        state->clear();
        return false;
    }
    state->resize(static_cast<qsizetype>(state_size));
    return true;
}

bool EmulatorWidget::load_state(Ear6SystemType system, const QByteArray& state) {
    if (state.isEmpty()) return false;
    Ear6* candidate = ear6_create(system);
    if (!candidate) return false;
    if (ear6_load_state_from_memory(
            candidate, state.constData(), static_cast<size_t>(state.size())) != 0) {
        ear6_destroy(candidate);
        return false;
    }

    stop();
    if (ctx_) ear6_destroy(ctx_);
    ctx_ = candidate;
    has_content_ = true;
    update_frame();
    return true;
}

bool EmulatorWidget::is_running() const {
    return running_;
}

bool EmulatorWidget::has_content() const {
    return has_content_;
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

    if (ear6_step(ctx_) != 0) return;
    update_frame();
}

void EmulatorWidget::update_frame() {
    if (!ctx_) return;

    const uint8_t* fb = ear6_get_framebuffer(ctx_);
    int w = ear6_get_frame_width(ctx_);
    int h = ear6_get_frame_height(ctx_);

    if (fb && w > 0 && h > 0) {
        frame_image_ = QImage(fb, w, h, w * 4, QImage::Format_RGB32).copy();
        update();
    }
}
