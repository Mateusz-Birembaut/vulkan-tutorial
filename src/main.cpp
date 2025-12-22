#include <VulkanApp/VulkanApp.h>


#include <QApplication>

#include <cstdlib>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <vector>
#include <set>

int main(int argc, char *argv[]) {
	QApplication app(argc, argv);

	VulkanApp vulkanWindow;
    vulkanWindow.show(); 
    vulkanWindow.init(); 

    return app.exec();

}