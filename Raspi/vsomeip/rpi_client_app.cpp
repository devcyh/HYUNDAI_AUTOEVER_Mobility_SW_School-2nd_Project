#include <vsomeip/vsomeip.hpp>
#include <iomanip>
#include <iostream>
#include <thread>
#include <chrono>

#define SERVICE_ID_SENSOR   0x0100
#define METHOD_ID_TOF       0x0101
#define METHOD_ID_ULT       0x0102

#define SERVICE_ID_CONTROL  0x0200
#define METHOD_ID_MOTOR     0x0201
#define METHOD_ID_ALERT     0x0202

#define INSTANCE_ID         0x0001
#define CLIENT_ID           0x5678

std::shared_ptr<vsomeip::application> app;

// === 서비스 가용성 콜백 ===
void on_availability(vsomeip::service_t service,
                     vsomeip::instance_t instance,
                     bool is_available) {
    std::cout << "Service [0x"
              << std::setw(4) << std::setfill('0') << std::hex << service
              << ".0x" << std::setw(4) << instance
              << "] is " << (is_available ? "AVAILABLE" : "NOT available")
              << std::endl;

    if (is_available) {
        std::cout << "Requesting service 0x" << std::hex << service
                  << " instance 0x" << instance << "..." << std::endl;
        app->request_service(service, instance);
    }
}


int main() {
    app = vsomeip::runtime::get()->create_application("rpi_client_app");

    if (!app->init()) {
        std::cerr << "Couldn't initialize application!" << std::endl;
        return -1;
    }

    // === 가용성 콜백 등록 ===
    app->register_availability_handler(SERVICE_ID_SENSOR, INSTANCE_ID, on_availability);
    app->register_availability_handler(SERVICE_ID_CONTROL, INSTANCE_ID, on_availability);
    
    // 서비스 탐색
    app->request_service(SERVICE_ID_SENSOR, INSTANCE_ID);
    app->request_service(SERVICE_ID_CONTROL, INSTANCE_ID);

    // === vsomeip 시작 ===
    app->start();

    return 0;
}
