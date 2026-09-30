#include <functional>
#include <iostream>
#include <map>
#include <random>

using namespace std;

void function1() {
    cout << "выполнена функция 1" << endl;
}

void function2() {
    cout << "выполнена функция 2" << endl;
}

void function3() {
    cout << "выполнена функция 3" << endl;
}

void function4() {
    cout << "выполнена функция 4" << endl;
}

int main() {
    map<double, int> events;
    random_device rd;
    mt19937 gen(rd());
    uniform_real_distribution<double> timeDistribution(0.0, 100.0);
    uniform_int_distribution<int> functionDistribution(1, 4);

    while (events.size() < 100) {
        double time = timeDistribution(gen);
        int functionType = functionDistribution(gen);
        events.emplace(time, functionType);
    }

    const function<void()> functions[] = {
        function1,
        function2,
        function3,
        function4,
    };

    for (const auto& [time, functionType] : events) {
        cout << "Время: " << time << " -> ";
        functions[functionType - 1]();
    }

    return 0;
}
