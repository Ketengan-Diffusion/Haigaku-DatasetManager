#include "mainwindow.h"
#include "StatisticsDialog.h" 
#include "ThumbnailListModel.h" 
#include "ThumbnailLoader.h"  

#include <QApplication>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QLabel>
#include <QTextEdit>
#include <QListView>      
#include <QScrollBar> 
#include <QSplitter>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout> 
#include <QStackedWidget> 
#include <QStatusBar>
#include <QPixmap>
#include <QImageReader>
#include <QFileInfo>
#include <QDir>
#include <QKeyEvent>
#include <QPainter> 
#include <QDebug>
#include <QIcon> 
#include <QTimer> 
#include <QStandardPaths> 
#include <QDataStream> 
#include <QRadioButton>   
#include <QGroupBox>      
#include <QButtonGroup>   

#include <QtConcurrent/QtConcurrent> 
#include <QFuture>

#include <QVideoWidget>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QPushButton>
#include <QSlider>
#include <QStyle> 

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , imageDisplayLabel(nullptr)
    , videoDisplayWidget(nullptr)
    , mediaDisplayContainer(nullptr)
    , imageScrollArea(nullptr)
    , captionEditor(nullptr)
    , thumbnailListView(nullptr) 
    , m_thumbnailModel(nullptr)  
    , m_thumbnailLoaderService(nullptr) 
    , captionModeGroupBox(nullptr)   
    , nlpModeRadioButton(nullptr)    
    , tagsModeRadioButton(nullptr)   
    , fileDetailsLabel(nullptr)
    , videoControlsWidget(nullptr)
    , playPauseButton(nullptr)
    , seekerSlider(nullptr)
    , durationLabel(nullptr)
    , mainSplitter(nullptr)
    , rightPanelSplitter(nullptr)
    , mediaPlayer(nullptr)
    , audioOutput(nullptr)
    , currentMediaIndex(-1)
    , captionChangedSinceLoad(false)
    , thumbnailDefaultSize(180, 100)
    , autoSaveTimer(nullptr)
    , m_scrollStopTimer(nullptr) 
{
    resize(1200, 800);

    mediaPlayer = new QMediaPlayer(this);
    audioOutput = new QAudioOutput(this);
    mediaPlayer->setAudioOutput(audioOutput);

    m_thumbnailModel = new ThumbnailListModel(this);
    m_thumbnailLoaderService = new ThumbnailLoader(this);
    m_thumbnailModel->setThumbnailLoader(m_thumbnailLoaderService);
    m_thumbnailModel->setThumbnailSize(thumbnailDefaultSize);

    m_scrollStopTimer = new QTimer(this);
    m_scrollStopTimer->setSingleShot(true);
    m_scrollStopTimer->setInterval(500); 
    connect(m_scrollStopTimer, &QTimer::timeout, this, &MainWindow::loadVisibleThumbnails);

    setupUI(); 
    createMenus();
    createStatusBar();

    connect(mediaPlayer, &QMediaPlayer::durationChanged, this, [this](qint64 duration){
        if(seekerSlider) seekerSlider->setRange(0, duration / 1000); 
        qint64 secs = duration / 1000; qint64 mins = secs / 60; secs %= 60;
        if (durationLabel) durationLabel->setText(QString("%1:%2").arg(mins, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0')));
    });
    connect(mediaPlayer, &QMediaPlayer::positionChanged, this, [this](qint64 position){
        if (seekerSlider && !seekerSlider->isSliderDown()) seekerSlider->setValue(position / 1000);
    });
    connect(mediaPlayer, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state){
        if(playPauseButton) {
            if (state == QMediaPlayer::PlayingState) {
                playPauseButton->setIcon(style()->standardIcon(QStyle::SP_MediaPause)); playPauseButton->setToolTip(tr("Pause"));
            } else {
                playPauseButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay)); playPauseButton->setToolTip(tr("Play"));
            }
        }
    });
    connect(mediaPlayer, &QMediaPlayer::errorChanged, this, [this](){
        if (mediaPlayer->error() != QMediaPlayer::NoError) {
            qWarning() << "MediaPlayer Error:" << mediaPlayer->errorString();
            statusBar()->showMessage(tr("Error playing media: %1").arg(mediaPlayer->errorString()), 5000);
        }
    });
    
    autoSaveTimer = new QTimer(this);
    connect(autoSaveTimer, &QTimer::timeout, this, &MainWindow::performAutoSave);
    autoSaveTimer->start(30 * 60 * 1000); 

    statusBar()->showMessage(tr("Ready. Please open a directory."));
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUI()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);
    mainSplitter = new QSplitter(Qt::Horizontal, centralWidget);

    thumbnailListView = new QListView(mainSplitter);
    thumbnailListView->setModel(m_thumbnailModel);
    thumbnailListView->setFixedWidth(thumbnailDefaultSize.width() + 40); 
    thumbnailListView->setViewMode(QListView::IconMode);
    thumbnailListView->setIconSize(thumbnailDefaultSize);
    thumbnailListView->setResizeMode(QListView::Adjust); 
    thumbnailListView->setMovement(QListView::Static); 
    thumbnailListView->setWordWrap(true); 
    connect(thumbnailListView, &QListView::clicked, this, &MainWindow::onThumbnailViewClicked);
    connect(thumbnailListView->verticalScrollBar(), &QScrollBar::valueChanged, this, &MainWindow::onThumbnailViewScrolled);
    mainSplitter->addWidget(thumbnailListView);
    
    QWidget *centerPanelWidget = new QWidget(mainSplitter);
    QVBoxLayout *centerPanelLayout = new QVBoxLayout(centerPanelWidget);
    centerPanelLayout->setContentsMargins(0,0,0,0);
    mediaDisplayContainer = new QStackedWidget(centerPanelWidget);
    imageScrollArea = new QScrollArea(mediaDisplayContainer);
    imageScrollArea->setBackgroundRole(QPalette::Dark);
    imageScrollArea->setAlignment(Qt::AlignCenter);
    imageDisplayLabel = new QLabel(imageScrollArea);
    imageDisplayLabel->setObjectName("imageDisplayLabel");
    imageDisplayLabel->setBackgroundRole(QPalette::Dark);
    imageDisplayLabel->setAlignment(Qt::AlignCenter);
    imageDisplayLabel->setScaledContents(false);
    imageScrollArea->setWidget(imageDisplayLabel);
    imageScrollArea->setWidgetResizable(true);
    mediaDisplayContainer->addWidget(imageScrollArea);
    videoDisplayWidget = new QVideoWidget(mediaDisplayContainer);
    mediaPlayer->setVideoOutput(videoDisplayWidget);
    mediaDisplayContainer->addWidget(videoDisplayWidget);
    centerPanelLayout->addWidget(mediaDisplayContainer, 1);
    videoControlsWidget = new QWidget(centerPanelWidget);
    QHBoxLayout *controlsLayout = new QHBoxLayout(videoControlsWidget);
    controlsLayout->setContentsMargins(5,2,5,2);
    playPauseButton = new QPushButton(videoControlsWidget);
    playPauseButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    playPauseButton->setToolTip(tr("Play"));
    connect(playPauseButton, &QPushButton::clicked, this, [this](){
        if (mediaPlayer->playbackState() == QMediaPlayer::PlayingState) mediaPlayer->pause(); else mediaPlayer->play();
    });
    controlsLayout->addWidget(playPauseButton);
    seekerSlider = new QSlider(Qt::Horizontal, videoControlsWidget);
    seekerSlider->setRange(0,0);
    connect(seekerSlider, &QSlider::sliderMoved, this, [this](int position){
        mediaPlayer->setPosition(static_cast<qint64>(position) * 1000);
    });
    controlsLayout->addWidget(seekerSlider);
    durationLabel = new QLabel("--:--", videoControlsWidget);
    controlsLayout->addWidget(durationLabel);
    centerPanelLayout->addWidget(videoControlsWidget);
    videoControlsWidget->setVisible(false);
    mainSplitter->addWidget(centerPanelWidget);
    
    QWidget *rightPanelContainer = new QWidget(mainSplitter);
    QVBoxLayout *rightPanelLayout = new QVBoxLayout(rightPanelContainer);
    rightPanelLayout->setContentsMargins(0,0,0,0); 
    captionModeGroupBox = new QGroupBox(tr("Caption Mode"), rightPanelContainer);
    QHBoxLayout *captionModeLayout = new QHBoxLayout(captionModeGroupBox);
    nlpModeRadioButton = new QRadioButton(tr("NLP"), captionModeGroupBox);
    tagsModeRadioButton = new QRadioButton(tr("Tags"), captionModeGroupBox);
    nlpModeRadioButton->setChecked(true); 
    captionModeLayout->addWidget(nlpModeRadioButton);
    captionModeLayout->addWidget(tagsModeRadioButton);
    captionModeGroupBox->setLayout(captionModeLayout);
    rightPanelLayout->addWidget(captionModeGroupBox);
    QButtonGroup *captionModeButtonGroup = new QButtonGroup(this);
    captionModeButtonGroup->addButton(nlpModeRadioButton);
    captionModeButtonGroup->addButton(tagsModeRadioButton);
    connect(nlpModeRadioButton, &QRadioButton::toggled, this, &MainWindow::onCaptionModeChanged);
    rightPanelSplitter = new QSplitter(Qt::Vertical, rightPanelContainer);
    captionEditor = new QTextEdit(rightPanelSplitter);
    captionEditor->setPlaceholderText(tr("Caption will appear here..."));
    connect(captionEditor, &QTextEdit::textChanged, this, [this]() { 
        captionChangedSinceLoad = true; 
        if (currentMediaIndex >= 0 && currentMediaIndex < mediaFiles.count()) {
            if(captionEditor) unsavedCaptions[mediaFiles.at(currentMediaIndex)] = captionEditor->toPlainText();
        }
    });
    rightPanelSplitter->addWidget(captionEditor);
    fileDetailsLabel = new QLabel(tr("File details will appear here..."), rightPanelSplitter);
    fileDetailsLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    fileDetailsLabel->setWordWrap(true);
    rightPanelSplitter->addWidget(fileDetailsLabel);
    rightPanelSplitter->setSizes({400, 100}); 
    rightPanelLayout->addWidget(rightPanelSplitter, 1); 
    rightPanelContainer->setLayout(rightPanelLayout);
    mainSplitter->addWidget(rightPanelContainer);
    
    mainSplitter->setStretchFactor(1, 1); 
    mainSplitter->setStretchFactor(2, 0); 
    mainLayout->addWidget(mainSplitter);
    centralWidget->setLayout(mainLayout);
    mainSplitter->setSizes({thumbnailDefaultSize.width() + 40, 700, 300});
}

