import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;
import java.util.Objects;

public class TimeComparator {
    private static String FILE_NAME_PATTERN = ".*/[^/\\d]*(\\d+[^/]*.txt)";
    private static String PATH_TO_TBB_EXECUTOR_RESULT_FILE = "./experiment.txt";
    private static String PATH_TO_GA_EXECUTOR_RESULT_FILE = "./experiment2_results.txt";

    public static void main(String[] args) throws IOException {
        for (int i = 0; i < args.length; i++) {
            String paramName = args[i];

            if (Objects.equals(paramName, "--tbb-results-file")) {
                i++;
                PATH_TO_TBB_EXECUTOR_RESULT_FILE = args[i];
            }

            if (Objects.equals(paramName, "--ga-results-file")) {
                i++;
                PATH_TO_GA_EXECUTOR_RESULT_FILE = args[i];
            }
        }

        if (PATH_TO_TBB_EXECUTOR_RESULT_FILE == null) {
            throw new RuntimeException("Missing tbb-results-file. See README.md for more information");
        }
        if (PATH_TO_GA_EXECUTOR_RESULT_FILE == null) {
            throw new RuntimeException("Missing ga-results-file. See README.md for more information");
        }

        Map<String, Double> tbbResult = getResultMap(PATH_TO_TBB_EXECUTOR_RESULT_FILE);
        Map<String, Double> gaResult = getResultMap(PATH_TO_GA_EXECUTOR_RESULT_FILE);

        System.out.println("filename\tavg ga time\tavg tbb time");
        gaResult.forEach((fileName, gaTime) -> {
            System.out.printf("%s\t%.2f\t%.2f\n", fileName, gaTime, tbbResult.getOrDefault(fileName, -1.0));
        });
    }

    private static Map<String, Double> getResultMap(String pathToResultFile) throws IOException {
        Map<String, Double> resultMap = new HashMap<>();
        try (BufferedReader br = new BufferedReader(new FileReader(pathToResultFile))) {
            String strLine;
            while ((strLine = br.readLine()) != null) {
                String fileName = strLine.replaceAll(FILE_NAME_PATTERN, "$1");
                String line = br.readLine();
                String[] times = line.split(",");
                times[0] = "0.0";
                Double avgTime = Arrays.stream(times).map(Double::parseDouble).reduce(Double::sum).orElse(0.0) / (times.length - 1);
                resultMap.put(fileName, avgTime);
                br.readLine();
            }
        }
        return resultMap;
    }
}
