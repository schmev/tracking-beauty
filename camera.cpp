/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "camera.h"
#include "ui_camera.h"
#include "videosettings.h"
#include "imagesettings.h"

#include <QMediaService>
#include <QMediaRecorder>
#include <QCameraViewfinder>
#include <QMediaMetaData>

#include <QMessageBox>
#include <QPalette>

#include <QtWidgets>


#include "graphrepainter.h"

#define NUM_SAMPLES 500

#if (defined(Q_WS_MAEMO_6)) && QT_VERSION >= 0x040700
#define HAVE_CAMERA_BUTTONS
#endif

Camera::Camera(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::Camera),
    camera(0),
    imageCapture(0),
    mediaRecorder(0),
    isCapturingImage(false),
    applicationExiting(false),
    muse_data( NUM_SAMPLES )
{
    ui->setupUi(this);

    //Camera devices:
    QByteArray cameraDevice;

    QActionGroup *videoDevicesGroup = new QActionGroup(this);
    videoDevicesGroup->setExclusive(true);
    foreach(const QByteArray &deviceName, QCamera::availableDevices()) {
        QString description = camera->deviceDescription(deviceName);
        QAction *videoDeviceAction = new QAction(description, videoDevicesGroup);
        videoDeviceAction->setCheckable(true);
        videoDeviceAction->setData(QVariant(deviceName));
        if (cameraDevice.isEmpty()) {
            cameraDevice = deviceName;
            videoDeviceAction->setChecked(true);
        }
        ui->menuDevices->addAction(videoDeviceAction);
    }

    connect(videoDevicesGroup, SIGNAL(triggered(QAction*)), SLOT(updateCameraDevice(QAction*)));
    //connect(ui->captureWidget, SIGNAL(currentChanged(int)), SLOT(updateCaptureMode()));

#ifdef HAVE_CAMERA_BUTTONS
    ui->lockButton->hide();
#endif

    setCamera(cameraDevice);

    socket = new UdpListeningReceiveSocket ( IpEndpointName( IpEndpointName::ANY_ADDRESS, 5000 ), &listener );

    //socket->RunUntilSigInt();
    //socket->

    QTimer *timer = new QTimer( this );
    connect( timer, SIGNAL( timeout() ), this, SLOT( onTimer() ) );
    timer->start( 0 );

    cur_ring_idx = 0;

    //connect( ui->graphView, SIGNAL(), this, SLOT( onGraphRepaint( QPaintEvent* ) ) );
    GraphRepainter *graph_repainter = new GraphRepainter( ui->graphView, this );
    ui->graphView->installEventFilter( graph_repainter );
}

Camera::~Camera()
{
    delete mediaRecorder;
    delete imageCapture;
    delete camera;
}

void Camera::onGraphRepaint( QPaintEvent *paintEvent )
{
    QPainter p(ui->graphView);
    p.setPen(Qt::green);

    //p.drawImage(0, 0, *img);
    p.drawText(10,10,"hello");

    qDebug("**********onGraphRepaint***********");
}