void MainWindow::onCaptionModeChanged()
{
    if (nlpModeRadioButton && nlpModeRadioButton->isChecked()) {
        qDebug() << "Caption Mode: NLP";
    } else if (tagsModeRadioButton && tagsModeRadioButton->isChecked()) {
        qDebug() << "Caption Mode: Tags";
    }
}

void MainWindow::onThumbnailViewClicked(const QModelIndex &index) { 
    if (captionChangedSinceLoad && currentMediaIndex >= 0 && currentMediaIndex < mediaFiles.count()) {
        if(captionEditor) unsavedCaptions[mediaFiles.at(currentMediaIndex)] = captionEditor->toPlainText();
    }
    displayMediaAtIndex(index.row());
}
void MainWindow::onThumbnailViewScrolled() {
    if (m_scrollStopTimer) {
        m_scrollStopTimer->start(); 
    }
}
void MainWindow::loadVisibleThumbnails() {
    if (!thumbnailListView || !m_thumbnailModel || !m_thumbnailLoaderService || mediaFiles.isEmpty()) { 
        return;
    }
    QModelIndex topLeft = thumbnailListView->indexAt(thumbnailListView->viewport()->rect().topLeft());
    QModelIndex bottomRight = thumbnailListView->indexAt(thumbnailListView->viewport()->rect().bottomRight());
    int firstVisibleRow = topLeft.isValid() ? topLeft.row() : 0;
    int lastVisibleRow = bottomRight.isValid() ? bottomRight.row() : m_thumbnailModel->rowCount() - 1;
    int buffer = 10; 
    int startRow = qMax(0, firstVisibleRow - buffer);
    int endRow = qMin(m_thumbnailModel->rowCount() - 1, lastVisibleRow + buffer);
    m_thumbnailLoaderService->clearQueue();
    QList<ThumbnailRequest> requests;
    for (int i = startRow; i <= endRow; ++i) {
        if (!m_thumbnailModel->isThumbnailLoaded(i)) { 
            requests.append({i, m_thumbnailModel->filePathAt(i), thumbnailDefaultSize});
        }
    }
    if (!requests.isEmpty()) {
        m_thumbnailLoaderService->requestThumbnailBatch(requests);
    }
}
void MainWindow::createMenus() { 
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    openDirAction = new QAction(tr("&Open Directory..."), this);
    openDirAction->setShortcuts(QKeySequence::Open);
    connect(openDirAction, &QAction::triggered, this, &MainWindow::openDirectory);
    fileMenu->addAction(openDirAction);
    fileMenu->addSeparator();
    QAction *saveCaptionAction = new QAction(tr("&Save Caption"), this);
    saveCaptionAction->setShortcuts(QKeySequence::Save);
    connect(saveCaptionAction, &QAction::triggered, this, &MainWindow::saveCurrentCaption);
    fileMenu->addAction(saveCaptionAction);
    fileMenu->addSeparator();
    QAction *nextAction = new QAction(tr("&Next Media"), this);
    connect(nextAction, &QAction::triggered, this, &MainWindow::nextMedia);
    fileMenu->addAction(nextAction);
    QAction *prevAction = new QAction(tr("&Previous Media"), this);
    connect(prevAction, &QAction::triggered, this, &MainWindow::previousMedia);
    fileMenu->addAction(prevAction);
    fileMenu->addSeparator();
    exitAction = new QAction(tr("E&xit"), this);
    exitAction->setShortcuts(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, qApp, &QApplication::quit);
    fileMenu->addAction(exitAction);
    QMenu *statisticMenu = menuBar()->addMenu(tr("&Statistic"));
    statisticsAction = new QAction(tr("Show &Dataset Statistics..."), this);
    connect(statisticsAction, &QAction::triggered, this, &MainWindow::showStatisticsDialog);
    statisticMenu->addAction(statisticsAction);
    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    aboutAction = new QAction(tr("&About Haigaku Manager"), this);
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAboutDialog);
    helpMenu->addAction(aboutAction);
    aboutQtAction = new QAction(tr("About &Qt"), this);
    connect(aboutQtAction, &QAction::triggered, qApp, &QApplication::aboutQt);
    helpMenu->addAction(aboutQtAction);
}
void MainWindow::createStatusBar() { 
    statusBar()->showMessage(tr("Ready"));
}
void MainWindow::openDirectory() { 
    if (captionChangedSinceLoad && currentMediaIndex >= 0 && currentMediaIndex < mediaFiles.count()) {
         if(captionEditor) unsavedCaptions[mediaFiles.at(currentMediaIndex)] = captionEditor->toPlainText();
    }
    QString dirPath = QFileDialog::getExistingDirectory(this, tr("Open Directory"),
                                                       currentDirectory.isEmpty() ? QDir::homePath() : currentDirectory,
                                                       QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!dirPath.isEmpty()) {
        currentDirectory = dirPath;
        unsavedCaptions.clear(); 
        statusBar()->showMessage(tr("Loading files from: %1").arg(currentDirectory));
        loadFiles(currentDirectory);
    }
}
void MainWindow::loadFiles(const QString &dirPath)
{
    if(mediaPlayer) mediaPlayer->stop();
    mediaFiles.clear(); 
    if(m_thumbnailModel) m_thumbnailModel->clear(); 
    currentMediaIndex = -1;
    captionChangedSinceLoad = false; 
    QDir directory(dirPath);
    QStringList nameFilters;
    nameFilters << "*.jpg" << "*.jpeg" << "*.png" << "*.bmp" << "*.gif" << "*.webp" << "*.tiff";
    nameFilters << "*.mp4" << "*.mkv" << "*.webm";
    QStringList filesFound = directory.entryList(nameFilters, QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    if (filesFound.isEmpty()) { 
        statusBar()->showMessage(tr("No supported media files found in %1").arg(dirPath));
        return;
    }
    QStringList fullFilePaths;
    for(const QString &file : filesFound) {
        fullFilePaths.append(directory.filePath(file));
    }
    mediaFiles = fullFilePaths; 
    if(m_thumbnailModel) m_thumbnailModel->setFilePaths(fullFilePaths); 
    if (!mediaFiles.isEmpty()) {
        displayMediaAtIndex(0);
        if(thumbnailListView && m_thumbnailModel) {
            QModelIndex firstIndex = m_thumbnailModel->index(0,0);
            thumbnailListView->setCurrentIndex(firstIndex);
        }
        QTimer::singleShot(0, this, &MainWindow::loadVisibleThumbnails); 
    }
    statusBar()->showMessage(tr("Loaded %1 media files. Thumbnails loading on demand...").arg(mediaFiles.count()));
}
void MainWindow::displayMediaAtIndex(int index) { 
    if (captionChangedSinceLoad && currentMediaIndex >= 0 && currentMediaIndex < mediaFiles.count()) {
        if (captionEditor) { 
             unsavedCaptions[mediaFiles.at(currentMediaIndex)] = captionEditor->toPlainText();
        }
    }
    if (index < 0 || index >= mediaFiles.count()) { 
        qWarning() << "displayMediaAtIndex: Index out of bounds" << index;
        return;
    }
    if(mediaPlayer) mediaPlayer->stop();
    currentMediaIndex = index;
    QString filePath = mediaFiles.at(currentMediaIndex);
    QFileInfo fileInfo(filePath);
    QStringList imageExtensions = {"jpg", "jpeg", "png", "bmp", "gif", "webp", "tiff"};
    bool isImage = imageExtensions.contains(fileInfo.suffix().toLower());
    if (isImage) {
        if(mediaDisplayContainer && imageScrollArea) mediaDisplayContainer->setCurrentWidget(imageScrollArea);
        if(videoControlsWidget) videoControlsWidget->setVisible(false);
        if(videoDisplayWidget) videoDisplayWidget->setVisible(false); 
        if(imageScrollArea) imageScrollArea->setVisible(true);
        QImageReader reader(filePath); reader.setAutoTransform(true); QImage image = reader.read();
        if (image.isNull()) {
            qWarning() << "Failed to read image:" << filePath << "Error:" << reader.errorString();
            if(imageDisplayLabel) {
                imageDisplayLabel->setText(tr("Cannot load image: %1").arg(fileInfo.fileName()));
                QPixmap errorPixmap(200, 200); errorPixmap.fill(Qt::gray); imageDisplayLabel->setPixmap(errorPixmap);
            }
        } else {
            QPixmap pixmap = QPixmap::fromImage(image);
            if (!pixmap.isNull() && imageScrollArea && imageScrollArea->viewport()) {
                QSize viewportSize = imageScrollArea->viewport()->size();
                if (viewportSize.width() > 0 && viewportSize.height() > 0) 
                     pixmap = pixmap.scaled(viewportSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
            if(imageDisplayLabel) {
                imageDisplayLabel->setPixmap(pixmap);
                if (!pixmap.isNull()) imageDisplayLabel->adjustSize(); else imageDisplayLabel->setMinimumSize(1,1);
            }
        }
    } else { 
        if(mediaDisplayContainer && videoDisplayWidget) mediaDisplayContainer->setCurrentWidget(videoDisplayWidget);
        if(videoControlsWidget) videoControlsWidget->setVisible(true);
        if(imageScrollArea) imageScrollArea->setVisible(false); 
        if(videoDisplayWidget) videoDisplayWidget->setVisible(true);
        if(mediaPlayer) mediaPlayer->setSource(QUrl::fromLocalFile(filePath));
    }
    updateFileDetails(filePath);
    loadCaptionForCurrentImage(); 
    if(thumbnailListView && m_thumbnailModel) {
         thumbnailListView->setCurrentIndex(m_thumbnailModel->index(currentMediaIndex, 0));
    }
    statusBar()->showMessage(tr("Displaying: %1 (%2/%3)")
                             .arg(fileInfo.fileName()).arg(currentMediaIndex + 1).arg(mediaFiles.count()));
}
void MainWindow::updateFileDetails(const QString &filePath) { 
    QFileInfo info(filePath);
    QString detailsText = QString("<b>File:</b> %1<br><b>Path:</b> %2<br><b>Size:</b> %3 KB")
                          .arg(info.fileName()).arg(info.absoluteFilePath()).arg(info.size() / 1024);
    QStringList imageExtensions = {"jpg", "jpeg", "png", "bmp", "gif", "webp", "tiff"};
    if (imageExtensions.contains(info.suffix().toLower())) {
        QImageReader reader(filePath);
        QSize imageSize = reader.size();
        detailsText += QString("<br><b>Resolution:</b> %1x%2").arg(imageSize.width()).arg(imageSize.height());
    }
    if(fileDetailsLabel) fileDetailsLabel->setText(detailsText);
}
void MainWindow::loadCaptionForCurrentImage() { 
    if(captionEditor) captionEditor->blockSignals(true); 
    QString captionToLoad = "";
    if (currentMediaIndex >= 0 && currentMediaIndex < mediaFiles.count()) {
        QString mediaPath = mediaFiles.at(currentMediaIndex);
        if (unsavedCaptions.contains(mediaPath)) {
            captionToLoad = unsavedCaptions.value(mediaPath);
        } else {
            QFileInfo mediaInfo(mediaPath);
            QString baseName = mediaInfo.absolutePath() + "/" + mediaInfo.completeBaseName();
            QString captionPathTxt = baseName + ".txt";
            QString captionPathCaption = baseName + ".caption";
            QFile captionFile;
            if (QFile::exists(captionPathTxt)) captionFile.setFileName(captionPathTxt);
            else if (QFile::exists(captionPathCaption)) captionFile.setFileName(captionPathCaption);
            if (!captionFile.fileName().isEmpty()) {
                if (captionFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    captionToLoad = captionFile.readAll();
                    captionFile.close();
                } else {
                    qWarning() << "Could not open caption file:" << captionFile.fileName();
                    captionToLoad = tr("[Error reading caption file]");
                }
            }
        }
    }
    if(captionEditor) captionEditor->setPlainText(captionToLoad);
    if(captionEditor) captionEditor->blockSignals(false);
    captionChangedSinceLoad = false; 
}
void MainWindow::saveCurrentCaption()  { 
    if (currentMediaIndex < 0 || currentMediaIndex >= mediaFiles.count()) return;
    QString captionTextToSave;
    if (captionEditor) captionTextToSave = captionEditor->toPlainText();
    else return; 
    QString mediaPath = mediaFiles.at(currentMediaIndex);
    QFileInfo mediaInfo(mediaPath);
    QString captionPath = mediaInfo.absolutePath() + "/" + mediaInfo.completeBaseName() + ".txt";
    QFile captionFile(captionPath);
    if (captionFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QTextStream out(&captionFile);
        out << captionTextToSave;
        captionFile.close();
        captionChangedSinceLoad = false;
        unsavedCaptions.remove(mediaPath); 
        statusBar()->showMessage(tr("Caption saved for %1").arg(mediaInfo.fileName()));
    } else {
        qWarning() << "Could not save caption file:" << captionPath;
        QMessageBox::warning(this, tr("Save Error"), tr("Could not save caption to %1").arg(captionPath));
        statusBar()->showMessage(tr("Error saving caption for %1").arg(mediaInfo.fileName()));
    }
}
void MainWindow::applyScoreToCaption(int scoreValue)  { 
    if (currentMediaIndex < 0 || currentMediaIndex >= mediaFiles.count() || !captionEditor) return;
    QString scoreWord;
    if (scoreValue >= 1 && scoreValue <= 3) scoreWord = "Worst";
    else if (scoreValue == 4) scoreWord = "Lousy";
    else if (scoreValue == 5) scoreWord = "adequate";
    else if (scoreValue == 6) scoreWord = "Superior";
    else if (scoreValue == 7) scoreWord = "Masterpiece";
    else if (scoreValue == 8) scoreWord = "Exceptional";
    else if (scoreValue == 9) scoreWord = "Iconic";
    else return;
    QString currentCaption = captionEditor->toPlainText();
    QStringList parts = currentCaption.split(',', Qt::SkipEmptyParts);
    QStringList existingScores = {"Worst", "Lousy", "adequate", "Superior", "Masterpiece", "Exceptional", "Iconic"};
    if (!parts.isEmpty()) {
        QString firstPartTrimmed = parts.first().trimmed();
        if (existingScores.contains(firstPartTrimmed, Qt::CaseInsensitive)) {
            parts.removeFirst();
        }
    }
    parts.prepend(scoreWord);
    for(int i = 0; i < parts.size(); ++i) parts[i] = parts[i].trimmed();
    captionEditor->setPlainText(parts.join(", "));
}
void MainWindow::keyPressEvent(QKeyEvent *event) { 
    if (captionEditor && captionEditor->hasFocus()) {
        QMainWindow::keyPressEvent(event); return;
    }
    switch (event->key()) {
    case Qt::Key_S: saveCurrentCaption(); nextMedia(); break;
    case Qt::Key_Right: nextMedia(); break;
    case Qt::Key_Left: previousMedia(); break;
    case Qt::Key_Delete: 
        deleteCurrentMediaItem(); 
        break;
    case Qt::Key_1: case Qt::Key_2: case Qt::Key_3: case Qt::Key_4: case Qt::Key_5: 
    case Qt::Key_6: case Qt::Key_7: case Qt::Key_8: case Qt::Key_9:
        applyScoreToCaption(event->key() - Qt::Key_0); saveCurrentCaption(); nextMedia(); break;
    default: QMainWindow::keyPressEvent(event);
    }
}
void MainWindow::nextMedia() { 
    if (mediaFiles.isEmpty()) return;
    int nextIndex = (currentMediaIndex + 1);
    if (nextIndex >= mediaFiles.count()) nextIndex = 0; 
    if (nextIndex != currentMediaIndex || mediaFiles.count() == 1) {
         displayMediaAtIndex(nextIndex);
    }
}
void MainWindow::previousMedia() { 
    if (mediaFiles.isEmpty()) return;
    int prevIndex = (currentMediaIndex - 1);
    if (prevIndex < 0) prevIndex = mediaFiles.count() - 1; 
     if (prevIndex != currentMediaIndex || mediaFiles.count() == 1) {
        displayMediaAtIndex(prevIndex);
    }
}
void MainWindow::performAutoSave() { 
    qDebug() << "Performing auto-save...";
    int savedCount = 0;
    QStringList keysToSave = unsavedCaptions.keys();
    for (const QString &filePathToSave : keysToSave) {
        if (!mediaFiles.contains(filePathToSave)) continue; 
        QString captionText = unsavedCaptions.value(filePathToSave);
        QFileInfo mediaInfo(filePathToSave);
        QString captionPath = mediaInfo.absolutePath() + "/" + mediaInfo.completeBaseName() + ".txt";
        QFile captionFile(captionPath);
        if (captionFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            QTextStream out(&captionFile); out << captionText; captionFile.close();
            unsavedCaptions.remove(filePathToSave); savedCount++;
        } else {
            qWarning() << "Auto-save failed for" << captionPath;
        }
    }
    if (savedCount > 0) statusBar()->showMessage(tr("Auto-saved %1 captions.").arg(savedCount), 5000);
    else statusBar()->showMessage(tr("Auto-save: No changes to save."), 3000);
}

void MainWindow::deleteCurrentMediaItem() {
    if (currentMediaIndex < 0 || currentMediaIndex >= mediaFiles.count()) {
        statusBar()->showMessage(tr("No media item selected to delete."), 3000);
        return;
    }

    QString filePathToDelete = mediaFiles.at(currentMediaIndex);
    QFileInfo mediaInfo(filePathToDelete);

    // Delete media file
    if (QFile::remove(filePathToDelete)) {
        statusBar()->showMessage(tr("Deleted media file: %1").arg(mediaInfo.fileName()), 3000);
    } else {
        statusBar()->showMessage(tr("Error deleting media file: %1").arg(mediaInfo.fileName()), 3000);
        qWarning() << "Error deleting media file:" << filePathToDelete;
        // Proceed to delete caption anyway, or stop? For now, proceed.
    }

    // Delete associated caption files (.txt and .caption)
    QString baseName = mediaInfo.absolutePath() + "/" + mediaInfo.completeBaseName();
    QString captionPathTxt = baseName + ".txt";
    QString captionPathCaption = baseName + ".caption";

    if (QFile::exists(captionPathTxt)) {
        if (QFile::remove(captionPathTxt)) {
            qDebug() << "Deleted caption file:" << captionPathTxt;
        } else {
            qWarning() << "Error deleting caption file:" << captionPathTxt;
        }
    }
    if (QFile::exists(captionPathCaption)) {
        if (QFile::remove(captionPathCaption)) {
            qDebug() << "Deleted caption file:" << captionPathCaption;
        } else {
            qWarning() << "Error deleting caption file:" << captionPathCaption;
        }
    }

    // Remove from internal lists and update UI
    unsavedCaptions.remove(filePathToDelete);
    mediaFiles.removeAt(currentMediaIndex);
    
    // Refresh the model with the modified mediaFiles list
    // This will also trigger ThumbnailLoader to clear its queue and re-evaluate.
    if (m_thumbnailModel) {
        m_thumbnailModel->setFilePaths(mediaFiles); // This re-populates the model
    }
    
    if (mediaFiles.isEmpty()) {
        currentMediaIndex = -1;
        // Clear displays
        if(imageDisplayLabel) imageDisplayLabel->clear();
        if(videoDisplayWidget && mediaPlayer) mediaPlayer->setSource(QUrl()); // Clear video source
        if(videoControlsWidget) videoControlsWidget->setVisible(false);
        if(captionEditor) captionEditor->clear();
        if(fileDetailsLabel) fileDetailsLabel->setText(tr("File details will appear here..."));
        statusBar()->showMessage(tr("All media deleted or directory empty."), 3000);
    } else {
        // Adjust currentMediaIndex if it's now out of bounds
        if (currentMediaIndex >= mediaFiles.count()) {
            currentMediaIndex = mediaFiles.count() - 1;
        }
        // If currentMediaIndex became -1 (e.g. last item deleted from a list of 1),
        // and list is not empty, set to 0.
        if (currentMediaIndex < 0 && !mediaFiles.isEmpty()) {
            currentMediaIndex = 0;
        }
        displayMediaAtIndex(currentMediaIndex); // Display the item at the (potentially new) current index
    }
     // After model is updated, trigger a load for the new visible range
    QTimer::singleShot(0, this, &MainWindow::loadVisibleThumbnails);
}


void MainWindow::showStatisticsDialog() { 
    if (mediaFiles.isEmpty() && currentDirectory.isEmpty()) {
        QMessageBox::information(this, tr("Statistics"), tr("Please open a directory first.")); return;
    }
    StatisticsDialog dialog(mediaFiles, currentDirectory, this);
    dialog.exec();
}
void MainWindow::showAboutDialog()  { 
    QMessageBox::about(this, tr("About Haigaku Manager"),
                       tr("<b>Haigaku Manager</b><br>Version 0.1 (Alpha)<br><br>"
                          "A dataset manager for images and videos.<br>"
                          "Created by Ketengan Diffusion™.<br><br>Built with Qt."));
}
