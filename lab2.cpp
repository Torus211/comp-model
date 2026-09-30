#include <algorithm>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>

using namespace std;

int main() {

    mt19937 mt(42);

    
    uniform_real_distribution<> tau(5.0, 7.0);

   
    uniform_real_distribution<> sigma(4.0, 10.0);

    int K = 0;       
    int TYP = 1;     
    int L = 0;       
    int N = 1000000; 

    int inCount = 0;
    int outCount = 0;
    int maxQueue = 0;
    double ML = 0.0;
    double systemtime = 0.0;
    double T1;
    double T2;

    const double INF = numeric_limits<double>::infinity();

    T1 = tau(mt);

    
    T2 = INF;

    cout << fixed << setprecision(6);

    
    while (inCount < N || K == 1 || L > 0) {
        systemtime = min(T1, T2);

        if (T1 <= T2) {
            TYP = 1;
        } else {
            TYP = 2;
        }

        if (TYP == 1) {
           
            inCount++;
            ML += L;

            if (K == 0) {
               
                K = 1;
                T2 = systemtime + sigma(mt);
            } else {
               
                L++;

                if (L > maxQueue) {
                    maxQueue = L;
                }
            }

            // Планируем следующий приход
            if (inCount < N) {
                double arrivalInterval = tau(mt);
                T1 = systemtime + arrivalInterval;
            } else {
                // Больше задач не будет
                T1 = INF;
            }
        } else {
            // Сервер закончил обработку
            outCount++;

            if (L > 0) {
                // Берём следующую задачу из очереди
                L--;

                double processingTime = sigma(mt);
                T2 = systemtime + processingTime;
            } else {
                // Очередь пуста
                K = 0;
                T2 = INF;
            }
        }

                cout << "Время: " << systemtime
                         << " | " << (TYP == 1 ? "IN" : "OUT")
                         << " | K = " << K
                         << " | очередь L = " << L << endl;
    }

    

    return 0;
}