void Camera::onTimer()
{
    //qDebug("Camera::onTimer()");
    size_t buf_size = 10000;
    char buf[buf_size];

    for(int i = 0; i < 1; i++)
    {
        IpEndpointName end_pt_name( IpEndpointName::ANY_ADDRESS, 5000 );
        size_t bytes_received = socket->ReceiveFrom( end_pt_name , buf, buf_size );

        //osc::ReceivedPacket( buf, bytes_received );

//        osc::ReceivedPacket p( buf, bytes_received );
//        if( !p.IsBundle() )
//        {
//            qDebug("bytes received: %u\n", bytes_received );
//            qDebug( buf );
//        }

        osc::ReceivedPacket p( buf, bytes_received );
        if( p.IsBundle() )
        {
            //ProcessBundle( ReceivedBundle(p), remoteEndpoint );
        }
        else
        {
            osc::ReceivedMessage msg(p);

            //if( std::strcmp( msg.AddressPattern(), "/muse/elements/alpha_session_score" ) == 0 )
            if( std::strcmp( msg.AddressPattern(), "/muse/eeg" ) == 0 )
            {
                float f1, f2, f3, f4;
                osc::ReceivedMessageArgumentStream args = msg.ArgumentStream();
                args >> f1 >> f2 >> f3 >> f4 >> osc::EndMessage;

                qDebug("alpha session score received: %f, %f, %f, %f", f1, f2, f3, f4);

                MuseDataStream mds;
                mds.eeg[0] = f1;
                mds.eeg[1] = f2;
                mds.eeg[2] = f3;
                mds.eeg[3] = f4;
                muse_data[cur_ring_idx] = mds;
                cur_ring_idx++;
                if( cur_ring_idx == NUM_SAMPLES ) cur_ring_idx = 0;
            }
            else if( std::strcmp( msg.AddressPattern(), "/muse/elements/jaw_clench" ) == 0 )
            {
                int i = 0;
                osc::ReceivedMessageArgumentStream args = msg.ArgumentStream();
                args >> i >> osc::EndMessage;
                if( i )
                {
                    qDebug("jaw clench detected.");

                    // FAKE IT FOR NOW:
                    takeImage();
                }
            }
        }
        //qDebug("bytes received: %u\n", bytes_received );
        //qDebug( buf );
    }

    ui->graphView->update();
}

void Camera::setCamera(const QByteArray &cameraDevice)
{
    delete imageCapture;
    delete mediaRecorder;
    delete camera;

    if (cameraDevice.isEmpty())
        camera = new QCamera;
    else
        camera = new QCamera(cameraDevice);

    connect(camera, SIGNAL(stateChanged(QCamera::State)), this, SLOT(updateCameraState(QCamera::State)));
    connect(camera, SIGNAL(error(QCamera::Error)), this, SLOT(displayCameraError()));

    mediaRecorder = new QMediaRecorder(camera);
    connect(mediaRecorder, SIGNAL(stateChanged(QMediaRecorder::State)), this, SLOT(updateRecorderState(QMediaRecorder::State)));

    imageCapture = new QCameraImageCapture(camera);

    connect(mediaRecorder, SIGNAL(durationChanged(qint64)), this, SLOT(updateRecordTime()));
    connect(mediaRecorder, SIGNAL(error(QMediaRecorder::Error)), this, SLOT(displayRecorderError()));

    mediaRecorder->setMetaData(QMediaMetaData::Title, QVariant(QLatin1String("Test Title")));

    //connect(ui->exposureCompensation, SIGNAL(valueChanged(int)), SLOT(setExposureCompensation(int)));

    camera->setViewfinder(ui->viewfinder);

    updateCameraState(camera->state());
    updateLockStatus(camera->lockStatus(), QCamera::UserRequest);
    updateRecorderState(mediaRecorder->state());

    connect(imageCapture, SIGNAL(readyForCaptureChanged(bool)), this, SLOT(readyForCapture(bool)));
    connect(imageCapture, SIGNAL(imageCaptured(int,QImage)), this, SLOT(processCapturedImage(int,QImage)));
    connect(imageCapture, SIGNAL(imageSaved(int,QString)), this, SLOT(imageSaved(int,QString)));
    connect(imageCapture, SIGNAL(error(int,QCameraImageCapture::Error,QString)), this,
            SLOT(displayCaptureError(int,QCameraImageCapture::Error,QString)));

    connect(camera, SIGNAL(lockStatusChanged(QCamera::LockStatus,QCamera::LockChangeReason)),
            this, SLOT(updateLockStatus(QCamera::LockStatus,QCamera::LockChangeReason)));

    //ui->captureWidget->setTabEnabled(0, (camera->isCaptureModeSupported(QCamera::CaptureStillImage)));
    //ui->captureWidget->setTabEnabled(1, (camera->isCaptureModeSupported(QCamera::CaptureVideo)));

    updateCaptureMode();
    camera->start();
}

