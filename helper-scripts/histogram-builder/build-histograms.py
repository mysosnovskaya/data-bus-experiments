import os
import os.path
import scipy.stats
import matplotlib.pyplot as plt


def main() -> None:
    data_path = "./data"
    results_path = "./results"

    os.makedirs(results_path, exist_ok=True)

    for file_name in os.listdir(data_path):
        file_path = os.path.join(data_path, file_name)
        with open(file_path, mode="r") as file:
            while True:
                name = file.readline().strip()

                if not name:
                    break

                values = list(map(float, file.readline().split(",")))
                values = values[1:]

                for _ in range(6):
                    file.readline()

                stat, p = scipy.stats.shapiro(values)
                plt.hist(values, density=True, bins=10)
                hist_file_name = f"{p:.4f}_{file_name}__{name}_hist.png"
                hist_file_path = os.path.join(results_path, hist_file_name)
                plt.savefig(hist_file_path)
                plt.close()

                print(f"Processed: {name}.")


if __name__ == "__main__":
    main()
