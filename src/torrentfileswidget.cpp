#include "torrentfileswidget.h"
#include <QStyle>
#include <algorithm>

TorrentFilesWidget::TorrentFilesWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

TorrentFilesWidget::~TorrentFilesWidget()
{
}

void TorrentFilesWidget::setupUi()
{
    setObjectName("torrentFilesWidget");
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 4, 8, 4);
    mainLayout->setSpacing(4);
    
    // Header with title and info
    QHBoxLayout* headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(12);
    
    titleLabel_ = new QLabel(tr("📁 Files"));
    titleLabel_->setObjectName("filesWidgetTitle");
    headerLayout->addWidget(titleLabel_);
    
    infoLabel_ = new QLabel();
    infoLabel_->setObjectName("filesWidgetInfo");
    headerLayout->addWidget(infoLabel_);
    
    headerLayout->addStretch();
    mainLayout->addLayout(headerLayout);
    
    // File tree
    filesTree_ = new QTreeWidget();
    filesTree_->setObjectName("filesTreeBottom");
    filesTree_->setHeaderLabels({tr("Name"), tr("Size"), tr("Download")});
    filesTree_->setColumnCount(3);
    filesTree_->setAlternatingRowColors(true);
    filesTree_->setRootIsDecorated(true);
    filesTree_->setItemsExpandable(true);
    filesTree_->setUniformRowHeights(true);
    
    // Configure header
    filesTree_->header()->setStretchLastSection(false);
    filesTree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    filesTree_->header()->setSectionResizeMode(1, QHeaderView::Fixed);
    filesTree_->header()->setSectionResizeMode(2, QHeaderView::Fixed);
    filesTree_->setColumnWidth(1, 100);
    filesTree_->setColumnWidth(2, 70);
    
    connect(filesTree_, &QTreeWidget::itemChanged, this, &TorrentFilesWidget::onFileItemChanged);
    
    mainLayout->addWidget(filesTree_, 1);
    
    // Start hidden - will be shown when files are loaded
    clear();
}

void TorrentFilesWidget::setFiles(const QString& hash, const QString& name, const QJsonArray& files)
{
    currentHash_ = hash;
    currentName_ = name;
    
    filesTree_->clear();
    
    if (files.isEmpty()) {
        infoLabel_->setText(tr("No files"));
        return;
    }
    
    // Update header info
    qint64 totalSize = 0;
    for (const QJsonValue& val : files) {
        totalSize += val.toObject()["size"].toVariant().toLongLong();
    }
    infoLabel_->setText(QString("%1 %2 • %3")
        .arg(files.size())
        .arg(files.size() == 1 ? tr("file") : tr("files"))
        .arg(formatBytes(totalSize)));
    
    // Block signals while populating
    filesTree_->blockSignals(true);
    
    // Build hierarchical tree structure
    FileTreeNode* root = buildFileTree(files);
    
    // Add nodes to widget
    for (auto it = root->children.begin(); it != root->children.end(); ++it) {
        addTreeNodeToWidget(it.value(), nullptr);
    }
    
    delete root;
    
    // Expand first level for better UX
    for (int i = 0; i < filesTree_->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = filesTree_->topLevelItem(i);
        item->setExpanded(true);
    }
    
    filesTree_->blockSignals(false);
    
    show();
}

void TorrentFilesWidget::clear()
{
    currentHash_.clear();
    currentName_.clear();
    filesTree_->clear();
    infoLabel_->setText(tr("Select a torrent to view files"));
}

QList<int> TorrentFilesWidget::getSelectedFileIndices() const
{
    QList<int> indices;
    
    for (int i = 0; i < filesTree_->topLevelItemCount(); ++i) {
        collectSelectedFiles(filesTree_->topLevelItem(i), indices);
    }
    
    return indices;
}

void TorrentFilesWidget::onFileItemChanged(QTreeWidgetItem* item, int column)
{
    if (column != 2) return;  // Only care about checkbox column
    
    Q_UNUSED(item);
    
    if (!currentHash_.isEmpty()) {
        emit fileSelectionChanged(currentHash_, getSelectedFileIndices());
    }
}

QString TorrentFilesWidget::formatBytes(qint64 bytes) const
{
    const qint64 KB = 1024;
    const qint64 MB = KB * 1024;
    const qint64 GB = MB * 1024;
    const qint64 TB = GB * 1024;
    
    if (bytes >= TB) {
        return QString::number(bytes / static_cast<double>(TB), 'f', 2) + " TB";
    } else if (bytes >= GB) {
        return QString::number(bytes / static_cast<double>(GB), 'f', 2) + " GB";
    } else if (bytes >= MB) {
        return QString::number(bytes / static_cast<double>(MB), 'f', 2) + " MB";
    } else if (bytes >= KB) {
        return QString::number(bytes / static_cast<double>(KB), 'f', 2) + " KB";
    } else {
        return QString::number(bytes) + " B";
    }
}

