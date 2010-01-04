#ifndef __BOB_LOADER_H__
#define __BOB_LOADER_H__

#include <QWidget>
#include "ui_BobLoader.h"

#include <QFile>

class BobLoader : public QWidget, private Ui::BobLoader
{
Q_OBJECT

public:
  BobLoader(QWidget *parent = 0);
  ~BobLoader();
  
public slots:
  void on_ui_RefreshButton_clicked(bool checked = false);
  void on_ui_DownloadButton_clicked(bool checked = false);
  
private:
  QString m_firmwareVersion;
  QString m_failMessage;
  
  QStringList getAvailablePorts();
  bool downloadFirmware(QString port);
};

#endif
