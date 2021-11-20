import java.io.BufferedReader;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;

public class DisperisonComparator {
    private static String FILE_NAME_PATTERN = ".*/[^/\\d]*(\\d+[^/]*.txt)";
    private static String PATH_TO_TBB_EXECUTOR_RESULT_FILE;
    private static String PATH_TO_GA_EXECUTOR_RESULT_FILE;

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

        Map<String, String> tbbResult = getResultMap(PATH_TO_TBB_EXECUTOR_RESULT_FILE);
        Map<String, String> gaResult = getResultMap(PATH_TO_GA_EXECUTOR_RESULT_FILE);

        System.out.println("filename\tga dispersion\tga avg time\ttbb dispersion\ttbb avg time");
        gaResult.forEach((key, value) -> {
            if (tbbResult.containsKey(key)) {
                System.out.printf("%s%s%s\n", key, value, tbbResult.get(key));
            }
        });

    }

    private static Map<String, String> getResultMap(String pathToResultFile) throws IOException {
        Map<String, String> resultMap = new HashMap<>();
        try (BufferedReader br = new BufferedReader(new FileReader(pathToResultFile))) {
            String strLine;
            while ((strLine = br.readLine()) != null) {
                String fileName = strLine.replaceAll(FILE_NAME_PATTERN, "$1");
                String line = br.readLine();
                String[] stringTimes = line.split(",");
                stringTimes[0] = "0.0";
                List<Double> doubleTimes = Arrays.stream(stringTimes).map(Double::parseDouble).collect(Collectors.toList());
                Double avgTime = doubleTimes.stream().reduce(Double::sum).orElse(0.0) / (stringTimes.length - 1);
                Double dispersion = 0.0;
                for (int i = 1; i < doubleTimes.size(); i++) {
                    dispersion += Math.pow(avgTime - doubleTimes.get(i), 2);
                }
                resultMap.put(fileName, String.format("\t%.2f\t%.2f", dispersion / (stringTimes.length - 1), avgTime));
                br.readLine();
            }
        }
        return resultMap;
    }
}
