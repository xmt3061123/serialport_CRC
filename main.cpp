#include "serialassistant.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    SerialAssistant w;  // 注意这里使用大写的 SerialAssistant
    w.show();
    return a.exec();
}