void Camera::keyPressEvent(QKeyEvent * event)
{
    //if (event->isAutoRepeat())
    //    return;

    switch (event->key()) {
    case Qt::Key_CameraFocus:
        displayViewfinder();
        camera->searchAndLock();
        event->accept();
        break;
    //case Qt::Key_Camera:
    case Qt::Key_C:
        if (camera->captureMode() == QCamera::CaptureStillImage) {
            takeImage();
        } else {
            if (mediaRecorder->state() == QMediaRecorder::RecordingState)
                stop();
            else
                record();
        }
        event->accept();
        break;
    default:
        QMainWindow::keyPressEvent(event);
    }
}

void Camera::keyReleaseEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat())
        return;

    switch (event->key()) {
    case Qt::Key_CameraFocus:
        camera->unlock();
        break;
    default:
        QMainWindow::keyReleaseEvent(event);
    }
}

void Camera::updateRecordTime()
{
    QString str = QString("Recorded %1 sec").arg(mediaRecorder->duration()/1000);
    ui->statusbar->showMessage(str);
}


void Camera::processCapturedImage(int requestId, const QImage& img)
{ 
    Q_UNUSED(requestId);
    QImage scaledImage = img.scaled(ui->lastImagePreviewLabel->size(),
                                    Qt::KeepAspectRatio,
                                    Qt::SmoothTransformation);

    ui->lastImagePreviewLabel->setPixmap(QPixmap::fromImage(scaledImage));
    //ui->lastImagePreviewLabel->setPixmap(QPixmap::fromImage(img));

    // Display captured image for 4 seconds.
    displayCapturedImage();
    QTimer::singleShot(4000, this, SLOT(displayViewfinder()));

}

//void Camera::processCapturedImage(int requestId, const QImage& img)
//{
//    Q_UNUSED(requestId);
////    QImage scaledImage = img.scaled(ui->viewfinder->size(),
////                                    Qt::KeepAspectRatio,
////                                    Qt::SmoothTransformation);

//    //ui->lastImagePreviewLabel->setPixmap(QPixmap::fromImage(scaledImage));
//    ui->lastImagePreviewLabel->setPixmap(QPixmap::fromImage(img));

//    // Display captured image for 4 seconds.
//    displayCapturedImage();
//    QTimer::singleShot(4000, this, SLOT(displayViewfinder()));

//}

void Camera::configureCaptureSettings()
{
    switch (camera->captureMode()) {
    case QCamera::CaptureStillImage:
        configureImageSettings();
        break;
    case QCamera::CaptureVideo:
        configureVideoSettings();
        break;
    default:
        break;
    }
}

void Camera::configureVideoSettings()
{
    VideoSettings settingsDialog(mediaRecorder);

    settingsDialog.setAudioSettings(audioSettings);
    settingsDialog.setVideoSettings(videoSettings);
    settingsDialog.setFormat(videoContainerFormat);

    if (settingsDialog.exec()) {
        audioSettings = settingsDialog.audioSettings();
        videoSettings = settingsDialog.videoSettings();
        videoContainerFormat = settingsDialog.format();

        mediaRecorder->setEncodingSettings(
                    audioSettings,
                    videoSettings,
                    videoContainerFormat);
    }
}

void Camera::configureImageSettings()
{
    ImageSettings settingsDialog(imageCapture);

    settingsDialog.setImageSettings(imageSettings);

    if (settingsDialog.exec()) {
        imageSettings = settingsDialog.imageSettings();
        imageCapture->setEncodingSettings(imageSettings);
    }
}

void Camera::record()
{
    mediaRecorder->record();
    updateRecordTime();
}

void Camera::pause()
{
    mediaRecorder->pause();
}

void Camera::stop()
{
    mediaRecorder->stop();
}

void Camera::setMuted(bool muted)
{
    mediaRecorder->setMuted(muted);
}

void Camera::toggleLock()
{
    switch (camera->lockStatus()) {
    case QCamera::Searching:
    case QCamera::Locked:
        camera->unlock();
        break;
    case QCamera::Unlocked:
        camera->searchAndLock();
    }
}

