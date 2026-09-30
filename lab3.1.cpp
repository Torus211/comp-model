#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numeric>

using namespace std;

int main() {
    const double T = 10000.0;
    const int bins = 10;

    int N;
    double a, b;

    cout << "Введите количество потоков N: ";
    cin >> N;
    cout << "Введите a: ";
    cin >> a;
    cout << "Введите b: ";
    cin >> b;

    if (N <= 0 || a >= b) {
        cout << "Ошибка: N должно быть больше 0, а b должно быть больше a.\n";
        return 1;
    }

    mt19937 generator(12345);
    uniform_real_distribution<double> randomInterval(a, b);

    cout << fixed << setprecision(6);
    cout << "a = " << a << ", b = " << b << ", T = " << T << "\n";
    cout << "\n";

    vector<double> events;

    for (int stream = 0; stream < N; ++stream) {
        double time = 0.0;

        while (time <= T) {
            time += randomInterval(generator);
            if (time <= T) {
                events.push_back(time);
            }
        }
    }

    sort(events.begin(), events.end());

    vector<double> intervals;
    for (size_t i = 1; i < events.size(); ++i) {
        intervals.push_back(events[i] - events[i - 1]);
    }

    double meanIntervalOneStream = (a + b) / 2.0;
    double lambda = N / meanIntervalOneStream;
    double maxInterval = 5.0 / lambda;
    double binWidth = maxInterval / bins;
    vector<int> counts(bins, 0);

    for (double interval : intervals) {
        if (interval < maxInterval) {
            int bin = static_cast<int>(interval / binWidth);
            counts[bin]++;
        }
    }

    cout << "Параметры: N = " << N << ", a = " << a
         << ", b = " << b << ", lambda = " << lambda << "\n";
    cout << "Плотность интервалов между соседними событиями\n";
    cout << "      Интервал            Число           Плотность эксп.  Плотность теор.\n";

    for (int i = 0; i < bins; ++i) {
        double left = i * binWidth;
        double right = (i + 1) * binWidth;
        double experimentalDensity = counts[i] / (intervals.size() * binWidth);
        double middle = (left + right) / 2.0;
        double theoreticalDensity = lambda * exp(-lambda * middle);

        cout << left << " - " << right << "    "
             << setw(8) << counts[i] << "    "
             << setw(14) << experimentalDensity << "    "
             << theoreticalDensity << "\n";
    }

    cout << "Средний интервал: "
         << (1.0 / lambda) << " теоретически, "
         << (intervals.empty() ? 0.0 : accumulate(intervals.begin(), intervals.end(), 0.0) / intervals.size())
         << " экспериментально\n";

    return 0;
}