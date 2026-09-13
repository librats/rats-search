#include "migrationprogresswindow.h"

#include "services/migration_service.h"
#include "theme.h"

#include <QApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QLabel>
#include <QLocale>
#include <QPixmap>
#include <QProgressBar>
#include <QScreen>
#include <QVBoxLayout>

namespace {
// Minimum gap between repaints. A page of the row-id sweep is ~500 rows, which
// on a warm index is a few milliseconds — repainting on every one of them would
// make the progress window the slowest part of the migration.
constexpr qint64 kRepaintIntervalMs = 100;
} // namespace

MigrationProgressWindow::MigrationProgressWindow(
    rats::service::MigrationService* migrations, bool darkMode, QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    applyTheme(darkMode);

    if (!migrations)
        return;

    connect(migrations, &rats::service::MigrationService::syncMigrationStarted, this,
        &MigrationProgressWindow::onMigrationStarted);
    connect(
        migrations, &rats::service::MigrationService::migrationProgress, this, &MigrationProgressWindow::onProgress);
    connect(migrations, &rats::service::MigrationService::syncMigrationsFinished, this,
        &MigrationProgressWindow::onFinished);
}

void MigrationProgressWindow::setupUi()
{
    // A frameless, always-on-top splash: there is no event loop behind it yet,
    // so a real window frame would offer buttons that cannot be serviced.
    // WA_QuitOnClose is cleared so hiding it at the end of the migration cannot
    // be mistaken for the last window closing.
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_QuitOnClose, false);
    setWindowTitle(tr("Rats Search"));
    setWindowIcon(QIcon(":/images/icon.png"));
    setObjectName(QStringLiteral("migrationSplash"));
    setFixedSize(460, 210);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(10);

    auto* logo = new QLabel(this);
    const QPixmap icon(":/images/icon.png");
    if (!icon.isNull())
        logo->setPixmap(icon.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logo->setAlignment(Qt::AlignCenter);
    layout->addWidget(logo);

    headingLabel_ = new QLabel(tr("Updating the database…"), this);
    headingLabel_->setObjectName(QStringLiteral("migrationHeading"));
    headingLabel_->setAlignment(Qt::AlignCenter);
    layout->addWidget(headingLabel_);

    detailLabel_ = new QLabel(QString(), this);
    detailLabel_->setObjectName(QStringLiteral("migrationDetail"));
    detailLabel_->setAlignment(Qt::AlignCenter);
    detailLabel_->setWordWrap(true);
    layout->addWidget(detailLabel_);

    progressBar_ = new QProgressBar(this);
    progressBar_->setRange(0, 0); // busy indicator until a total is known
    progressBar_->setTextVisible(true);
    layout->addWidget(progressBar_);

    hintLabel_ = new QLabel(tr("This runs once after an update. Please don't close the application."), this);
    hintLabel_->setObjectName(QStringLiteral("migrationHint"));
    hintLabel_->setAlignment(Qt::AlignCenter);
    hintLabel_->setWordWrap(true);
    layout->addWidget(hintLabel_);

    layout->addStretch();
}

void MigrationProgressWindow::applyTheme(bool darkMode)
{
    // Same sheet MainWindow uses, so the splash matches the window that follows
    // it; the frameless edge and the three text styles live in theme.qss under
    // #migrationSplash and friends.
    rats::ui::Theme::instance().setDark(darkMode);
    setStyleSheet(rats::ui::Theme::instance().styleSheet());
}

void MigrationProgressWindow::onMigrationStarted(const QString& migrationId, const QString& description)
{
    Q_UNUSED(migrationId);
    detailLabel_->setText(description);
    progressBar_->setRange(0, 0); // unknown length until the first tick with a total
    progressBar_->setFormat(QStringLiteral("%p%"));

    if (!isVisible()) {
        if (QScreen* screen = QGuiApplication::primaryScreen())
            move(screen->availableGeometry().center() - rect().center());
        show();
        raise();
        activateWindow();
    }
    pumpEvents(true);
}

void MigrationProgressWindow::onProgress(const QString& migrationId, qint64 current, qint64 total)
{
    Q_UNUSED(migrationId);
    if (!isVisible())
        return;

    if (total > 0) {
        // Scaled to permille: a QProgressBar range is int, and a multi-million
        // row index overflows it.
        const int value = static_cast<int>((qMin(current, total) * 1000) / total);
        if (progressBar_->maximum() != 1000)
            progressBar_->setRange(0, 1000);
        progressBar_->setValue(value);
        progressBar_->setFormat(
            tr("%1 of %2 (%3%)").arg(QLocale().toString(current), QLocale().toString(total)).arg(value / 10));
    } else if (progressBar_->maximum() != 0) {
        progressBar_->setRange(0, 0);
    }

    pumpEvents();
}

void MigrationProgressWindow::onFinished()
{
    // hide(), not close(): closing the last visible window before exec() starts
    // is a quit trigger under some styles.
    hide();
    pumpEvents(true);
}

void MigrationProgressWindow::pumpEvents(bool force)
{
    if (!force && repaintClock_.isValid() && repaintClock_.elapsed() < kRepaintIntervalMs)
        return;
    repaintClock_.restart();
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 20);
}
