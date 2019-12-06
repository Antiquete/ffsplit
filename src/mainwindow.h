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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileDialog>
#include <QProcess>
#include <QtMath>
#include <QTime>
#include <QDebug>
#include <QSettings>
#include <QMessageBox>
#include <signal.h>
#include <QTreeWidgetItem>
#include <QCloseEvent>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    QFileInfo src;
    QDir dst;
    int vidlen;


    QSettings settings;
    QProcess* ff = new QProcess(this);

    int timestep;
    int crossfade;
    int crf;
    int start = 0;

    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

    QTreeWidgetItem* currentSegment = nullptr;
    void closeEvent (QCloseEvent *event);

    QTime ffcounter;
    int timerState;

private slots:
    void setSource();
    void setDest();
    void segmentSet(int);

    void startTranscode();
    void disableInterface();
    void enableInterface();

    void processQueue();
    void ffmake(QTreeWidgetItem*);
    void finish();

    void setCrossfade(int);
    void setCRF(int);

    void processProgress();
    void setSegmentStyle(QString);

    void refreshSegments();
};
#endif // MAINWINDOW_H