void Camera::updateLockStatus(QCamera::LockStatus status, QCamera::LockChangeReason reason)
{
    QColor indicationColor = Qt::black;

    switch (status) {
    case QCamera::Searching:
        indicationColor = Qt::yellow;
        ui->statusbar->showMessage(tr("Focusing..."));
        //ui->lockButton->setText(tr("Focusing..."));
        break;
    case QCamera::Locked:
        indicationColor = Qt::darkGreen;
        //ui->lockButton->setText(tr("Unlock"));
        ui->statusbar->showMessage(tr("Focused"), 2000);
        break;
    case QCamera::Unlocked:
        indicationColor = reason == QCamera::LockFailed ? Qt::red : Qt::black;
        //ui->lockButton->setText(tr("Focus"));
        if (reason == QCamera::LockFailed)
            ui->statusbar->showMessage(tr("Focus Failed"), 2000);
    }

    //QPalette palette = ui->lockButton->palette();
    //palette.setColor(QPalette::ButtonText, indicationColor);
    //ui->lockButton->setPalette(palette);
}

void Camera::takeImage()
{
    isCapturingImage = true;
    imageCapture->capture();
}

void Camera::displayCaptureError(int id, const QCameraImageCapture::Error error, const QString &errorString)
{
    Q_UNUSED(id);
    Q_UNUSED(error);
    QMessageBox::warning(this, tr("Image Capture Error"), errorString);
    isCapturingImage = false;
}

void Camera::startCamera()
{
    camera->start();
}

void Camera::stopCamera()
{
    camera->stop();
}

void Camera::updateCaptureMode()
{
    //int tabIndex = ui->captureWidget->currentIndex();
    //QCamera::CaptureModes captureMode = tabIndex == 0 ? QCamera::CaptureStillImage : QCamera::CaptureVideo;

    //if (camera->isCaptureModeSupported(captureMode))
    //    camera->setCaptureMode(captureMode);
}

void Camera::updateCameraState(QCamera::State state)
{
    switch (state) {
    case QCamera::ActiveState:
        ui->actionStartCamera->setEnabled(false);
        ui->actionStopCamera->setEnabled(true);
        //ui->captureWidget->setEnabled(true);
        ui->actionSettings->setEnabled(true);
        break;
    case QCamera::UnloadedState:
    case QCamera::LoadedState:
        ui->actionStartCamera->setEnabled(true);
        ui->actionStopCamera->setEnabled(false);
        //ui->captureWidget->setEnabled(false);
        ui->actionSettings->setEnabled(false);
    }
}

void Camera::updateRecorderState(QMediaRecorder::State state)
{
    switch (state) {
    case QMediaRecorder::StoppedState:
        //ui->recordButton->setEnabled(true);
        //ui->pauseButton->setEnabled(true);
        //ui->stopButton->setEnabled(false);
        break;
    case QMediaRecorder::PausedState:
        //ui->recordButton->setEnabled(true);
        //ui->pauseButton->setEnabled(false);
        //ui->stopButton->setEnabled(true);
        break;
    case QMediaRecorder::RecordingState:
        //ui->recordButton->setEnabled(false);
        //ui->pauseButton->setEnabled(true);
        //ui->stopButton->setEnabled(true);
        break;
    }
}

void Camera::setExposureCompensation(int index)
{
    camera->exposure()->setExposureCompensation(index*0.5);
}

void Camera::displayRecorderError()
{
    QMessageBox::warning(this, tr("Capture error"), mediaRecorder->errorString());
}

void Camera::displayCameraError()
{
    QMessageBox::warning(this, tr("Camera error"), camera->errorString());
}

void Camera::updateCameraDevice(QAction *action)
{
    setCamera(action->data().toByteArray());
}

void Camera::displayViewfinder()
{
    //ui->stackedWidget->setCurrentIndex(0);
}

void Camera::displayCapturedImage()
{
    //ui->stackedWidget->setCurrentIndex(1);
}

void Camera::readyForCapture(bool ready)
{
    //ui->takeImageButton->setEnabled(ready);
}

void Camera::imageSaved(int id, const QString &fileName)
{
    Q_UNUSED(id);
    Q_UNUSED(fileName);

    isCapturingImage = false;
    if (applicationExiting)
        close();
}

void Camera::closeEvent(QCloseEvent *event)
{
    if (isCapturingImage) {
        setEnabled(false);
        applicationExiting = true;
        event->ignore();
    } else {
        event->accept();
    }
}