TorrentFilesWidget::FileTreeNode* TorrentFilesWidget::buildFileTree(const QJsonArray& files)
{
    FileTreeNode* root = new FileTreeNode();
    root->name = "";
    root->size = 0;
    
    for (int i = 0; i < files.size(); ++i) {
        QJsonObject fileObj = files.at(i).toObject();
        QString path = fileObj["path"].toString();
        qint64 size = fileObj["size"].toVariant().toLongLong();
        
        // Split path into parts
        QStringList pathParts = path.split('/', Qt::SkipEmptyParts);
        
        FileTreeNode* current = root;
        for (int j = 0; j < pathParts.size(); ++j) {
            const QString& part = pathParts[j];
            bool isLastPart = (j == pathParts.size() - 1);
            
            if (!current->children.contains(part)) {
                FileTreeNode* newNode = new FileTreeNode();
                newNode->name = part;
                newNode->size = 0;
                newNode->isFile = isLastPart;
                newNode->fileIndex = isLastPart ? i : -1;
                current->children[part] = newNode;
            }
            
            current = current->children[part];
            current->size += size;
        }
        
        root->size += size;
    }
    
    return root;
}

void TorrentFilesWidget::addTreeNodeToWidget(FileTreeNode* node, QTreeWidgetItem* parent)
{
    QTreeWidgetItem* item = new QTreeWidgetItem();
    
    // Set name with icon
    QString icon = node->isFile ? getFileTypeIcon(node->name) : "📁";
    item->setText(0, icon + " " + node->name);
    item->setToolTip(0, node->name);
    
    // Set size
    item->setText(1, formatBytes(node->size));
    item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
    
    // Set checkbox for files
    if (node->isFile) {
        item->setCheckState(2, Qt::Checked);
        item->setData(0, Qt::UserRole, node->fileIndex);
    } else {
        item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
    }
    
    // Add to tree
    if (parent) {
        parent->addChild(item);
    } else {
        filesTree_->addTopLevelItem(item);
    }
    
    // Separate folders and files for sorting
    QList<FileTreeNode*> folders;
    QList<FileTreeNode*> fileNodes;
    
    for (auto it = node->children.begin(); it != node->children.end(); ++it) {
        if (it.value()->isFile) {
            fileNodes.append(it.value());
        } else {
            folders.append(it.value());
        }
    }
    
    // Sort by name
    std::sort(folders.begin(), folders.end(), [](FileTreeNode* a, FileTreeNode* b) {
        return a->name.toLower() < b->name.toLower();
    });
    std::sort(fileNodes.begin(), fileNodes.end(), [](FileTreeNode* a, FileTreeNode* b) {
        return a->name.toLower() < b->name.toLower();
    });
    
    // Add folders first, then files
    for (FileTreeNode* folder : folders) {
        addTreeNodeToWidget(folder, item);
    }
    for (FileTreeNode* file : fileNodes) {
        addTreeNodeToWidget(file, item);
    }
}

QString TorrentFilesWidget::getFileTypeIcon(const QString& filename) const
{
    QString ext = filename.section('.', -1).toLower();
    
    // Video extensions
    static const QSet<QString> videoExts = {
        "mkv", "mp4", "avi", "mov", "wmv", "flv", "webm", "m4v", "mpg", "mpeg",
        "3gp", "ts", "m2ts", "vob", "divx", "rmvb", "asf"
    };
    
    // Audio extensions
    static const QSet<QString> audioExts = {
        "mp3", "flac", "wav", "aac", "ogg", "wma", "m4a", "opus", "ape", "ac3"
    };
    
    // Image extensions
    static const QSet<QString> imageExts = {
        "jpg", "jpeg", "png", "gif", "bmp", "webp", "tiff", "svg", "ico", "psd"
    };
    
    // Book/document extensions
    static const QSet<QString> bookExts = {
        "pdf", "epub", "mobi", "djvu", "fb2", "doc", "docx", "txt", "rtf", "chm"
    };
    
    // Archive extensions
    static const QSet<QString> archiveExts = {
        "zip", "rar", "7z", "tar", "gz", "bz2", "xz", "iso"
    };
    
    // Application extensions
    static const QSet<QString> appExts = {
        "exe", "msi", "dmg", "apk", "deb", "rpm", "app"
    };
    
    if (videoExts.contains(ext)) return "🎬";
    if (audioExts.contains(ext)) return "🎵";
    if (imageExts.contains(ext)) return "🖼️";
    if (bookExts.contains(ext)) return "📚";
    if (archiveExts.contains(ext)) return "📦";
    if (appExts.contains(ext)) return "💿";
    
    return "📄";  // Default file icon
}

void TorrentFilesWidget::collectSelectedFiles(QTreeWidgetItem* item, QList<int>& indices) const
{
    if (!item) return;
    
    int fileIndex = item->data(0, Qt::UserRole).toInt();
    if (fileIndex >= 0 && item->checkState(2) == Qt::Checked) {
        indices.append(fileIndex);
    }
    
    for (int i = 0; i < item->childCount(); ++i) {
        collectSelectedFiles(item->child(i), indices);
    }
}
