#include "favoriteswidget.h"
#include "app/application.h"
#include "app/favorites_store.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

FavoritesWidget::FavoritesWidget(QWidget* parent) : TorrentTableWidget(parent)
{
    setupControls();
}

void FavoritesWidget::setupControls()
{
    // Header row (above the table).
    QWidget* headerRow = new QWidget(this);
    headerRow->setObjectName("filterRow");
    QHBoxLayout* headerLayout = new QHBoxLayout(headerRow);
    headerLayout->setContentsMargins(12, 8, 12, 8);

    QLabel* titleLabel = new QLabel(tr("⭐ My Favorites"), this);
    titleLabel->setObjectName("sectionTitle");
    headerLayout->addWidget(titleLabel);

    headerLayout->addStretch();

    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName("statusLabel");
    headerLayout->addWidget(statusLabel_);

    removeButton_ = new QPushButton(tr("Remove from Favorites"), this);
    removeButton_->setObjectName("dangerButton");
    removeButton_->setCursor(Qt::PointingHandCursor);
    removeButton_->setEnabled(false);
    connect(removeButton_, &QPushButton::clicked, this, &FavoritesWidget::onRemoveClicked);
    headerLayout->addWidget(removeButton_);

    mainLayout_->insertWidget(0, headerRow);

    // Empty state label.
    emptyLabel_ = new QLabel(tr("No favorites yet.\n\nAdd torrents to favorites from the details panel\nor they will "
                                "be added automatically when you create or import a torrent."),
        this);
    emptyLabel_->setAlignment(Qt::AlignCenter);
    emptyLabel_->setObjectName("emptyStateLabel");
    emptyLabel_->setWordWrap(true);
    mainLayout_->insertWidget(1, emptyLabel_);
}

void FavoritesWidget::onApplicationSet()
{
    if (app_ && app_->favorites()) {
        connect(app_->favorites(), &rats::app::FavoritesStore::favoritesChanged, this, &FavoritesWidget::refresh);
    }
    refresh();
}

void FavoritesWidget::refresh()
{
    if (!app_ || !app_->favorites())
        return;

    const QVector<rats::app::FavoritesStore::Entry> entries = app_->favorites()->favorites();

    QVector<rats::domain::Torrent> torrents;
    torrents.reserve(entries.size());
    for (const rats::app::FavoritesStore::Entry& entry : entries) {
        if (entry.torrent.isValid()) {
            torrents.append(entry.torrent);
        }
    }

    setTorrents(torrents);

    const bool isEmpty = torrents.isEmpty();
    emptyLabel_->setVisible(isEmpty);
    tableView_->setVisible(!isEmpty);
    statusLabel_->setText(isEmpty ? QString() : tr("%1 favorites").arg(torrents.size()));
    removeButton_->setEnabled(false);
}

void FavoritesWidget::onSelectionChanged(const rats::domain::Torrent&, bool valid)
{
    removeButton_->setEnabled(valid);
}

QMenu* FavoritesWidget::buildContextMenu(const rats::domain::Torrent& torrent)
{
    QMenu* menu = TorrentTableWidget::buildContextMenu(torrent);
    if (!menu)
        return nullptr;

    const QList<QAction*> actions = menu->actions();
    QAction* before = actions.isEmpty() ? nullptr : actions.first();

    QAction* removeAction = new QAction(tr("❌ Remove from Favorites"), menu);
    connect(removeAction, &QAction::triggered, this, [this, torrent]() { removeFavorite(torrent.hash); });
    menu->insertAction(before, removeAction);
    menu->insertSeparator(before);

    return menu;
}

void FavoritesWidget::onRemoveClicked()
{
    const rats::domain::Torrent torrent = selectedTorrent();
    if (torrent.isValid()) {
        removeFavorite(torrent.hash);
    }
}

void FavoritesWidget::removeFavorite(const QString& hash)
{
    if (app_ && app_->favorites()) {
        app_->favorites()->remove(hash);
    }
}
