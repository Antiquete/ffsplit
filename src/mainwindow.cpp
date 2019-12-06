// Copyright (C) 2019 Hari Saksena <hari.mail@protonmail.ch>
// 
// This file is part of FFSplit.
// 
// FFSplit is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// FFSplit is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with FFSplit.  If not, see <http://www.gnu.org/licenses/>.

#include "mainwindow.h"
#include "ui_mainwindow.h"

#define PROGRESS_WIDTH_PER_SEGMENT 10

static QString secondsToTime(int secs)
{
    return QString("%1:%2:%3").arg(secs/3600, 2, 10, QChar('0')).arg((secs%3600)/60, 2, 10, QChar('0')).arg(secs%60, 2, 10, QChar('0'));
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    int hrs = settings.value("lastHrs", 0).toInt();
    ui->prtHrs->setValue(hrs);
    connect(ui->prtHrs, SIGNAL(valueChanged(int)), this, SLOT(segmentSet(int)));

    int mins = settings.value("lastMins", 5).toInt();
    ui->prtMins->setValue(mins);
    connect(ui->prtMins, SIGNAL(valueChanged(int)), this, SLOT(segmentSet(int)));

    int secs = settings.value("lastSecs", 0).toInt();
    ui->prtSecs->setValue(secs);
    connect(ui->prtSecs, SIGNAL(valueChanged(int)), this, SLOT(segmentSet(int)));

    timestep = (hrs*3600) + (mins*60) + secs;

    crossfade = settings.value("lastCrossfade", 10).toInt();
    ui->crossfade->setValue(crossfade);
    connect(ui->crossfade, SIGNAL(valueChanged(int)), this, SLOT(setCrossfade(int)));

    crf = settings.value("lastCRF", 18).toInt();
    ui->cbCrf->setCurrentIndex(crf);
    connect(ui->cbCrf, SIGNAL(currentIndexChanged(int)), this, SLOT(setCRF(int)));

    ff->setProgram("ffmpeg");

    connect(ui->srcButton, SIGNAL(clicked(bool)), this, SLOT(setSource()));
    connect(ui->dstButton, SIGNAL(clicked(bool)), this, SLOT(setDest()));

    connect(ui->customName, SIGNAL(editingFinished()), this, SLOT(refreshSegments()));


    connect(ui->cbOverwrite_corrupt, SIGNAL(stateChanged(int)), this, SLOT(refreshSegments()));
    connect(ui->cbOverwrite_diffsize, SIGNAL(stateChanged(int)), this, SLOT(refreshSegments()));
    connect(ui->cbOverwrite_existing, SIGNAL(stateChanged(int)), this, SLOT(refreshSegments()));


    connect(ui->startButton, SIGNAL(clicked()), this, SLOT(startTranscode()));
    connect(ff, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(processQueue()));
    connect(ff, SIGNAL(readyReadStandardError()), this, SLOT(processProgress()));

    statusBar()->showMessage("Ready!");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setSource()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Select source"),  settings.value("lastOpen", QDir::homePath()).value<QString>(), tr("Video Files (*.avi *.mkv *.mp4)"));

    src = QFileInfo(fileName);
    ui->vidSource->setText(src.filePath());
    ui->customName->setText(src.completeBaseName());

    dst = QDir(src.absolutePath());
    ui->vidDest->setText(dst.path());
    settings.setValue("lastOpen", dst.path());

    QProcess prober(this);
    QStringList proberArgs;
    proberArgs << "-v" << "error" << "-show_entries" << "format=duration" << "-of" << "default=noprint_wrappers=1:nokey=1" << src.filePath();
    prober.start("ffprobe", proberArgs);
    prober.waitForFinished();
    vidlen = int(prober.readAllStandardOutput().toDouble());
    if(vidlen == 0)
    {
        statusBar()->showMessage("Error: Cannot find video length.");
        return;
    }

    ui->startButton->setEnabled(true);
    refreshSegments();
}
void MainWindow::setDest()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Destination Directory"), QDir::homePath(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    dst = QDir(dir);
    ui->vidDest->setText(dir);
}
void MainWindow::segmentSet(int val)
{
    ui->segments->clear();
    ui->progressBar->setValue(0);

    QString sname = src.completeBaseName();
    if(ui->customName->text() != "")
    {
        sname = ui->customName->text();
    }

    int ssize = (ui->prtHrs->value()*3600) + (ui->prtMins->value()*60) + (ui->prtSecs->value());
    int scount = qCeil(vidlen/(double)ssize);
    int ss = 0;
    ui->progressBar->setMaximum(scount*PROGRESS_WIDTH_PER_SEGMENT);
    for(int i=1;i<=scount;i++)
    {
        int slen = ssize;
        if(ss!=0)
        {
            ss -= crossfade/2;
            slen += crossfade/2;
        }

        QString sfin = (i == scount) ? secondsToTime(vidlen) : secondsToTime(ss+slen);
        QString sfname = sname+ "_" + QString::number(i) + ".mp4";
        QString spath = QDir(ui->vidDest->text()).filePath(sfname);

        bool isDone = false;
        QString state = "Queued";
        QString progress = "0%";
        if(QFileInfo(spath).exists())
        {
            QProcess prober(this);
            QStringList proberArgs;
            proberArgs << "-v" << "error" << "-show_entries" << "format=duration" << "-of" << "default=noprint_wrappers=1:nokey=1" << spath;
            prober.start("ffprobe", proberArgs);
            prober.waitForFinished();
            int t = int(prober.readAllStandardOutput().toDouble());
            if(slen == t)
            {
                if(ui->cbOverwrite_existing->isChecked())
                {
                    state = "Exists -> Overwriting";
                }
                else
                {
                    state = "Exists -> Skipping";
                    progress = "100%";
                    isDone = true;
                    ui->progressBar->setValue(ui->progressBar->value()+PROGRESS_WIDTH_PER_SEGMENT);
                }
            }
            else if(t == 0)
            {
                if(ui->cbOverwrite_corrupt->isChecked())
                {
                    state = "Corrupt -> Overwriting";
                }
                else
                {
                    state = "Corrupt -> Skipping";
                    progress = "100%";
                    isDone = true;
                    ui->progressBar->setValue(ui->progressBar->value()+PROGRESS_WIDTH_PER_SEGMENT);
                }
            }
            else
            {
                if(ui->cbOverwrite_diffsize->isChecked())
                {
                    state = "Size Mismatch -> Overwriting";
                }
                else
                {
                    state = "Size Mismatch -> Skipping";
                    progress = "100%";
                    isDone = true;
                    ui->progressBar->setValue(ui->progressBar->value()+PROGRESS_WIDTH_PER_SEGMENT);
                }
            }
        }

        QStringList columns;
        columns << QString::number(i) << secondsToTime(ss) << sfin << progress << sfname << state;
        QTreeWidgetItem* s = new QTreeWidgetItem(columns);
        s->setData(0, Qt::UserRole, isDone);
        s->setData(2, Qt::UserRole, slen);
        s->setData(4, Qt::UserRole, spath);
        ui->segments->addTopLevelItem(s);
        ss += slen;
    }

    if(sender() == ui->prtHrs)
        settings.setValue("lastHrs", val);
    else if(sender() == ui->prtMins)
        settings.setValue("lastMins", val);
    else if(sender() == ui->prtSecs)
        settings.setValue("lastSecs", val);
}
void MainWindow::disableInterface()
{
    ui->srcButton->setEnabled(false);
    ui->dstButton->setEnabled(false);
    ui->prtHrs->setEnabled(false);
    ui->prtMins->setEnabled(false);
    ui->prtSecs->setEnabled(false);
    ui->cbCrf->setEnabled(false);
    ui->crossfade->setEnabled(false);
    ui->customName->setEnabled(false);
}
void MainWindow::enableInterface()
{
    ui->srcButton->setEnabled(true);
    ui->dstButton->setEnabled(true);
    ui->prtHrs->setEnabled(true);
    ui->prtMins->setEnabled(true);
    ui->prtSecs->setEnabled(true);
    ui->cbCrf->setEnabled(true);
    ui->crossfade->setEnabled(true);
    ui->customName->setEnabled(true);
}
void MainWindow::startTranscode()
{
    if(ui->startButton->text() == "Start")
    {
        disableInterface();
        ui->startButton->setText("Pause");
        processQueue();
    }
    else if(ui->startButton->text() == "Pause")
    {
        if(ff->state() == QProcess::Running)
            kill(ff->pid(), SIGSTOP);
        ui->startButton->setText("Resume");
        setSegmentStyle("Paused");
        timerState = ffcounter.elapsed()/1000;
    }
    else if(ui->startButton->text() == "Resume")
    {
        if(ff->state() == QProcess::Running)
            kill(ff->pid(), SIGCONT);
        ui->startButton->setText("Pause");
        ffcounter.setHMS(0,0,timerState);
    }
}
void MainWindow::processQueue()
{
    if(ff->state() == QProcess::Running)
        ff->kill();

    if(currentSegment != nullptr)
    {
        currentSegment->setData(0, Qt::UserRole, true);
        setSegmentStyle("Done");
        ui->progressBar->setValue(currentSegment->text(0).toInt()*PROGRESS_WIDTH_PER_SEGMENT);
    }

    int count = ui->segments->topLevelItemCount();
    for (int i=0; i<count; i++)
    {
        if(!ui->segments->topLevelItem(i)->data(0, Qt::UserRole).value<bool>())
        {
            currentSegment = ui->segments->topLevelItem(i);
            ffcounter.start();
            setSegmentStyle("Processing");
            ffmake(ui->segments->topLevelItem(i));
            return;
        }
    }
    currentSegment = nullptr;
    finish();
}
void MainWindow::ffmake(QTreeWidgetItem* segment)
{
    QString spath = segment->data(4, Qt::UserRole).value<QString>();

    QStringList ffargs;
    ffargs << "-loglevel"   << "quiet";
    ffargs << "-stats";
    ffargs << "-y";
    ffargs << "-i"          << src.filePath();
    ffargs << "-c:v"        << "libx264";
    ffargs << "-crf"        << QString::number(crf);
    ffargs << "-vf"         << "format=yuv420p";
    ffargs << "-c:a"        << ui->cbAudioCodec->currentText().toLower();
    ffargs << "-ss"         << segment->text(1);
    ffargs << "-t"          << secondsToTime(segment->data(2, Qt::UserRole).value<int>());
    ffargs << spath;
    ff->setArguments(ffargs);
    ff->start();
}
void MainWindow::finish()
{
    ui->progressBar->setValue(ui->progressBar->maximum());
    ui->startButton->setText("Start");
}

