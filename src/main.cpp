#include <QApplication>
#include "BobLoader.h"

int main(int argc, char **argv)
{
  QApplication app(argc, argv);
  BobLoader loader;
  
  loader.show();
  
  return app.exec();
}
