#include <algorithm>
#include <array>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <vector>

using namespace std;

struct Task {
	int id;
	double arrivalTime;
	double processingTime;
};

int main() {
	constexpr double modelTime = 10000000.0;
	constexpr int bucketCount = 11;
	constexpr double lambda = 0.15;
	constexpr double mu = 0.30;

	random_device rd;
	mt19937 generator(rd());
	exponential_distribution<double> arrivalInterval(lambda);
	exponential_distribution<double> processingTime(mu);

	vector<Task> waitingTasks;
	array<int, bucketCount> waitingTimeBuckets{};

	double nextArrival = arrivalInterval(generator);
	double completionTime = 0.0;
	double totalWaitingTime = 0.0;
	double maximumWaitingTime = 0.0;
	int arrivedTasks = 0;
	int completedTasks = 0;
	int nextTaskId = 1;
	bool serverBusy = false;

	cout << fixed << setprecision(3);


	auto startTask = [&](const Task& task, double time) {
		double waitingTime = time - task.arrivalTime;
		int bucket = min(bucketCount - 1, static_cast<int>(waitingTime));
		waitingTimeBuckets[bucket]++;
		totalWaitingTime += waitingTime;
		maximumWaitingTime = max(maximumWaitingTime, waitingTime);
		completionTime = time + task.processingTime;
		serverBusy = true;
		
	};

	while (nextArrival <= modelTime || serverBusy) {
		bool arrivalEvent = !serverBusy || nextArrival <= completionTime;
		double systemTime;

		if (arrivalEvent && nextArrival <= modelTime) {
			systemTime = nextArrival;
			Task task{nextTaskId++, systemTime, processingTime(generator)};
			arrivedTasks++;

			if (serverBusy) {
				waitingTasks.push_back(task);
			} else {
				startTask(task, systemTime);
			}

			nextArrival = systemTime + arrivalInterval(generator);
		} else {
			systemTime = completionTime;
			completedTasks++;

			if (waitingTasks.empty()) {
				serverBusy = false;
			} else {
				uniform_int_distribution<size_t> index(0, waitingTasks.size() - 1);
				size_t selected = index(generator);
				Task task = waitingTasks[selected];
				waitingTasks[selected] = waitingTasks.back();
				waitingTasks.pop_back();
				startTask(task, systemTime);
			}
		}

		
	}

	cout << "Обработано задач: " << completedTasks << '\n';
	cout << "Поступило задач: " << arrivedTasks << '\n';
	cout << "Интенсивность поступления lambda = " << lambda << '\n';
	cout << "Интенсивность обслуживания mu = " << mu << '\n';
	cout << "Среднее время ожидания: " << totalWaitingTime / arrivedTasks << " сек.\n";
	cout << "Максимальное время ожидания: " << maximumWaitingTime << " сек.\n\n";
	cout << "Распределение и функция распределения времени ожидания:\n";
	cout << "Интервал | Число задач | Вероятность | F(x)\n";
	double cumulativeProbability = 0.0;

	for (int bucket = 0; bucket < bucketCount - 1; bucket++) {
		double probability = static_cast<double>(waitingTimeBuckets[bucket]) / arrivedTasks;
		cumulativeProbability += probability;
		cout << bucket << "-" << bucket + 1 << " сек. | "
			 << waitingTimeBuckets[bucket] << " | "
			 << probability << " | " << cumulativeProbability << '\n';
	}

	double probability = static_cast<double>(waitingTimeBuckets.back()) / arrivedTasks;
	cumulativeProbability += probability;
	cout << "10+ сек. | " << waitingTimeBuckets.back() << " | "
		 << probability << " | " << cumulativeProbability << '\n';

	

	return 0;
}