void MainWindow::setCrossfade(int v)
{
    crossfade=v;
    settings.setValue("lastCrossfade", v);
    refreshSegments();
}
void MainWindow::setCRF(int i)
{
    crf = i;
    settings.setValue("lastCRF", i);
}
void MainWindow::closeEvent (QCloseEvent *event)
{
    if(ff->state() == QProcess::Running)
        if(QMessageBox::Yes == QMessageBox::question(this, tr("Quit?"), tr("Do you wish to abort the operation?")))
        {
            if(ff->state() == QProcess::Running)
                kill(ff->pid(), SIGINT);
            event->accept();
        }
        else
        {
            event->ignore();
        }
}

void MainWindow::processProgress()
{
    QString stderr = ff->readAllStandardError();
    QStringList tlist = stderr.split("=")[5].split(QRegExp(" +")).first().split(":");

    int hr = tlist[0].toInt();
    int mins = tlist[1].toInt();
    int secs = qFloor(tlist[2].toDouble());

    int secsDone = (hr*3600) + (mins*60) + secs;
    int totalSecs = currentSegment->data(2, Qt::UserRole).value<int>();

    int secsLeft = totalSecs - secsDone;
    int timeTaken = ffcounter.elapsed()/1000;
    double speedFactor = timeTaken/(double)secsDone;
    int eta = qRound(speedFactor*secsLeft);
    currentSegment->setText(6, (secsDone == 0) ? "∞" : secondsToTime(eta));

    int segmentProgress = qRound((secsDone/(double)totalSecs)*100);
    currentSegment->setText(3, QString::number(segmentProgress)+"%");

    int queueProgressDelta = qRound(PROGRESS_WIDTH_PER_SEGMENT*(segmentProgress/100.0));
    int queueProgressBase = (currentSegment->text(0).toInt()-1)*PROGRESS_WIDTH_PER_SEGMENT;

    ui->progressBar->setValue(queueProgressBase + queueProgressDelta);

    int segmentsLeft = ui->segments->topLevelItemCount() - currentSegment->text(0).toInt();
    int totalTimeRemaining = segmentsLeft*totalSecs + secsLeft;
    int totalEta = speedFactor * totalTimeRemaining;
    statusBar()->showMessage("Total Time Remaining: " + secondsToTime(totalEta));

    QString state = "Processing";
    for(int i=0; i<segmentProgress%4; i++)
    {
        state += ".";
    }
    setSegmentStyle(state);
}
void MainWindow::setSegmentStyle(QString state)
{
    currentSegment->setText(5, state);
    if(state == "Done")
    {
        currentSegment->setText(3, "100%");
        currentSegment->setText(6, "00:00");
    }
}


void MainWindow::refreshSegments()
{
    segmentSet(0);
}
