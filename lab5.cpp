#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

namespace
{
    constexpr array<double, 2> sigma{11.0, 17.0};
    constexpr double infinity = numeric_limits<double>::infinity();

    struct Config
    {
        double a = 20, b = 40;
        size_t count = 100000, warmup = 10000, runs = 5;
        uint64_t seed = 42;
        int k = 0, m = 0; // 0 — случайно; положительное значение для всех заданий.
    };

    double number(const string &s, const string &name)
    {
        char *end = nullptr;
        errno = 0;
        double value = strtod(s.c_str(), &end);
        if (end == s.c_str() || *end || errno || !isfinite(value) || value <= 0 || value > 1e12)
            throw invalid_argument(name + ": требуется положительное число не больше 1e12");
        return value;
    }

    uint64_t integer(const string &s, const string &name, uint64_t maximum)
    {
        if (s.empty() || s.find_first_not_of("0123456789") != string::npos)
            throw invalid_argument(name + ": требуется целое неотрицательное число");
        char *end = nullptr;
        errno = 0;
        auto value = strtoull(s.c_str(), &end, 10);
        if (errno || *end || value > maximum)
            throw invalid_argument(name + ": значение слишком велико");
        return value;
    }

    string input(const string &prompt, const string &defaultValue)
    {
        cout << prompt << " [" << defaultValue << "]: " << flush;
        string value;
        if (!getline(cin, value))
            throw invalid_argument("не удалось прочитать ввод");
        const auto first = value.find_first_not_of(" \t\r");
        if (first == string::npos)
            return defaultValue;
        const auto last = value.find_last_not_of(" \t\r");
        value = value.substr(first, last - first + 1);
        return value;
    }

    Config readConfig(int argc, char **argv)
    {
        Config c;
        if (argc == 1)
        {
            c.a = number(input("a", "20"), "a");
            c.b = number(input("b", "40"), "b");
            c.k = static_cast<int>(integer(input("k (Enter или 0 — случайно; 1..3 — фиксированное)", "0"), "k", 3));
            c.m = static_cast<int>(integer(input("m (Enter или 0 — случайно; 1..2 — фиксированное)", "0"), "m", 2));
            c.count = integer(input("Число наблюдаемых заданий в одном опыте", "100000"), "N", 1000000);
            c.warmup = integer(input("Прогрев (число заданий)", "10000"), "прогрев", 1000000);
            c.runs = integer(input("Число независимых опытов", "5"), "число опытов", 100);
            c.seed = integer(input("Seed генераторов", "42"), "seed", numeric_limits<uint64_t>::max());
        }
        else
        {
            if (argc < 3 || argc > 9)
                throw invalid_argument("неверное число аргументов");
            c.a = number(argv[1], "a");
            c.b = number(argv[2], "b");
            if (argc > 3)
                c.count = integer(argv[3], "N", 1000000);
            if (argc > 4)
                c.seed = integer(argv[4], "seed", numeric_limits<uint64_t>::max());
            if (argc > 5)
                c.warmup = integer(argv[5], "прогрев", 1000000);
            if (argc > 6)
                c.runs = integer(argv[6], "число опытов", 100);
            if (argc > 7)
                c.k = static_cast<int>(integer(argv[7], "k", 3));
            if (argc > 8)
                c.m = static_cast<int>(integer(argv[8], "m", 2));
        }
        if (c.a >= c.b)
            throw invalid_argument("должно выполняться 0 < a < b");
        if (!c.count || !c.runs)
            throw invalid_argument("N и число опытов должны быть положительными");
        return c;
    }

    struct Task
    {
        size_t id;
        double arrival, queuedAt;
        int k, m;
        array<int, 2> remaining;
        array<double, 2> waiting{0, 0};
    };

    struct Station
    {
        deque<Task> queue;
        Task active{};
        double departure = infinity;
        size_t maxQueue = 0;
    };

    struct Result
    {
        double residence = 0, wait1 = 0, wait2 = 0, service = 0;
        double maxResidence = 0;
        array<size_t, 2> maxQueue{0, 0};
        size_t completed = 0;
    };

