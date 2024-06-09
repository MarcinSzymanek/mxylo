#include "m_xylo.hpp"

extern "C" void app_main(void){
    int old = 0;
    MXylo::instance().start();
    for(;;){
        vTaskDelay(10000);
    }
}