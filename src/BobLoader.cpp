#include "BobLoader.h"
#include <QDir>
#include <QFile>
#include <QProgressDialog>
#include <QtEndian>
#include <QMessageBox>
#include <QFileDialog>

#include "crc32.h"

#include "QSerialPort.h"

#ifdef Q_OS_WIN32
#include <windows.h>
#endif

BobLoader::BobLoader(QWidget *parent) : QWidget(parent)
{
  setupUi(this);
  
  QFile firmwareVersion(":/firmware_version");
  if(firmwareVersion.open(QIODevice::ReadOnly))
    m_firmwareVersion = firmwareVersion.readAll().trimmed();
  
  ui_FirmwareVersion->setText(m_firmwareVersion);
  
  on_ui_RefreshButton_clicked();
}

BobLoader::~BobLoader()
{
}

void BobLoader::on_ui_RefreshButton_clicked(bool)
{
  ui_PortList->clear();
  ui_PortList->addItems(getAvailablePorts());
}

void BobLoader::on_ui_DownloadButton_clicked(bool)
{
  QString port = ui_PortList->currentText();
  
#ifndef Q_OS_WIN32
  port = "/dev/" + port;
#endif

  if(downloadFirmware(port))
    QMessageBox::information(this, "Firmware Download", "Download Succeeded!");
  else
    QMessageBox::warning(this, "Firmware Download", m_failMessage);
}

QStringList BobLoader::getAvailablePorts()
{
  QStringList portList;
#ifdef Q_OS_WIN32
  COMMTIMEOUTS cto;
  cto.ReadIntervalTimeout = MAXDWORD;
  cto.ReadTotalTimeoutMultiplier = 0;
  cto.ReadTotalTimeoutConstant = 0;
  cto.WriteTotalTimeoutMultiplier = 0;
  cto.WriteTotalTimeoutConstant = 0;
  
  for(int i = 1;i < 32;i++) {
    QString portName = "com" + QString::number(i);
    QString realPortName = "\\\\.\\" + portName;
    
    HANDLE handle = CreateFileA(realPortName.toLocal8Bit(), GENERIC_READ | GENERIC_WRITE,
                                0, NULL, OPEN_EXISTING, 0, 0);
    
    if(handle != INVALID_HANDLE_VALUE) {
      CloseHandle(handle);
      portList << portName;
    }
  }
#else
  portList = QDir("/dev").entryList(QStringList()<<"ttyUSB*"<<"ttyACM*"<<"tty.usbmodem*", QDir::System);
#endif

  return portList;
}

bool BobLoader::downloadFirmware(QString port)
{
  QFile firmware(":/firmware");
  QSerialPort serialPort(port);
  
  if(!serialPort.open(QIODevice::ReadWrite)) {
    m_failMessage = "Failed to open serial port.";
    return false;
  }
  
  if(!firmware.open(QIODevice::ReadOnly)) {
    firmware.setFileName(QFileDialog::getOpenFileName(this, "Select Firmware Image"));
    if(!firmware.open(QIODevice::ReadOnly)) {
      m_failMessage = "No firmware image specified";
      return false;
    }
  }
  
  QProgressDialog progress("Downloading Firmware...", "Abort Download", 0, firmware.size(), this);
  progress.setMinimumDuration(0);
  progress.setWindowModality(Qt::WindowModal);
  
  quint32 crc;
  QByteArray data;
  QByteArray ok = QByteArray("OK", 2);
  QByteArray ret;
  
  data = QByteArray("WR", 2) + firmware.read(256);
  while(data.size() > 2) {
    progress.setValue(progress.value() + data.size()-2);
    data.append(QByteArray(258-data.size(), 0));
    crc = qToLittleEndian<quint32>(::crc32((const unsigned char*)data.constData(), 258));
    //FUGLY HACK TO MAKE QT 4.4 WORK
    data.append(((const char *)&crc)[0]);
    data.append(((const char *)&crc)[1]);
    data.append(((const char *)&crc)[2]);
    data.append(((const char *)&crc)[3]);
    
    ret.clear();
    while(ret != ok) {
      serialPort.write(data);
      
      if(progress.wasCanceled()) {
        m_failMessage = "Download Canceled";
        return false;
      }
      
      while(ret.size() < 2) {
        if(progress.wasCanceled()) {
          m_failMessage = "Download Canceled";
          return false;
        }
        ret += serialPort.read(2-ret.size());
      }
    }
    
    data = QByteArray("WR", 2) + firmware.read(256);
  }
  
  serialPort.write(ok);
  ret = serialPort.read(2);
  
  return ret == QByteArray("DC", 2);
}