    Result simulate(const Config &c, uint64_t seed)
    {
        // Разные генераторы: выбор из очереди не изменяет входной поток и типы заданий.
        mt19937_64 arrivals(seed), types(seed ^ 0x9e3779b97f4a7c15ULL), choices(seed ^ 0xd1b54a32d192ed03ULL);
        uniform_real_distribution<double> interval(c.a, c.b);
        uniform_int_distribution<int> chooseK(1, 3), chooseM(1, 2);
        array<Station, 2> stations;
        size_t arrived = 0;
        double nextArrival = interval(arrivals);
        Result result;
        long double sumResidence = 0, sumWait1 = 0, sumWait2 = 0, sumService = 0;

        auto enqueue = [&](size_t phase, Task task, double time)
        {
            task.queuedAt = time;
            stations[phase].queue.push_back(task);
        };
        auto start = [&](size_t phase, double time)
        {
            auto &s = stations[phase];
            if (s.departure != infinity || s.queue.empty())
                return;
            uniform_int_distribution<size_t> pick(0, s.queue.size() - 1);
            size_t index = pick(choices);
            s.active = s.queue[index];
            s.queue[index] = s.queue.back();
            s.queue.pop_back();
            s.active.waiting[phase] += time - s.active.queuedAt;
            s.departure = time + sigma[phase];
            if (!isfinite(s.departure) || s.departure <= time)
                throw runtime_error("потеря точности модельного времени");
        };

        // Наблюдаем фиксированную когорту после прогрева. Фоновый поток продолжается
        // до выхода ВСЕЙ когорты: остановка поступлений занизила бы её ожидание.
        while (result.completed < c.count)
        {
            double time = min({nextArrival, stations[0].departure, stations[1].departure});
            // При совпадении времён сначала завершения обеих фаз, затем поступление,
            // затем запуск обслуживания. Переводы и возвраты добавляются в очередь.
            for (size_t phase = 0; phase < 2; ++phase)
            {
                auto &s = stations[phase];
                if (s.departure != time)
                    continue;
                Task task = s.active;
                s.departure = infinity;
                --task.remaining[phase];
                if (task.remaining[phase] > 0)
                {
                    enqueue(phase, task, time);
                }
                else if (phase == 0)
                {
                    enqueue(1, task, time);
                }
                else if (task.id >= c.warmup && task.id < c.warmup + c.count)
                {
                    double residence = time - task.arrival;
                    double service = sigma[0] * task.k + sigma[1] * task.m;
                    double reconstructed = service + task.waiting[0] + task.waiting[1];
                    if (abs(residence - reconstructed) > 1e-7 * max(1.0, residence))
                        throw runtime_error("нарушен баланс времени задания");
                    sumResidence += residence;
                    sumWait1 += task.waiting[0];
                    sumWait2 += task.waiting[1];
                    sumService += service;
                    result.maxResidence = max(result.maxResidence, residence);
                    ++result.completed;
                }
            }
            if (nextArrival == time)
            {
                int k = c.k ? c.k : chooseK(types);
                int m = c.m ? c.m : chooseM(types);
                enqueue(0, {arrived++, time, time, k, m, {k, m}, {0, 0}}, time);
                nextArrival = time + interval(arrivals);
                if (!isfinite(nextArrival) || nextArrival <= time)
                    throw runtime_error("потеря точности времени поступления");
            }
            for (size_t phase = 0; phase < 2; ++phase)
            {
                start(phase, time);
                auto &s = stations[phase];
                s.maxQueue = max(s.maxQueue, s.queue.size());
            }
        }
        result.residence = static_cast<double>(sumResidence / c.count);
        result.wait1 = static_cast<double>(sumWait1 / c.count);
        result.wait2 = static_cast<double>(sumWait2 / c.count);
        result.service = static_cast<double>(sumService / c.count);
        result.maxQueue = {stations[0].maxQueue, stations[1].maxQueue};
        return result;
    }
} // namespace

int main(int argc, char **argv)
{
    try
    {
        Config c = readConfig(argc, argv);
        double meanK = c.k ? c.k : 2.0;
        double meanM = c.m ? c.m : 1.5;
        double meanInterval = c.a / 2 + c.b / 2;
        double load1 = sigma[0] * meanK / meanInterval;
        double load2 = sigma[1] * meanM / meanInterval;
        double boundary = 2 * max(sigma[0] * meanK, sigma[1] * meanM);
        cout << fixed << setprecision(6);
        cout << "a = " << c.a << ", b = " << c.b << ", sigma1 = 11, sigma2 = 17\n"
             << "k = " << (c.k ? to_string(c.k) + " (фиксировано)" : "U{1,2,3}")
             << ", m = " << (c.m ? to_string(c.m) + " (фиксировано)" : "U{1,2}") << '\n'
             << "N = " << c.count << ", прогрев = " << c.warmup
             << ", опытов = " << c.runs << ", seed = " << c.seed << '\n'
             << "E[tau] = " << meanInterval << ", lambda = " << 1 / meanInterval << '\n'
             << "rho1 = " << load1 << ", rho2 = " << load2 << '\n'
             << "Условие устойчивости: a + b > " << boundary << '\n';
        if (load1 >= 1 || load2 >= 1)
        {
            cout << (load1 > 1 || load2 > 1 ? "Система перегружена\n" : "Критический режим (rho = 1)\n")
                 << "Конечное стац ср время пребывания не определяется\n";
            return 2;
        }
        cout << "Система устойчива.\n\n"
             << "Опыт\tT\t\tW1\t\tW2\t\tS\t\tmax T\t\tmax Q1\t\tmax Q2\n";
        vector<double> means;
        double wait1 = 0, wait2 = 0, service = 0;
        for (size_t run = 0; run < c.runs; ++run)
        {
            Result r = simulate(c, c.seed + run * 0x9e3779b97f4a7c15ULL);
            means.push_back(r.residence);
            wait1 += r.wait1;
            wait2 += r.wait2;
            service += r.service;
            cout << run + 1 << '\t' << r.residence << '\t' << r.wait1 << '\t'
                 << r.wait2 << '\t' << r.service << '\t' << r.maxResidence << '\t'
                 << r.maxQueue[0] << '\t' << r.maxQueue[1] << '\n';
        }
        double mean = accumulate(means.begin(), means.end(), 0.0) / c.runs;
        cout << "\nСреднее время пребывания T = " << mean << '\n'
             << "Среднее ожидание W1 = " << wait1 / c.runs << '\n'
             << "Среднее ожидание W2 = " << wait2 / c.runs << '\n'
             << "Среднее время обслуживания S = " << service / c.runs << '\n'
             << "Теоретическое E[S] = " << sigma[0] * meanK + sigma[1] * meanM << '\n';
        if (c.runs > 1)
        {
            double squared = 0;
            for (double value : means)
                squared += (value - mean) * (value - mean);
            cout << "Стандартная ошибка по независимым опытам = "
                 << sqrt(squared / (c.runs - 1) / c.runs) << '\n';
        }
    }
    catch (const exception &error)
    {
        cerr << "Ошибка: " << error.what() << '\n'
             << "Использование: " << argv[0]
             << " a b [N] [seed] [прогрев] [опытов] [k:0..3] [m:0..2]\n";
        return 1;
    }
    return 0;
}
